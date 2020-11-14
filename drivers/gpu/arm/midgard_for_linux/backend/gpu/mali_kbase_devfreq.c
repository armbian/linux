/*
 *
 * (C) COPYRIGHT 2014-2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */


#define ENABLE_DEBUG_LOG
#include "../../platform/rk/custom_log.h"


#include <mali_kbase.h>
#include <mali_kbase_tlstream.h>
#include <mali_kbase_config_defaults.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <backend/gpu/mali_kbase_power_model_simple.h>
#endif

#include <linux/clk.h>
#include <linux/devfreq.h>
#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/devfreq_cooling.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/pm_opp.h>
#else /* Linux >= 3.13 */
/* In 3.13 the OPP include header file, types, and functions were all
 * renamed. Use the old filename for the include, and define the new names to
 * the old, when an old kernel is detected.
 */
#include <linux/opp.h>
#define dev_pm_opp opp
#define dev_pm_opp_get_voltage opp_get_voltage
#define dev_pm_opp_get_opp_count opp_get_opp_count
#define dev_pm_opp_find_freq_ceil opp_find_freq_ceil
#endif /* Linux >= 3.13 */
#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>

static struct devfreq_simple_ondemand_data ondemand_data;

static struct monitor_dev_profile mali_mdevp = {
	.type = MONITOR_TPYE_DEV,
	.low_temp_adjust = rockchip_monitor_dev_low_temp_adjust,
	.high_temp_adjust = rockchip_monitor_dev_high_temp_adjust,
};

static int
kbase_devfreq_target(struct device *dev, unsigned long *target_freq, u32 flags)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	unsigned long old_freq = kbdev->current_freq;
	unsigned long voltage;
	int err;

	freq = *target_freq;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, &freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Failed to get opp (%ld)\n", PTR_ERR(opp));
		return PTR_ERR(opp);
	}
	voltage = dev_pm_opp_get_voltage(opp);
	rcu_read_unlock();

	/*
	 * Only update if there is a change of frequency
	 */
	if (old_freq == freq) {
		*target_freq = freq;
#ifdef CONFIG_REGULATOR
		if (kbdev->current_voltage == voltage)
			return 0;
		err = regulator_set_voltage(kbdev->regulator, voltage, INT_MAX);
		if (err) {
			dev_err(dev, "Failed to set voltage (%d)\n", err);
			return err;
		}
#else
		return 0;
#endif
	}

#ifdef CONFIG_REGULATOR
	if (kbdev->regulator && kbdev->current_voltage != voltage &&
	    old_freq < freq) {
		err = regulator_set_voltage(kbdev->regulator, voltage, INT_MAX);
		if (err) {
			dev_err(dev, "Failed to increase voltage (%d)\n", err);
			return err;
		}
	}
#endif

	err = clk_set_rate(kbdev->clock, freq);
	if (err) {
		dev_err(dev, "Failed to set clock %lu (target %lu)\n",
				freq, *target_freq);
		return err;
	}
	*target_freq = freq;
	kbdev->current_freq = freq;
	if (kbdev->devfreq)
		kbdev->devfreq->last_status.current_frequency = freq;
#ifdef CONFIG_REGULATOR
	if (kbdev->regulator && kbdev->current_voltage != voltage &&
	    old_freq > freq) {
		err = regulator_set_voltage(kbdev->regulator, voltage, INT_MAX);
		if (err) {
			dev_err(dev, "Failed to decrease voltage (%d)\n", err);
			return err;
		}
	}
#endif

	kbdev->current_voltage = voltage;

	kbase_tlstream_aux_devfreq_target((u64)freq);

	kbase_pm_reset_dvfs_utilisation(kbdev);

	return err;
}

static int
kbase_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	*freq = kbdev->current_freq;

	return 0;
}

static int
kbase_devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	stat->current_frequency = kbdev->current_freq;

	kbase_pm_get_dvfs_utilisation(kbdev,
			&stat->total_time, &stat->busy_time);

	stat->private_data = NULL;

#ifdef CONFIG_DEVFREQ_THERMAL
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0)
	if (kbdev->devfreq_cooling)
		memcpy(&kbdev->devfreq_cooling->last_status, stat,
				sizeof(*stat));
#endif
#endif

	return 0;
}

static int kbase_devfreq_init_freq_table(struct kbase_device *kbdev,
		struct devfreq_dev_profile *dp)
{
	int count;
	int i = 0;
	unsigned long freq = 0;
	struct dev_pm_opp *opp;

	rcu_read_lock();
	count = dev_pm_opp_get_opp_count(kbdev->dev);
	if (count < 0) {
		rcu_read_unlock();
		return count;
	}
	rcu_read_unlock();

	dp->freq_table = kmalloc_array(count, sizeof(dp->freq_table[0]),
				GFP_KERNEL);
	if (!dp->freq_table)
		return -ENOMEM;

	rcu_read_lock();
	for (i = 0; i < count; i++, freq++) {
		opp = dev_pm_opp_find_freq_ceil(kbdev->dev, &freq);
		if (IS_ERR(opp))
			break;

		dp->freq_table[i] = freq;
	}
	rcu_read_unlock();

