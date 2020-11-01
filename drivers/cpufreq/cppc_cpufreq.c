/*
 * CPPC (Collaborative Processor Performance Control) driver for
 * interfacing with the CPUfreq layer and governors. See
 * cppc_acpi.c for CPPC specific methods.
 *
 * (C) Copyright 2014, 2015 Linaro Ltd.
 * Author: Ashwin Chaugule <ashwin.chaugule@linaro.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define pr_fmt(fmt)	"CPPC Cpufreq:"	fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/dmi.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#include <asm/unaligned.h>

#include <acpi/cppc_acpi.h>

/* Minimum struct length needed for the DMI processor entry we want */
#define DMI_ENTRY_PROCESSOR_MIN_LENGTH	48

/* Offest in the DMI processor structure for the max frequency */
#define DMI_PROCESSOR_MAX_SPEED  0x14

/*
 * These structs contain information parsed from per CPU
 * ACPI _CPC structures.
 * e.g. For each CPU the highest, lowest supported
 * performance capabilities, desired performance level
 * requested etc.
 */
static struct cppc_cpudata **all_cpu_data;

/* Capture the max KHz from DMI */
static u64 cppc_dmi_max_khz;

/* Callback function used to retrieve the max frequency from DMI */
static void cppc_find_dmi_mhz(const struct dmi_header *dm, void *private)
{
	const u8 *dmi_data = (const u8 *)dm;
	u16 *mhz = (u16 *)private;

	if (dm->type == DMI_ENTRY_PROCESSOR &&
	    dm->length >= DMI_ENTRY_PROCESSOR_MIN_LENGTH) {
		u16 val = (u16)get_unaligned((const u16 *)
				(dmi_data + DMI_PROCESSOR_MAX_SPEED));
		*mhz = val > *mhz ? val : *mhz;
	}
}

/* Look up the max frequency in DMI */
static u64 cppc_get_dmi_max_khz(void)
{
	u16 mhz = 0;

	dmi_walk(cppc_find_dmi_mhz, &mhz);

	/*
	 * Real stupid fallback value, just in case there is no
	 * actual value set.
	 */
	mhz = mhz ? mhz : 1;

	return (1000 * mhz);
}

static int cppc_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int target_freq,
		unsigned int relation)
{
	struct cppc_cpudata *cpu;
	struct cpufreq_freqs freqs;
	u32 desired_perf;
	int ret = 0;

	cpu = all_cpu_data[policy->cpu];

	desired_perf = (u64)target_freq * cpu->perf_caps.highest_perf / cppc_dmi_max_khz;
	/* Return if it is exactly the same perf */
	if (desired_perf == cpu->perf_ctrls.desired_perf)
		return ret;

	cpu->perf_ctrls.desired_perf = desired_perf;
	freqs.old = policy->cur;
	freqs.new = target_freq;

	cpufreq_freq_transition_begin(policy, &freqs);
	ret = cppc_set_perf(cpu->cpu, &cpu->perf_ctrls);
	cpufreq_freq_transition_end(policy, &freqs, ret != 0);

	if (ret)
		pr_debug("Failed to set target on CPU:%d. ret:%d\n",
				cpu->cpu, ret);

	return ret;
}

static int cppc_verify_policy(struct cpufreq_policy *policy)
{
	cpufreq_verify_within_cpu_limits(policy);
	return 0;
}

static void cppc_cpufreq_stop_cpu(struct cpufreq_policy *policy)
{
	int cpu_num = policy->cpu;
	struct cppc_cpudata *cpu = all_cpu_data[cpu_num];
	int ret;

	cpu->perf_ctrls.desired_perf = cpu->perf_caps.lowest_perf;

	ret = cppc_set_perf(cpu_num, &cpu->perf_ctrls);
	if (ret)
		pr_debug("Err setting perf value:%d on CPU:%d. ret:%d\n",
				cpu->perf_caps.lowest_perf, cpu_num, ret);
}

/*
 * The PCC subspace describes the rate at which platform can accept commands
 * on the shared PCC channel (including READs which do not count towards freq
 * trasition requests), so ideally we need to use the PCC values as a fallback
 * if we don't have a platform specific transition_delay_us
 */
#ifdef CONFIG_ARM64
#include <asm/cputype.h>

static unsigned int cppc_cpufreq_get_transition_delay_us(int cpu)
{
	unsigned long implementor = read_cpuid_implementor();
	unsigned long part_num = read_cpuid_part_number();
	unsigned int delay_us = 0;

	switch (implementor) {
	case ARM_CPU_IMP_QCOM:
		switch (part_num) {
		case QCOM_CPU_PART_FALKOR_V1:
		case QCOM_CPU_PART_FALKOR:
			delay_us = 10000;
			break;
		default:
			delay_us = cppc_get_transition_latency(cpu) / NSEC_PER_USEC;
			break;
		}
		break;
	default:
		delay_us = cppc_get_transition_latency(cpu) / NSEC_PER_USEC;
		break;
	}

	return delay_us;
}

#else

static unsigned int cppc_cpufreq_get_transition_delay_us(int cpu)
{
	return cppc_get_transition_latency(cpu) / NSEC_PER_USEC;
}
#endif

