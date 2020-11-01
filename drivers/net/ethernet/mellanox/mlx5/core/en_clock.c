/*
 * Copyright (c) 2015, Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/clocksource.h>
#include "en.h"

enum {
	MLX5E_CYCLES_SHIFT	= 23
};

enum {
	MLX5E_PIN_MODE_IN		= 0x0,
	MLX5E_PIN_MODE_OUT		= 0x1,
};

enum {
	MLX5E_OUT_PATTERN_PULSE		= 0x0,
	MLX5E_OUT_PATTERN_PERIODIC	= 0x1,
};

enum {
	MLX5E_EVENT_MODE_DISABLE	= 0x0,
	MLX5E_EVENT_MODE_REPETETIVE	= 0x1,
	MLX5E_EVENT_MODE_ONCE_TILL_ARM	= 0x2,
};

enum {
	MLX5E_MTPPS_FS_ENABLE			= BIT(0x0),
	MLX5E_MTPPS_FS_PATTERN			= BIT(0x2),
	MLX5E_MTPPS_FS_PIN_MODE			= BIT(0x3),
	MLX5E_MTPPS_FS_TIME_STAMP		= BIT(0x4),
	MLX5E_MTPPS_FS_OUT_PULSE_DURATION	= BIT(0x5),
	MLX5E_MTPPS_FS_ENH_OUT_PER_ADJ		= BIT(0x7),
};

void mlx5e_fill_hwstamp(struct mlx5e_tstamp *tstamp, u64 timestamp,
			struct skb_shared_hwtstamps *hwts)
{
	u64 nsec;

	read_lock(&tstamp->lock);
	nsec = timecounter_cyc2time(&tstamp->clock, timestamp);
	read_unlock(&tstamp->lock);

	hwts->hwtstamp = ns_to_ktime(nsec);
}

static u64 mlx5e_read_internal_timer(const struct cyclecounter *cc)
{
	struct mlx5e_tstamp *tstamp = container_of(cc, struct mlx5e_tstamp,
						   cycles);

	return mlx5_read_internal_timer(tstamp->mdev) & cc->mask;
}

static void mlx5e_pps_out(struct work_struct *work)
{
	struct mlx5e_pps *pps_info = container_of(work, struct mlx5e_pps,
						  out_work);
	struct mlx5e_tstamp *tstamp = container_of(pps_info, struct mlx5e_tstamp,
						   pps_info);
	u32 in[MLX5_ST_SZ_DW(mtpps_reg)] = {0};
	unsigned long flags;
	int i;

	for (i = 0; i < tstamp->ptp_info.n_pins; i++) {
		u64 tstart;

		write_lock_irqsave(&tstamp->lock, flags);
		tstart = tstamp->pps_info.start[i];
		tstamp->pps_info.start[i] = 0;
		write_unlock_irqrestore(&tstamp->lock, flags);
		if (!tstart)
			continue;

		MLX5_SET(mtpps_reg, in, pin, i);
		MLX5_SET64(mtpps_reg, in, time_stamp, tstart);
		MLX5_SET(mtpps_reg, in, field_select, MLX5E_MTPPS_FS_TIME_STAMP);
		mlx5_set_mtpps(tstamp->mdev, in, sizeof(in));
	}
}

static void mlx5e_timestamp_overflow(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mlx5e_tstamp *tstamp = container_of(dwork, struct mlx5e_tstamp,
						   overflow_work);
	struct mlx5e_priv *priv = container_of(tstamp, struct mlx5e_priv, tstamp);
	unsigned long flags;

	write_lock_irqsave(&tstamp->lock, flags);
	timecounter_read(&tstamp->clock);
	write_unlock_irqrestore(&tstamp->lock, flags);
	queue_delayed_work(priv->wq, &tstamp->overflow_work,
			   msecs_to_jiffies(tstamp->overflow_period * 1000));
}

int mlx5e_hwstamp_set(struct mlx5e_priv *priv, struct ifreq *ifr)
{
	struct hwtstamp_config config;
	int err;

	if (!MLX5_CAP_GEN(priv->mdev, device_frequency_khz))
		return -EOPNOTSUPP;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	/* TX HW timestamp */
	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
		break;
	default:
		return -ERANGE;
	}

	mutex_lock(&priv->state_lock);
	/* RX HW timestamp */
	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		/* Reset CQE compression to Admin default */
		mlx5e_modify_rx_cqe_compression_locked(priv, priv->channels.params.rx_cqe_compress_def);
		break;
	case HWTSTAMP_FILTER_ALL:
	case HWTSTAMP_FILTER_SOME:
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
	case HWTSTAMP_FILTER_NTP_ALL:
		/* Disable CQE compression */
		netdev_warn(priv->netdev, "Disabling cqe compression");
		err = mlx5e_modify_rx_cqe_compression_locked(priv, false);
		if (err) {
			netdev_err(priv->netdev, "Failed disabling cqe compression err=%d\n", err);
			mutex_unlock(&priv->state_lock);
			return err;
		}
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		break;
	default:
		mutex_unlock(&priv->state_lock);
		return -ERANGE;
	}

	memcpy(&priv->tstamp.hwtstamp_config, &config, sizeof(config));
	mutex_unlock(&priv->state_lock);

	return copy_to_user(ifr->ifr_data, &config,
			    sizeof(config)) ? -EFAULT : 0;
}

