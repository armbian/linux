/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author: Lin Huang <hl@rock-chips.com>
 */

#ifndef __SOC_ROCKCHIP_PHY_TYPEC_H
#define __SOC_ROCKCHIP_PHY_TYPEC_H

struct usb3phy_reg {
	u32 offset;
	u32 enable_bit;
	u32 write_enable;
};

struct rockchip_usb3phy_port_cfg {
	struct usb3phy_reg typec_conn_dir;
	struct usb3phy_reg usb3tousb2_en;
	struct usb3phy_reg usb3host_disable;
	struct usb3phy_reg usb3host_port;
	struct usb3phy_reg external_psm;
	struct usb3phy_reg pipe_status;
	struct usb3phy_reg uphy_dp_sel;
};

struct phy_config {
	int swing;
	int pe;
};

struct rockchip_typec_phy {
	struct device *dev;
	void __iomem *base;
	struct extcon_dev *extcon;
	struct regmap *grf_regs;
	struct clk *clk_core;
	struct clk *clk_ref;
	struct reset_control *uphy_rst;
	struct reset_control *pipe_rst;
	struct reset_control *tcphy_rst;
	struct rockchip_usb3phy_port_cfg port_cfgs;
	/* mutex to protect access to individual PHYs */
	struct mutex lock;

	bool flip;
	u8 mode;
	struct phy_config config[3][4];
	int (*typec_phy_config)(struct phy *phy, int link_rate,
				int lanes, u8 swing, u8 pre_emp);
};

#if IS_ENABLED(CONFIG_PHY_ROCKCHIP_TYPEC)
int tcphy_dp_set_lane_count(struct phy *phy, u8 lane_count);
int tcphy_dp_set_link_rate(struct phy *phy, int link_rate, bool ssc_on);
#else
static inline int tcphy_dp_set_lane_count(struct phy *phy, u8 lane_count)
{
	return -ENODEV;
}

static inline int tcphy_dp_set_link_rate(struct phy *phy, int link_rate,
					 bool ssc_on)
{
	return -ENODEV;
}
#endif

#endif