	if (count != i)
		dev_warn(kbdev->dev, "Unable to enumerate all OPPs (%d!=%d\n",
				count, i);

	dp->max_state = i;

	return 0;
}

static void kbase_devfreq_term_freq_table(struct kbase_device *kbdev)
{
	struct devfreq_dev_profile *dp = kbdev->devfreq->profile;

	kfree(dp->freq_table);
}

static void kbase_devfreq_exit(struct device *dev)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	kbase_devfreq_term_freq_table(kbdev);
}

int kbase_devfreq_init(struct kbase_device *kbdev)
{
	struct device_node *np = kbdev->dev->of_node;
	struct devfreq_dev_profile *dp;
	unsigned long opp_rate;
	int err;

	if (!kbdev->clock)
		return -ENODEV;

	kbdev->current_freq = clk_get_rate(kbdev->clock);
#ifdef CONFIG_REGULATOR
	if (kbdev->regulator)
		kbdev->current_voltage =
			regulator_get_voltage(kbdev->regulator);
#endif

	dp = &kbdev->devfreq_profile;

	dp->initial_freq = kbdev->current_freq;
	/* .KP : set devfreq_dvfs_interval_in_ms */
	dp->polling_ms = 20;
	dp->target = kbase_devfreq_target;
	dp->get_dev_status = kbase_devfreq_status;
	dp->get_cur_freq = kbase_devfreq_cur_freq;
	dp->exit = kbase_devfreq_exit;

	if (kbase_devfreq_init_freq_table(kbdev, dp))
		return -EFAULT;

	of_property_read_u32(np, "upthreshold",
			     &ondemand_data.upthreshold);
	of_property_read_u32(np, "downdifferential",
			     &ondemand_data.downdifferential);

	kbdev->devfreq = devfreq_add_device(kbdev->dev, dp,
				"simple_ondemand", &ondemand_data);
	if (IS_ERR(kbdev->devfreq)) {
		kbase_devfreq_term_freq_table(kbdev);
		return PTR_ERR(kbdev->devfreq);
	}

	err = devfreq_register_opp_notifier(kbdev->dev, kbdev->devfreq);
	if (err) {
		dev_err(kbdev->dev,
			"Failed to register OPP notifier (%d)\n", err);
		goto opp_notifier_failed;
	}

	opp_rate = kbdev->current_freq;
	rcu_read_lock();
	devfreq_recommended_opp(kbdev->dev, &opp_rate, 0);
	rcu_read_unlock();
	kbdev->devfreq->last_status.current_frequency = opp_rate;

	mali_mdevp.data = kbdev->devfreq;
	kbdev->mdev_info = rockchip_system_monitor_register(kbdev->dev,
							    &mali_mdevp);
	if (IS_ERR(kbdev->mdev_info)) {
		dev_dbg(kbdev->dev, "without system monitor\n");
		kbdev->mdev_info = NULL;
	}
#ifdef CONFIG_DEVFREQ_THERMAL
	err = kbase_power_model_simple_init(kbdev);
	if (err && err != -ENODEV && err != -EPROBE_DEFER) {
		dev_err(kbdev->dev,
			"Failed to initialize simple power model (%d)\n",
			err);
		goto cooling_failed;
	}
	if (err == -EPROBE_DEFER)
		goto cooling_failed;
	if (err != -ENODEV) {
		kbdev->devfreq_cooling = of_devfreq_cooling_register_power(
				kbdev->dev->of_node,
				kbdev->devfreq,
				&power_model_simple_ops);
		if (IS_ERR_OR_NULL(kbdev->devfreq_cooling)) {
			err = PTR_ERR(kbdev->devfreq_cooling);
			dev_err(kbdev->dev,
				"Failed to register cooling device (%d)\n",
				err);
			goto cooling_failed;
		}
	} else {
		err = 0;
	}
	I("success initing power_model_simple.");
#endif

	return 0;

#ifdef CONFIG_DEVFREQ_THERMAL
cooling_failed:
	devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);
#endif /* CONFIG_DEVFREQ_THERMAL */
opp_notifier_failed:
	if (devfreq_remove_device(kbdev->devfreq))
		dev_err(kbdev->dev, "Failed to terminate devfreq (%d)\n", err);
	else
		kbdev->devfreq = NULL;

	return err;
}

void kbase_devfreq_term(struct kbase_device *kbdev)
{
	int err;

	dev_dbg(kbdev->dev, "Term Mali devfreq\n");

	rockchip_system_monitor_unregister(kbdev->mdev_info);
#ifdef CONFIG_DEVFREQ_THERMAL
	if (kbdev->devfreq_cooling)
		devfreq_cooling_unregister(kbdev->devfreq_cooling);
#endif

	devfreq_unregister_opp_notifier(kbdev->dev, kbdev->devfreq);

	err = devfreq_remove_device(kbdev->devfreq);
	if (err)
		dev_err(kbdev->dev, "Failed to terminate devfreq (%d)\n", err);
	else
		kbdev->devfreq = NULL;
}