int mlx5e_hwstamp_get(struct mlx5e_priv *priv, struct ifreq *ifr)
{
	struct hwtstamp_config *cfg = &priv->tstamp.hwtstamp_config;

	if (!MLX5_CAP_GEN(priv->mdev, device_frequency_khz))
		return -EOPNOTSUPP;

	return copy_to_user(ifr->ifr_data, cfg, sizeof(*cfg)) ? -EFAULT : 0;
}

static int mlx5e_ptp_settime(struct ptp_clock_info *ptp,
			     const struct timespec64 *ts)
{
	struct mlx5e_tstamp *tstamp = container_of(ptp, struct mlx5e_tstamp,
						   ptp_info);
	u64 ns = timespec64_to_ns(ts);
	unsigned long flags;

	write_lock_irqsave(&tstamp->lock, flags);
	timecounter_init(&tstamp->clock, &tstamp->cycles, ns);
	write_unlock_irqrestore(&tstamp->lock, flags);

	return 0;
}

static int mlx5e_ptp_gettime(struct ptp_clock_info *ptp,
			     struct timespec64 *ts)
{
	struct mlx5e_tstamp *tstamp = container_of(ptp, struct mlx5e_tstamp,
						   ptp_info);
	u64 ns;
	unsigned long flags;

	write_lock_irqsave(&tstamp->lock, flags);
	ns = timecounter_read(&tstamp->clock);
	write_unlock_irqrestore(&tstamp->lock, flags);

	*ts = ns_to_timespec64(ns);

	return 0;
}

static int mlx5e_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct mlx5e_tstamp *tstamp = container_of(ptp, struct mlx5e_tstamp,
						   ptp_info);
	unsigned long flags;

	write_lock_irqsave(&tstamp->lock, flags);
	timecounter_adjtime(&tstamp->clock, delta);
	write_unlock_irqrestore(&tstamp->lock, flags);

	return 0;
}

static int mlx5e_ptp_adjfreq(struct ptp_clock_info *ptp, s32 delta)
{
	u64 adj;
	u32 diff;
	unsigned long flags;
	int neg_adj = 0;
	struct mlx5e_tstamp *tstamp = container_of(ptp, struct mlx5e_tstamp,
						  ptp_info);

	if (delta < 0) {
		neg_adj = 1;
		delta = -delta;
	}

	adj = tstamp->nominal_c_mult;
	adj *= delta;
	diff = div_u64(adj, 1000000000ULL);

	write_lock_irqsave(&tstamp->lock, flags);
	timecounter_read(&tstamp->clock);
	tstamp->cycles.mult = neg_adj ? tstamp->nominal_c_mult - diff :
					tstamp->nominal_c_mult + diff;
	write_unlock_irqrestore(&tstamp->lock, flags);

	return 0;
}