static int cppc_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	struct cppc_cpudata *cpu;
	unsigned int cpu_num = policy->cpu;
	int ret = 0;

	cpu = all_cpu_data[policy->cpu];

	cpu->cpu = cpu_num;
	ret = cppc_get_perf_caps(policy->cpu, &cpu->perf_caps);

	if (ret) {
		pr_debug("Err reading CPU%d perf capabilities. ret:%d\n",
				cpu_num, ret);
		return ret;
	}

	cppc_dmi_max_khz = cppc_get_dmi_max_khz();

	/*
	 * Set min to lowest nonlinear perf to avoid any efficiency penalty (see
	 * Section 8.4.7.1.1.5 of ACPI 6.1 spec)
	 */
	policy->min = cpu->perf_caps.lowest_nonlinear_perf * cppc_dmi_max_khz /
		cpu->perf_caps.highest_perf;
	policy->max = cppc_dmi_max_khz;

	/*
	 * Set cpuinfo.min_freq to Lowest to make the full range of performance
	 * available if userspace wants to use any perf between lowest & lowest
	 * nonlinear perf
	 */
	policy->cpuinfo.min_freq = cpu->perf_caps.lowest_perf * cppc_dmi_max_khz /
		cpu->perf_caps.highest_perf;
	policy->cpuinfo.max_freq = cppc_dmi_max_khz;

	policy->cpuinfo.transition_latency = cppc_get_transition_latency(cpu_num);
	policy->transition_delay_us = cppc_cpufreq_get_transition_delay_us(cpu_num);
	policy->shared_type = cpu->shared_type;

	if (policy->shared_type == CPUFREQ_SHARED_TYPE_ANY) {
		int i;

		cpumask_copy(policy->cpus, cpu->shared_cpu_map);

		for_each_cpu(i, policy->cpus) {
			if (unlikely(i == policy->cpu))
				continue;

			memcpy(&all_cpu_data[i]->perf_caps, &cpu->perf_caps,
			       sizeof(cpu->perf_caps));
		}
	} else if (policy->shared_type == CPUFREQ_SHARED_TYPE_ALL) {
		/* Support only SW_ANY for now. */
		pr_debug("Unsupported CPU co-ord type\n");
		return -EFAULT;
	}

	cpu->cur_policy = policy;

	/* Set policy->cur to max now. The governors will adjust later. */
	policy->cur = cppc_dmi_max_khz;
	cpu->perf_ctrls.desired_perf = cpu->perf_caps.highest_perf;

	ret = cppc_set_perf(cpu_num, &cpu->perf_ctrls);
	if (ret)
		pr_debug("Err setting perf value:%d on CPU:%d. ret:%d\n",
				cpu->perf_caps.highest_perf, cpu_num, ret);

	return ret;
}

static struct cpufreq_driver cppc_cpufreq_driver = {
	.flags = CPUFREQ_CONST_LOOPS,
	.verify = cppc_verify_policy,
	.target = cppc_cpufreq_set_target,
	.init = cppc_cpufreq_cpu_init,
	.stop_cpu = cppc_cpufreq_stop_cpu,
	.name = "cppc_cpufreq",
};

static int __init cppc_cpufreq_init(void)
{
	int i, ret = 0;
	struct cppc_cpudata *cpu;

	if (acpi_disabled)
		return -ENODEV;

	all_cpu_data = kzalloc(sizeof(void *) * num_possible_cpus(), GFP_KERNEL);
	if (!all_cpu_data)
		return -ENOMEM;

	for_each_possible_cpu(i) {
		all_cpu_data[i] = kzalloc(sizeof(struct cppc_cpudata), GFP_KERNEL);
		if (!all_cpu_data[i])
			goto out;

		cpu = all_cpu_data[i];
		if (!zalloc_cpumask_var(&cpu->shared_cpu_map, GFP_KERNEL))
			goto out;
	}

	ret = acpi_get_psd_map(all_cpu_data);
	if (ret) {
		pr_debug("Error parsing PSD data. Aborting cpufreq registration.\n");
		goto out;
	}

	ret = cpufreq_register_driver(&cppc_cpufreq_driver);
	if (ret)
		goto out;

	return ret;

out:
	for_each_possible_cpu(i) {
		cpu = all_cpu_data[i];
		if (!cpu)
			break;
		free_cpumask_var(cpu->shared_cpu_map);
		kfree(cpu);
	}

	kfree(all_cpu_data);
	return -ENODEV;
}

static void __exit cppc_cpufreq_exit(void)
{
	struct cppc_cpudata *cpu;
	int i;

	cpufreq_unregister_driver(&cppc_cpufreq_driver);

	for_each_possible_cpu(i) {
		cpu = all_cpu_data[i];
		free_cpumask_var(cpu->shared_cpu_map);
		kfree(cpu);
	}

	kfree(all_cpu_data);
}

module_exit(cppc_cpufreq_exit);
MODULE_AUTHOR("Ashwin Chaugule");
MODULE_DESCRIPTION("CPUFreq driver based on the ACPI CPPC v5.0+ spec");
MODULE_LICENSE("GPL");

late_initcall(cppc_cpufreq_init);

static const struct acpi_device_id cppc_acpi_ids[] = {
	{ACPI_PROCESSOR_DEVICE_HID, },
	{}
};

MODULE_DEVICE_TABLE(acpi, cppc_acpi_ids);