static int mlx5e_extts_configure(struct ptp_clock_info *ptp,
				 struct ptp_clock_request *rq,
				 int on)
{
	struct mlx5e_tstamp *tstamp =
		container_of(ptp, struct mlx5e_tstamp, ptp_info);
	struct mlx5e_priv *priv =
		container_of(tstamp, struct mlx5e_priv, tstamp);
	u32 in[MLX5_ST_SZ_DW(mtpps_reg)] = {0};
	u32 field_select = 0;
	u8 pin_mode = 0;
	u8 pattern = 0;
	int pin = -1;
	int err = 0;

	if (!MLX5_PPS_CAP(priv->mdev))
		return -EOPNOTSUPP;

	if (rq->extts.index >= tstamp->ptp_info.n_pins)
		return -EINVAL;

	if (on) {
		pin = ptp_find_pin(tstamp->ptp, PTP_PF_EXTTS, rq->extts.index);
		if (pin < 0)
			return -EBUSY;
		pin_mode = MLX5E_PIN_MODE_IN;
		pattern = !!(rq->extts.flags & PTP_FALLING_EDGE);
		field_select = MLX5E_MTPPS_FS_PIN_MODE |
			       MLX5E_MTPPS_FS_PATTERN |
			       MLX5E_MTPPS_FS_ENABLE;
	} else {
		pin = rq->extts.index;
		field_select = MLX5E_MTPPS_FS_ENABLE;
	}

	MLX5_SET(mtpps_reg, in, pin, pin);
	MLX5_SET(mtpps_reg, in, pin_mode, pin_mode);
	MLX5_SET(mtpps_reg, in, pattern, pattern);
	MLX5_SET(mtpps_reg, in, enable, on);
	MLX5_SET(mtpps_reg, in, field_select, field_select);

	err = mlx5_set_mtpps(priv->mdev, in, sizeof(in));
	if (err)
		return err;

	return mlx5_set_mtppse(priv->mdev, pin, 0,
			       MLX5E_EVENT_MODE_REPETETIVE & on);
}

static int mlx5e_perout_configure(struct ptp_clock_info *ptp,
				  struct ptp_clock_request *rq,
				  int on)
{
	struct mlx5e_tstamp *tstamp =
		container_of(ptp, struct mlx5e_tstamp, ptp_info);
	struct mlx5e_priv *priv =
		container_of(tstamp, struct mlx5e_priv, tstamp);
	u32 in[MLX5_ST_SZ_DW(mtpps_reg)] = {0};
	u64 nsec_now, nsec_delta, time_stamp = 0;
	u64 cycles_now, cycles_delta;
	struct timespec64 ts;
	unsigned long flags;
	u32 field_select = 0;
	u8 pin_mode = 0;
	u8 pattern = 0;
	int pin = -1;
	int err = 0;
	s64 ns;

	if (!MLX5_PPS_CAP(priv->mdev))
		return -EOPNOTSUPP;

	if (rq->perout.index >= tstamp->ptp_info.n_pins)
		return -EINVAL;

	if (on) {
		pin = ptp_find_pin(tstamp->ptp, PTP_PF_PEROUT,
				   rq->perout.index);
		if (pin < 0)
			return -EBUSY;

		pin_mode = MLX5E_PIN_MODE_OUT;
		pattern = MLX5E_OUT_PATTERN_PERIODIC;
		ts.tv_sec = rq->perout.period.sec;
		ts.tv_nsec = rq->perout.period.nsec;
		ns = timespec64_to_ns(&ts);

		if ((ns >> 1) != 500000000LL)
			return -EINVAL;

		ts.tv_sec = rq->perout.start.sec;
		ts.tv_nsec = rq->perout.start.nsec;
		ns = timespec64_to_ns(&ts);
		cycles_now = mlx5_read_internal_timer(tstamp->mdev);
		write_lock_irqsave(&tstamp->lock, flags);
		nsec_now = timecounter_cyc2time(&tstamp->clock, cycles_now);
		nsec_delta = ns - nsec_now;
		cycles_delta = div64_u64(nsec_delta << tstamp->cycles.shift,
					 tstamp->cycles.mult);
		write_unlock_irqrestore(&tstamp->lock, flags);
		time_stamp = cycles_now + cycles_delta;
		field_select = MLX5E_MTPPS_FS_PIN_MODE |
			       MLX5E_MTPPS_FS_PATTERN |
			       MLX5E_MTPPS_FS_ENABLE |
			       MLX5E_MTPPS_FS_TIME_STAMP;
	} else {
		pin = rq->perout.index;
		field_select = MLX5E_MTPPS_FS_ENABLE;
	}

	MLX5_SET(mtpps_reg, in, pin, pin);
	MLX5_SET(mtpps_reg, in, pin_mode, pin_mode);
	MLX5_SET(mtpps_reg, in, pattern, pattern);
	MLX5_SET(mtpps_reg, in, enable, on);
	MLX5_SET64(mtpps_reg, in, time_stamp, time_stamp);
	MLX5_SET(mtpps_reg, in, field_select, field_select);

	err = mlx5_set_mtpps(priv->mdev, in, sizeof(in));
	if (err)
		return err;

	return mlx5_set_mtppse(priv->mdev, pin, 0,
			       MLX5E_EVENT_MODE_REPETETIVE & on);
}

static int mlx5e_pps_configure(struct ptp_clock_info *ptp,
			       struct ptp_clock_request *rq,
			       int on)
{
	struct mlx5e_tstamp *tstamp =
		container_of(ptp, struct mlx5e_tstamp, ptp_info);

	tstamp->pps_info.enabled = !!on;
	return 0;
}

static int mlx5e_ptp_enable(struct ptp_clock_info *ptp,
			    struct ptp_clock_request *rq,
			    int on)
{
	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS:
		return mlx5e_extts_configure(ptp, rq, on);
	case PTP_CLK_REQ_PEROUT:
		return mlx5e_perout_configure(ptp, rq, on);
	case PTP_CLK_REQ_PPS:
		return mlx5e_pps_configure(ptp, rq, on);
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int mlx5e_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
			    enum ptp_pin_function func, unsigned int chan)
{
	return (func == PTP_PF_PHYSYNC) ? -EOPNOTSUPP : 0;
}

static const struct ptp_clock_info mlx5e_ptp_clock_info = {
	.owner		= THIS_MODULE,
	.max_adj	= 100000000,
	.n_alarm	= 0,
	.n_ext_ts	= 0,
	.n_per_out	= 0,
	.n_pins		= 0,
	.pps		= 0,
	.adjfreq	= mlx5e_ptp_adjfreq,
	.adjtime	= mlx5e_ptp_adjtime,
	.gettime64	= mlx5e_ptp_gettime,
	.settime64	= mlx5e_ptp_settime,
	.enable		= NULL,
	.verify		= NULL,
};

static void mlx5e_timestamp_init_config(struct mlx5e_tstamp *tstamp)
{
	tstamp->hwtstamp_config.tx_type = HWTSTAMP_TX_OFF;
	tstamp->hwtstamp_config.rx_filter = HWTSTAMP_FILTER_NONE;
}

static int mlx5e_init_pin_config(struct mlx5e_tstamp *tstamp)
{
	int i;

	tstamp->ptp_info.pin_config =
		kzalloc(sizeof(*tstamp->ptp_info.pin_config) *
			       tstamp->ptp_info.n_pins, GFP_KERNEL);
	if (!tstamp->ptp_info.pin_config)
		return -ENOMEM;
	tstamp->ptp_info.enable = mlx5e_ptp_enable;
	tstamp->ptp_info.verify = mlx5e_ptp_verify;
	tstamp->ptp_info.pps = 1;

	for (i = 0; i < tstamp->ptp_info.n_pins; i++) {
		snprintf(tstamp->ptp_info.pin_config[i].name,
			 sizeof(tstamp->ptp_info.pin_config[i].name),
			 "mlx5_pps%d", i);
		tstamp->ptp_info.pin_config[i].index = i;
		tstamp->ptp_info.pin_config[i].func = PTP_PF_NONE;
		tstamp->ptp_info.pin_config[i].chan = i;
	}

	return 0;
}

static void mlx5e_get_pps_caps(struct mlx5e_priv *priv,
			       struct mlx5e_tstamp *tstamp)
{
	u32 out[MLX5_ST_SZ_DW(mtpps_reg)] = {0};

	mlx5_query_mtpps(priv->mdev, out, sizeof(out));

	tstamp->ptp_info.n_pins = MLX5_GET(mtpps_reg, out,
					   cap_number_of_pps_pins);
	tstamp->ptp_info.n_ext_ts = MLX5_GET(mtpps_reg, out,
					     cap_max_num_of_pps_in_pins);
	tstamp->ptp_info.n_per_out = MLX5_GET(mtpps_reg, out,
					      cap_max_num_of_pps_out_pins);

	tstamp->pps_info.pin_caps[0] = MLX5_GET(mtpps_reg, out, cap_pin_0_mode);
	tstamp->pps_info.pin_caps[1] = MLX5_GET(mtpps_reg, out, cap_pin_1_mode);
	tstamp->pps_info.pin_caps[2] = MLX5_GET(mtpps_reg, out, cap_pin_2_mode);
	tstamp->pps_info.pin_caps[3] = MLX5_GET(mtpps_reg, out, cap_pin_3_mode);
	tstamp->pps_info.pin_caps[4] = MLX5_GET(mtpps_reg, out, cap_pin_4_mode);
	tstamp->pps_info.pin_caps[5] = MLX5_GET(mtpps_reg, out, cap_pin_5_mode);
	tstamp->pps_info.pin_caps[6] = MLX5_GET(mtpps_reg, out, cap_pin_6_mode);
	tstamp->pps_info.pin_caps[7] = MLX5_GET(mtpps_reg, out, cap_pin_7_mode);
}

void mlx5e_pps_event_handler(struct mlx5e_priv *priv,
			     struct ptp_clock_event *event)
{
	struct net_device *netdev = priv->netdev;
	struct mlx5e_tstamp *tstamp = &priv->tstamp;
	struct timespec64 ts;
	u64 nsec_now, nsec_delta;
	u64 cycles_now, cycles_delta;
	int pin = event->index;
	s64 ns;
	unsigned long flags;

	switch (tstamp->ptp_info.pin_config[pin].func) {
	case PTP_PF_EXTTS:
		if (tstamp->pps_info.enabled) {
			event->type = PTP_CLOCK_PPSUSR;
			event->pps_times.ts_real = ns_to_timespec64(event->timestamp);
		} else {
			event->type = PTP_CLOCK_EXTTS;
		}
		ptp_clock_event(tstamp->ptp, event);
		break;
	case PTP_PF_PEROUT:
		mlx5e_ptp_gettime(&tstamp->ptp_info, &ts);
		cycles_now = mlx5_read_internal_timer(tstamp->mdev);
		ts.tv_sec += 1;
		ts.tv_nsec = 0;
		ns = timespec64_to_ns(&ts);
		write_lock_irqsave(&tstamp->lock, flags);
		nsec_now = timecounter_cyc2time(&tstamp->clock, cycles_now);
		nsec_delta = ns - nsec_now;
		cycles_delta = div64_u64(nsec_delta << tstamp->cycles.shift,
					 tstamp->cycles.mult);
		tstamp->pps_info.start[pin] = cycles_now + cycles_delta;
		queue_work(priv->wq, &tstamp->pps_info.out_work);
		write_unlock_irqrestore(&tstamp->lock, flags);
		break;
	default:
		netdev_err(netdev, "%s: Unhandled event\n", __func__);
	}
}

void mlx5e_timestamp_init(struct mlx5e_priv *priv)
{
	struct mlx5e_tstamp *tstamp = &priv->tstamp;
	u64 overflow_cycles;
	u64 ns;
	u64 frac = 0;
	u32 dev_freq;

	mlx5e_timestamp_init_config(tstamp);
	dev_freq = MLX5_CAP_GEN(priv->mdev, device_frequency_khz);
	if (!dev_freq) {
		mlx5_core_warn(priv->mdev, "invalid device_frequency_khz, aborting HW clock init\n");
		return;
	}
	rwlock_init(&tstamp->lock);
	tstamp->cycles.read = mlx5e_read_internal_timer;
	tstamp->cycles.shift = MLX5E_CYCLES_SHIFT;
	tstamp->cycles.mult = clocksource_khz2mult(dev_freq,
						   tstamp->cycles.shift);
	tstamp->nominal_c_mult = tstamp->cycles.mult;
	tstamp->cycles.mask = CLOCKSOURCE_MASK(41);
	tstamp->mdev = priv->mdev;

	timecounter_init(&tstamp->clock, &tstamp->cycles,
			 ktime_to_ns(ktime_get_real()));

	/* Calculate period in seconds to call the overflow watchdog - to make
	 * sure counter is checked at least once every wrap around.
	 * The period is calculated as the minimum between max HW cycles count
	 * (The clock source mask) and max amount of cycles that can be
	 * multiplied by clock multiplier where the result doesn't exceed
	 * 64bits.
	 */
	overflow_cycles = div64_u64(~0ULL >> 1, tstamp->cycles.mult);
	overflow_cycles = min(overflow_cycles, tstamp->cycles.mask >> 1);

	ns = cyclecounter_cyc2ns(&tstamp->cycles, overflow_cycles,
				 frac, &frac);
	do_div(ns, NSEC_PER_SEC / HZ);
	tstamp->overflow_period = ns;

	INIT_WORK(&tstamp->pps_info.out_work, mlx5e_pps_out);
	INIT_DELAYED_WORK(&tstamp->overflow_work, mlx5e_timestamp_overflow);
	if (tstamp->overflow_period)
		queue_delayed_work(priv->wq, &tstamp->overflow_work, 0);
	else
		mlx5_core_warn(priv->mdev, "invalid overflow period, overflow_work is not scheduled\n");

	/* Configure the PHC */
	tstamp->ptp_info = mlx5e_ptp_clock_info;
	snprintf(tstamp->ptp_info.name, 16, "mlx5 ptp");

	/* Initialize 1PPS data structures */
	if (MLX5_PPS_CAP(priv->mdev))
		mlx5e_get_pps_caps(priv, tstamp);
	if (tstamp->ptp_info.n_pins)
		mlx5e_init_pin_config(tstamp);

	tstamp->ptp = ptp_clock_register(&tstamp->ptp_info,
					 &priv->mdev->pdev->dev);
	if (IS_ERR(tstamp->ptp)) {
		mlx5_core_warn(priv->mdev, "ptp_clock_register failed %ld\n",
			       PTR_ERR(tstamp->ptp));
		tstamp->ptp = NULL;
	}
}

void mlx5e_timestamp_cleanup(struct mlx5e_priv *priv)
{
	struct mlx5e_tstamp *tstamp = &priv->tstamp;

	if (!MLX5_CAP_GEN(priv->mdev, device_frequency_khz))
		return;

	if (priv->tstamp.ptp) {
		ptp_clock_unregister(priv->tstamp.ptp);
		priv->tstamp.ptp = NULL;
	}

	cancel_work_sync(&tstamp->pps_info.out_work);
	cancel_delayed_work_sync(&tstamp->overflow_work);
	kfree(tstamp->ptp_info.pin_config);
}
