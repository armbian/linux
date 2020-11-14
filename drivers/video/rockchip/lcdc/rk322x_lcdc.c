/*
 * drivers/video/rockchip/lcdc/rk322x_lcdc.c
 *
 * Copyright (C) 2015 ROCKCHIP, Inc.
 * Author: Mark Yao <mark.yao@rock-chips.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/rockchip-iovmm.h>
#include <asm/div64.h>
#include <linux/uaccess.h>
#include <linux/rockchip/grf.h>
#include <dt-bindings/clock/rk_system_status.h>
#include <soc/rockchip/rkfb_dmc.h>
#include <linux/of_gpio.h>

#include "rk322x_lcdc.h"

/*#define CONFIG_RK_FPGA 1*/
#define VOP_CHIP(dev)	(dev->data->chip_type)

static int dbg_thresd;
module_param(dbg_thresd, int, S_IRUGO | S_IWUSR);

#define DBG(level, x...) do {			\
	if (unlikely(dbg_thresd >= level))	\
		pr_info(x);\
	} while (0)

static struct rk_lcdc_win rk322x_vop_win[] = {
	{ .name = "win0",
	  .id = VOP_WIN0,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_SCALE | SUPPORT_YUV |
				SUPPORT_YUV10BIT,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{ .name = "win1",
	  .id = VOP_WIN1,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_SCALE | SUPPORT_YUV |
				SUPPORT_YUV10BIT,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{
	  .name = "hwc",
	  .id = VOP_HWC,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_HWC_LAYER,
	  .property.max_input_x = 128,
	  .property.max_input_y = 128
	}
};

static struct rk_lcdc_win rk3399_vop_win[] = {
	{ .name = "win0",
	  .id = VOP_WIN0,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_SCALE | SUPPORT_YUV |
				SUPPORT_YUV10BIT,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{ .name = "win1",
	  .id = VOP_WIN1,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_SCALE | SUPPORT_YUV |
				SUPPORT_YUV10BIT,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{ .name = "win2",
	  .id = VOP_WIN2,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_MULTI_AREA,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{ .name = "win3",
	  .id = VOP_WIN3,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_MULTI_AREA,
	  .property.max_input_x = 4096,
	  .property.max_input_y = 2304},
	{
	  .name = "hwc",
	  .id = VOP_HWC,
	  .property.feature = SUPPORT_WIN_IDENTIFY | SUPPORT_HW_EXIST |
				SUPPORT_HWC_LAYER,
	  .property.max_input_x = 128,
	  .property.max_input_y = 128
	}
};

static const struct vop_data rk322x_data = {
	.chip_type = VOP_RK322X,
	.win = rk322x_vop_win,
	.n_wins = ARRAY_SIZE(rk322x_vop_win),
};

static const struct vop_data rk3399_data = {
	.chip_type = VOP_RK3399,
	.win = rk3399_vop_win,
	.n_wins = ARRAY_SIZE(rk3399_vop_win),
};

#if defined(CONFIG_OF)
static const struct of_device_id vop_dt_ids[] = {
	{.compatible = "rockchip,rk322x-lcdc",
	 .data = &rk322x_data, },
	{.compatible = "rockchip,rk3399-lcdc",
	 .data = &rk3399_data, },
	{}
};
#endif

static const u32 csc_y2r_bt601_limit[12] = {
	0x04a8,      0,  0x0662, 0xfffc8654,
	0x04a8, 0xfe6f,  0xfcbf, 0x00022056,
	0x04a8, 0x0812,       0, 0xfffbaeac,
};

static const u32 csc_y2r_bt709_full[12] = {
	0x04a8,      0,  0x072c, 0xfffc219e,
	0x04a8, 0xff26,  0xfdde, 0x0001357b,
	0x04a8, 0x0873,       0, 0xfffb7dee,
};

static const u32 csc_y2r_bt601_full[12] = {
	0x0400,      0,  0x059c, 0xfffd342d,
	0x0400, 0xfea0,  0xfd25, 0x00021fcc,
	0x0400, 0x0717,       0, 0xfffc76bc,
};

static const u32 csc_y2r_bt601_limit_10[12] = {
	0x04a8,      0,  0x0662, 0xfff2134e,
	0x04a8, 0xfe6f,  0xfcbf, 0x00087b58,
	0x04a8, 0x0812,       0, 0xffeeb4b0,
};

static const u32 csc_y2r_bt709_full_10[12] = {
	0x04a8,      0,  0x072c, 0xfff08077,
	0x04a8, 0xff26,  0xfdde, 0x0004cfed,
	0x04a8, 0x0873,       0, 0xffedf1b8,
};

static const u32 csc_y2r_bt601_full_10[12] = {
	0x0400,      0,  0x059c, 0xfff4cab4,
	0x0400, 0xfea0,  0xfd25, 0x00087932,
	0x0400, 0x0717,       0, 0xfff1d4f2,
};

static const u32 csc_y2r_bt2020[12] = {
	0x04a8,      0, 0x06b6, 0xfff16bfc,
	0x04a8, 0xff40, 0xfd66, 0x58ae9,
	0x04a8, 0x0890,      0, 0xffedb828,
};

static const u32 csc_r2y_bt601_limit[12] = {
	0x0107, 0x0204, 0x0064, 0x04200,
	0xff68, 0xfed6, 0x01c2, 0x20200,
	0x01c2, 0xfe87, 0xffb7, 0x20200,
};

static const u32 csc_r2y_bt709_full[12] = {
	0x00bb, 0x0275, 0x003f, 0x04200,
	0xff99, 0xfea5, 0x01c2, 0x20200,
	0x01c2, 0xfe68, 0xffd7, 0x20200,
};

static const u32 csc_r2y_bt601_full[12] = {
	0x0132, 0x0259, 0x0075, 0x200,
	0xff53, 0xfead, 0x0200, 0x20200,
	0x0200, 0xfe53, 0xffad, 0x20200,
};

static const u32 csc_r2y_bt601_limit_10[12] = {
	0x0107, 0x0204, 0x0064, 0x10200,
	0xff68, 0xfed6, 0x01c2, 0x80200,
	0x01c2, 0xfe87, 0xffb7, 0x80200,
};

static const u32 csc_r2y_bt709_full_10[12] = {
	0x00bb, 0x0275, 0x003f, 0x10200,
	0xff99, 0xfea5, 0x01c2, 0x80200,
	0x01c2, 0xfe68, 0xffd7, 0x80200,
};

static const u32 csc_r2y_bt601_full_10[12] = {
	0x0132, 0x0259, 0x0075, 0x200,
	0xff53, 0xfead, 0x0200, 0x80200,
	0x0200, 0xfe53, 0xffad, 0x80200,
};

static const u32 csc_r2y_bt2020[12] = {
	0x00e6, 0x0253, 0x0034, 0x10200,
	0xff83, 0xfebd, 0x01c1, 0x80200,
	0x01c1, 0xfe64, 0xffdc, 0x80200,
};

static const u32 csc_r2r_bt2020to709[12] = {
	0x06a4, 0xfda6, 0xffb5, 0x200,
	0xff80, 0x0488, 0xfff8, 0x200,
	0xffed, 0xff99, 0x047a, 0x200,
};

static const u32 csc_r2r_bt709to2020[12] = {
	0x282, 0x151, 0x02c, 0x200,
	0x047, 0x3ae, 0x00c, 0x200,
	0x011, 0x05a, 0x395, 0x200,
};

static int vop_get_id(struct vop_device *vop_dev, u32 phy_base)
{
	if (VOP_CHIP(vop_dev) == VOP_RK3399) {
		if (phy_base == 0xff900000) /* vop big */
			return 0;
		else if (phy_base == 0xff8f0000) /* vop lit */
			return 1;
		else
			return -EINVAL;
	} else {
		return 0;
	}
}

static void vop_load_csc_table(struct vop_device *vop_dev, u32 offset,
			       const u32 *table)
{
	u32 csc_val;

	csc_val = table[1] << 16 | table[0];
	vop_writel(vop_dev, offset, csc_val);
	csc_val = table[4] << 16 | table[2];
	vop_writel(vop_dev, offset + 4, csc_val);
	csc_val = table[6] << 16 | table[5];
	vop_writel(vop_dev, offset + 8, csc_val);
	csc_val = table[9] << 16 | table[8];
	vop_writel(vop_dev, offset + 0xc, csc_val);
	csc_val = table[10];
	vop_writel(vop_dev, offset + 0x10, csc_val);
	csc_val = table[3];
	vop_writel(vop_dev, offset + 0x14, csc_val);
	csc_val = table[7];
	vop_writel(vop_dev, offset + 0x18, csc_val);
	csc_val = table[11];
	vop_writel(vop_dev, offset + 0x1c, csc_val);
}

#define LOAD_CSC(dev, mode, table, win_id) \
		vop_load_csc_table(dev, \
				   WIN0_YUV2YUV_##mode + 0x60 * win_id, \
				   table)

static int vop_set_bcsh(struct rk_lcdc_driver *dev_drv, bool enable);

static int vop_set_lut(struct rk_lcdc_driver *dev_drv, int *dsp_lut)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	int i, j;

	if (!vop_dev->dsp_lut_addr_base) {
		dev_warn(vop_dev->dev, "not support dsp lut config\n");
		return 0;
	}

	if (!dsp_lut) {
		dev_err(vop_dev->dev, "dsp lut table is null\n");
		return -EINVAL;
	}

	spin_lock(&vop_dev->reg_lock);
	for (i = 0; i < 256; i++) {
		u32 v, r, g, b;
		int __iomem *c;

		v = dsp_lut[i];
		if (dev_drv->id == 0) {
			c = vop_dev->dsp_lut_addr_base + (i << 2);
			b = (v & 0xff) << 2;
			g = (v & 0xff00) << 4;
			r = (v & 0xff0000) << 6;
			v = r + g + b;
			for (j = 0; j < 4; j++) {
				writel_relaxed(v, c);
				v += (1 + (1 << 10) + (1 << 20));
				c++;
			}
		} else {
			c = vop_dev->dsp_lut_addr_base + i;
			writel_relaxed(v, c);
		}
	}
	vop_msk_reg(vop_dev, DSP_CTRL1, V_DSP_LUT_EN(1));
	/*
	 * update_gamma value auto clean to 0 by HW, should not
	 * bakeup it.
	 */
	vop_msk_reg_nobak(vop_dev, DSP_CTRL1, V_UPDATE_GAMMA_LUT(1));

	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_set_cabc(struct rk_lcdc_driver *dev_drv, int *cabc_lut)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	int i;

	if (!vop_dev->cabc_lut_addr_base) {
		dev_warn(vop_dev->dev, "not support cabc config\n");
		return 0;
	}

	if (!cabc_lut) {
		dev_err(vop_dev->dev, "cabc lut table is null\n");
		return -EINVAL;
	}
	spin_lock(&vop_dev->reg_lock);
	vop_msk_reg(vop_dev, CABC_CTRL1, V_CABC_LUT_EN(0));
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);

	mdelay(25);

	spin_lock(&vop_dev->reg_lock);
	for (i = 0; i < 128; i++) {
		u32 v;

		v = cabc_lut[i];

		writel_relaxed(v, vop_dev->cabc_lut_addr_base + i);
	}
	vop_msk_reg(vop_dev, CABC_CTRL1, V_CABC_LUT_EN(1));
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_clk_enable(struct vop_device *vop_dev)
{
	if (!vop_dev->clk_on) {
		clk_prepare_enable(vop_dev->hclk);
		clk_prepare_enable(vop_dev->dclk);
		clk_prepare_enable(vop_dev->aclk);
		if (vop_dev->hclk_noc)
			clk_prepare_enable(vop_dev->hclk_noc);
		if (vop_dev->aclk_noc)
			clk_prepare_enable(vop_dev->aclk_noc);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		pm_runtime_get_sync(vop_dev->dev);
#endif
		spin_lock(&vop_dev->reg_lock);
		vop_dev->clk_on = 1;
		spin_unlock(&vop_dev->reg_lock);
	}

	return 0;
}

static int vop_clk_disable(struct vop_device *vop_dev)
{
	if (vop_dev->clk_on) {
		spin_lock(&vop_dev->reg_lock);
		vop_dev->clk_on = 0;
		spin_unlock(&vop_dev->reg_lock);
		mdelay(25);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		pm_runtime_put_sync(vop_dev->dev);
#endif
		clk_disable_unprepare(vop_dev->dclk);
		clk_disable_unprepare(vop_dev->hclk);
		clk_disable_unprepare(vop_dev->aclk);
		if (vop_dev->hclk_noc)
			clk_disable_unprepare(vop_dev->hclk_noc);
		if (vop_dev->aclk_noc)
			clk_disable_unprepare(vop_dev->aclk_noc);
	}

	return 0;
}

static int __maybe_unused vop_disable_irq(struct vop_device *vop_dev)
{
	if (likely(vop_dev->clk_on)) {
		spin_lock(&vop_dev->reg_lock);
		vop_writel(vop_dev, INTR_EN0, 0xffff0000);
		vop_writel(vop_dev, INTR_EN1, 0xffff0000);
		vop_writel(vop_dev, INTR_CLEAR0, 0xffffffff);
		vop_writel(vop_dev, INTR_CLEAR1, 0xffffffff);
		vop_cfg_done(vop_dev);
		spin_unlock(&vop_dev->reg_lock);
	};

	return 0;
}

static int vop_reg_dump(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int *cbase = (int *)vop_dev->regs;
	int *regsbak = (int *)vop_dev->regsbak;
	int i, j, val;
	char dbg_message[30];
	char buf[10];

	pr_info("lcd back up reg:\n");
	memset(dbg_message, 0, sizeof(dbg_message));
	memset(buf, 0, sizeof(buf));
	for (i = 0; i <= (0x200 >> 4); i++) {
		val = sprintf(dbg_message, "0x%04x: ", i * 16);
		for (j = 0; j < 4; j++) {
			val = sprintf(buf, "%08x  ", *(regsbak + i * 4 + j));
			strcat(dbg_message, buf);
		}
		pr_info("%s\n", dbg_message);
		memset(dbg_message, 0, sizeof(dbg_message));
		memset(buf, 0, sizeof(buf));
	}

	pr_info("lcdc reg:\n");
	for (i = 0; i <= (0x200 >> 4); i++) {
		val = sprintf(dbg_message, "0x%04x: ", i * 16);
		for (j = 0; j < 4; j++) {
			sprintf(buf, "%08x  ",
				readl_relaxed(cbase + i * 4 + j));
			strcat(dbg_message, buf);
		}
		pr_info("%s\n", dbg_message);
		memset(dbg_message, 0, sizeof(dbg_message));
		memset(buf, 0, sizeof(buf));
	}

	return 0;
}

#define WIN_EN(id)		\
static int win##id##_enable(struct vop_device *vop_dev, int en)	\
{ \
	spin_lock(&vop_dev->reg_lock);					\
	vop_msk_reg(vop_dev, WIN##id##_CTRL0, V_WIN##id##_EN((u64)en));	\
	vop_cfg_done(vop_dev);						\
	spin_unlock(&vop_dev->reg_lock);				\
	return 0;							\
}

WIN_EN(0);
WIN_EN(1);
WIN_EN(2);
WIN_EN(3);

/*enable/disable win directly*/
static int vop_win_direct_en(struct rk_lcdc_driver *drv,
			     int win_id, int en)
{
	struct vop_device *vop_dev =
	    container_of(drv, struct vop_device, driver);

	drv->win[win_id]->state = en;
	if (win_id == 0)
		win0_enable(vop_dev, en);
	else if (win_id == 1)
		win1_enable(vop_dev, en);
	else if (win_id == 2)
		win2_enable(vop_dev, en);
	else if (win_id == 3)
		win3_enable(vop_dev, en);
	else
		dev_err(vop_dev->dev, "invalid win number:%d\n", win_id);
	return 0;
}

#define SET_WIN_ADDR(id) \
static int set_win##id##_addr(struct vop_device *vop_dev, u32 addr) \
{							\
	spin_lock(&vop_dev->reg_lock);			\
	vop_writel(vop_dev, WIN##id##_YRGB_MST, addr);	\
	vop_msk_reg(vop_dev, WIN##id##_CTRL0, V_WIN##id##_EN(1));	\
	vop_cfg_done(vop_dev);			\
	spin_unlock(&vop_dev->reg_lock);		\
	return 0;					\
}

SET_WIN_ADDR(0);
SET_WIN_ADDR(1);
int vop_direct_set_win_addr(struct rk_lcdc_driver *dev_drv,
			    int win_id, u32 addr)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	if (win_id == 0)
		set_win0_addr(vop_dev, addr);
	else
		set_win1_addr(vop_dev, addr);

	return 0;
}

static void lcdc_read_reg_defalut_cfg(struct vop_device *vop_dev)
{
	int reg = 0;
	u32 val = 0;
	struct rk_screen *screen = vop_dev->driver.cur_screen;
	u32 h_pw_bp = screen->mode.hsync_len + screen->mode.left_margin;
	u32 V_pw_bp = screen->mode.vsync_len + screen->mode.upper_margin;
	u32 st_x, st_y;
	struct rk_lcdc_win *win0 = vop_dev->driver.win[0];

	spin_lock(&vop_dev->reg_lock);
	for (reg = 0; reg < vop_dev->len; reg += 4) {
		val = vop_readl_backup(vop_dev, reg);
		switch (reg) {
		case WIN0_ACT_INFO:
			win0->area[0].xact = (val & MASK(WIN0_ACT_WIDTH)) + 1;
			win0->area[0].yact =
				((val & MASK(WIN0_ACT_HEIGHT)) >> 16) + 1;
			break;
		case WIN0_DSP_INFO:
			win0->area[0].xsize = (val & MASK(WIN0_DSP_WIDTH)) + 1;
			win0->area[0].ysize =
			    ((val & MASK(WIN0_DSP_HEIGHT)) >> 16) + 1;
			break;
		case WIN0_DSP_ST:
			st_x = val & MASK(WIN0_DSP_XST);
			st_y = (val & MASK(WIN0_DSP_YST)) >> 16;
			win0->area[0].xpos = st_x - h_pw_bp;
			win0->area[0].ypos = st_y - V_pw_bp;
			break;
		case WIN0_CTRL0:
			win0->state = val & MASK(WIN0_EN);
			win0->area[0].fmt_cfg =
					(val & MASK(WIN0_DATA_FMT)) >> 1;
			win0->fmt_10 = (val & MASK(WIN0_FMT_10)) >> 4;
			win0->area[0].format = win0->area[0].fmt_cfg;
			break;
		case WIN0_VIR:
			win0->area[0].y_vir_stride =
					val & MASK(WIN0_VIR_STRIDE);
			win0->area[0].uv_vir_stride =
			    (val & MASK(WIN0_VIR_STRIDE_UV)) >> 16;
			if (win0->area[0].format == ARGB888)
				win0->area[0].xvir = win0->area[0].y_vir_stride;
			else if (win0->area[0].format == RGB888)
				win0->area[0].xvir =
				    win0->area[0].y_vir_stride * 4 / 3;
			else if (win0->area[0].format == RGB565)
				win0->area[0].xvir =
				    2 * win0->area[0].y_vir_stride;
			else
				win0->area[0].xvir =
				    4 * win0->area[0].y_vir_stride;
			break;
		case WIN0_YRGB_MST:
			win0->area[0].smem_start = val;
			break;
		case WIN0_CBR_MST:
			win0->area[0].cbr_start = val;
			break;
		default:
			break;
		}
	}
	spin_unlock(&vop_dev->reg_lock);
}

/********do basic init*********/
static int vop_pre_init(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	if (vop_dev->pre_init)
		return 0;
	vop_dev->hclk = devm_clk_get(vop_dev->dev, "hclk_lcdc");
	vop_dev->aclk = devm_clk_get(vop_dev->dev, "aclk_lcdc");
	vop_dev->dclk = devm_clk_get(vop_dev->dev, "dclk_lcdc");
	if (IS_ERR(vop_dev->aclk) || IS_ERR(vop_dev->dclk) ||
	    IS_ERR(vop_dev->hclk)) {
		dev_err(vop_dev->dev, "failed to get clk source\n");
		return -1;
	}
	vop_dev->hclk_noc = devm_clk_get(vop_dev->dev, "hclk_vop_noc");
	if (IS_ERR(vop_dev->hclk_noc)) {
		vop_dev->hclk_noc = NULL;
		dev_err(vop_dev->dev, "failed to get clk source\n");
	}
	vop_dev->aclk_noc = devm_clk_get(vop_dev->dev, "aclk_vop_noc");
	if (IS_ERR(vop_dev->aclk_noc)) {
		vop_dev->aclk_noc = NULL;
		dev_err(vop_dev->dev, "failed to get clk source\n");
	}
	if (!support_uboot_display())
		rk_disp_pwr_enable(dev_drv);
	vop_clk_enable(vop_dev);

	memcpy(vop_dev->regsbak, vop_dev->regs, vop_dev->len);
	/*backup reg config at uboot */
	lcdc_read_reg_defalut_cfg(vop_dev);
	#ifndef CONFIG_RK_FPGA
	/*
	 * Todo, not verified
	 *
	if (vop_dev->pwr18 == 1) {
		v = 0x00200020;
		vop_grf_writel(vop_dev->pmugrf_base,
				PMUGRF_SOC_CON0_VOP, v);
	} else {
		v = 0x00200000;
		vop_grf_writel(vop_dev->pmugrf_base,
				PMUGRF_SOC_CON0_VOP, v);
	}
	*/
	#endif
	vop_writel(vop_dev, FRC_LOWER01_0, 0x12844821);
	vop_writel(vop_dev, FRC_LOWER01_1, 0x21488412);
	vop_writel(vop_dev, FRC_LOWER10_0, 0xa55a9696);
	vop_writel(vop_dev, FRC_LOWER10_1, 0x5aa56969);
	vop_writel(vop_dev, FRC_LOWER11_0, 0xdeb77deb);
	vop_writel(vop_dev, FRC_LOWER11_1, 0xed7bb7de);

	if (!dev_drv->cabc_mode)
		vop_msk_reg(vop_dev, SYS_CTRL, V_AUTO_GATING_EN(0));
	vop_msk_reg(vop_dev, DSP_CTRL1, V_DITHER_UP_EN(1));
	vop_cfg_done(vop_dev);
	if ((dev_drv->cur_screen->refresh_mode == SCREEN_CMD_MODE) &&
	    (support_uboot_display() == 0))
		vop_msk_reg(vop_dev, SYS_CTRL, V_EDPI_WMS_MODE(1));
	vop_dev->pre_init = true;

	return 0;
}

static void vop_deint(struct vop_device *vop_dev)
{
	if (vop_dev->clk_on) {
		u64 val;

		vop_disable_irq(vop_dev);
		spin_lock(&vop_dev->reg_lock);
		vop_msk_reg(vop_dev, WIN0_CTRL0, V_WIN0_EN(0));
		vop_msk_reg(vop_dev, WIN1_CTRL0, V_WIN0_EN(0));

		val = V_WIN2_EN(0) | V_WIN2_MST0_EN(0) | V_WIN2_MST1_EN(0) |
			V_WIN2_MST2_EN(0) | V_WIN2_MST3_EN(0);
		vop_msk_reg(vop_dev, WIN2_CTRL0, val);
		vop_msk_reg(vop_dev, WIN3_CTRL0, val);
		vop_cfg_done(vop_dev);
		spin_unlock(&vop_dev->reg_lock);
		mdelay(50);
	}
}

static void vop_win_csc_mode(struct vop_device *vop_dev,
			     struct rk_lcdc_win *win,
			     int csc_mode)
{
	u64 val;

	if (win->id == VOP_WIN0) {
		val = V_WIN0_CSC_MODE(csc_mode);
		vop_msk_reg(vop_dev, WIN0_CTRL0, val);
	} else if (win->id == VOP_WIN1) {
		val = V_WIN1_CSC_MODE(csc_mode);
		vop_msk_reg(vop_dev, WIN1_CTRL0, val);
	} else {
		val = V_HWC_CSC_MODE(csc_mode);
		vop_msk_reg(vop_dev, HWC_CTRL0, val);
	}
}

static int rk3399_vop_win_csc_cfg(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int output_color = dev_drv->output_color;
	int i;

	for (i = 0; i < dev_drv->lcdc_win_num && i < 4; i++) {
		struct rk_lcdc_win *win = dev_drv->win[i];
		int shift = i * 8;
		u64 val = V_WIN0_YUV2YUV_EN(0) | V_WIN0_YUV2YUV_R2Y_EN(0) |
				V_WIN0_YUV2YUV_Y2R_EN(0);

		if (!win->state)
			continue;
		if (output_color == COLOR_RGB &&
		    !(IS_YUV(win->area[0].fmt_cfg) || win->area[0].yuyv_fmt))
			goto post;

		if (output_color == COLOR_RGB) {
			val |= V_WIN0_YUV2YUV_Y2R_EN(1);
			if (win->colorspace == CSC_BT601) {
				/*
				 * Win Y2Y moudle always use 10bit mode.
				 */
				LOAD_CSC(vop_dev, Y2R,
					 csc_y2r_bt601_full_10, i);
			} else if (win->colorspace == CSC_BT709) {
				LOAD_CSC(vop_dev, Y2R,
					 csc_y2r_bt709_full_10, i);
			} else if (win->colorspace == CSC_BT2020) {
				val |= V_WIN0_YUV2YUV_EN(1);
				LOAD_CSC(vop_dev, Y2R, csc_y2r_bt2020, i);
				LOAD_CSC(vop_dev, R2R, csc_r2r_bt2020to709, i);
			}
		} else if (output_color == COLOR_YCBCR ||
				output_color == COLOR_YCBCR_BT709) {
			if (!(IS_YUV(win->area[0].fmt_cfg) ||
			      win->area[0].yuyv_fmt)) {
				val |= V_WIN0_YUV2YUV_R2Y_EN(1);
				if ((win->id == 0) || (win->id == 1))
					LOAD_CSC(vop_dev, R2Y, csc_r2y_bt709_full_10, i);
				else
					val |= V_WIN0_YUV2YUV_R2Y_MODE(VOP_R2Y_CSC_BT709);

			} else if (win->colorspace == CSC_BT2020) {
				val |= V_WIN0_YUV2YUV_EN(1) |
					V_WIN0_YUV2YUV_Y2R_EN(1) |
					V_WIN0_YUV2YUV_R2Y_EN(1);
				LOAD_CSC(vop_dev, Y2R, csc_y2r_bt2020, i);
				LOAD_CSC(vop_dev, R2R, csc_r2r_bt2020to709, i);
				LOAD_CSC(vop_dev, R2Y, csc_r2y_bt709_full_10, i);
			}
		} else if (output_color == COLOR_YCBCR_BT2020) {
			if (!(IS_YUV(win->area[0].fmt_cfg) ||
			      win->area[0].yuyv_fmt)) {
				LOAD_CSC(vop_dev, R2R, csc_r2r_bt709to2020, i);
				val |= V_WIN0_YUV2YUV_R2Y_EN(1) |
					V_WIN0_YUV2YUV_EN(1);
				if ((win->id == 0) || (win->id == 1)) {
					LOAD_CSC(vop_dev, R2Y, csc_r2y_bt2020, i);
				} else {
					val |= V_WIN0_YUV2YUV_R2Y_MODE(VOP_R2Y_CSC_BT2020);
				}
			} else if (win->colorspace == CSC_BT601 ||
					win->colorspace == CSC_BT709) {
				val |= V_WIN0_YUV2YUV_Y2R_EN(1) |
					V_WIN0_YUV2YUV_R2Y_EN(1) |
					V_WIN0_YUV2YUV_EN(1);
				LOAD_CSC(vop_dev, Y2R, csc_y2r_bt709_full_10, i);
				LOAD_CSC(vop_dev, R2R, csc_r2r_bt709to2020, i);
				LOAD_CSC(vop_dev, R2Y, csc_r2y_bt2020, i);
			}
		}
post:
		vop_msk_reg(vop_dev, YUV2YUV_WIN, val << shift);
	}

	return output_color;
}

/*
 * colorspace path:
 *      Input        Win csc            Post csc              Output
 * 1. YUV(2020)  --> bypass   ---+ Y2R->2020To709->R2Y --> YUV_OUTPUT(601/709)
 *    RGB        --> R2Y(709) __/
 *
 * 2. YUV(2020)  --> bypass   ---+       bypass        --> YUV_OUTPUT(2020)
 *    RGB        --> R2Y(709) __/
 *
 * 3. YUV(2020)  --> bypass   ---+    Y2R->2020To709   --> RGB_OUTPUT(709)
 *    RGB        --> R2Y(709) __/
 *
 * 4. YUV(601/709)-> bypass   ---+ Y2R->709To2020->R2Y --> YUV_OUTPUT(2020)
 *    RGB        --> R2Y(709) __/
 *
 * 5. YUV(601/709)-> bypass   ---+       bypass        --> YUV_OUTPUT(709)
 *    RGB        --> R2Y(709) __/
 *
 * 6. YUV(601/709)-> bypass   ---+       bypass        --> YUV_OUTPUT(601)
 *    RGB        --> R2Y(601) __/
 *
 * 7. YUV(601)   --> Y2R(601/mpeg)-+     bypass        --> RGB_OUTPUT(709)
 *    RGB        --> bypass   ____/
 *
 * 8. YUV(709)   --> Y2R(709/hd) --+     bypass        --> RGB_OUTPUT(709)
 *    RGB        --> bypass   ____/
 *
 * 9. RGB        --> bypass   --->    709To2020->R2Y   --> YUV_OUTPUT(2020)
 *
 * 10. RGB       --> R2Y(709) --->      bypass        --> YUV_OUTPUT(709)
 *
 * 11. RGB       --> R2Y(601) --->       bypass        --> YUV_OUTPUT(601)
 *
 * 12. RGB       --> bypass   --->       bypass        --> RGB_OUTPUT(709)
 */
static int rk3228_vop_win_csc_cfg(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win;
	int output_color = dev_drv->output_color;
	int win_csc = COLOR_RGB;
	int r2y_mode = VOP_R2Y_CSC_BT709;
	int i;

	for (i = 0; i < dev_drv->lcdc_win_num; i++) {
		win = dev_drv->win[i];
		if (!win->state)
			continue;

		if (IS_YUV(win->area[0].fmt_cfg)) {
			if (win->colorspace == CSC_BT2020 &&
			    win_csc < COLOR_YCBCR_BT2020) {
				r2y_mode = VOP_R2Y_CSC_BT709;
				win_csc = COLOR_YCBCR_BT2020;
			}

			if (win->colorspace == CSC_BT709 &&
			    win_csc < COLOR_YCBCR_BT709) {
				r2y_mode = VOP_R2Y_CSC_BT709;
				win_csc = COLOR_YCBCR_BT709;
			}

			if (win->colorspace == CSC_BT601 &&
			    win_csc < COLOR_YCBCR) {
				r2y_mode = VOP_R2Y_CSC_BT709;
				win_csc = COLOR_YCBCR;
			}
		}
	}

	if (win_csc == COLOR_RGB) {
		if (output_color == COLOR_YCBCR_BT709) {
			r2y_mode = VOP_R2Y_CSC_BT709;
			win_csc = COLOR_YCBCR_BT709;
		} else if (output_color == COLOR_YCBCR) {
			r2y_mode = VOP_R2Y_CSC_BT601;
			win_csc = COLOR_YCBCR;
		}
	}

	for (i = 0; i < dev_drv->lcdc_win_num; i++) {
		win = dev_drv->win[i];
		if (!win->state)
			continue;

		if (win_csc != COLOR_RGB && !IS_YUV(win->area[0].fmt_cfg))
			vop_win_csc_mode(vop_dev, win, r2y_mode);

		if (IS_YUV(win->area[0].fmt_cfg)) {
			if (win_csc == COLOR_YCBCR)
				vop_win_csc_mode(vop_dev, win,
						 VOP_Y2R_CSC_MPEG);
			else if (win_csc == COLOR_YCBCR_BT709)
				vop_win_csc_mode(vop_dev, win, VOP_Y2R_CSC_HD);
		}
	}

	return win_csc;
}

static int vop_post_csc_cfg(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int output_color = dev_drv->output_color;
	int win_csc = 0, overlay_mode = 0;
	u64 val;

	if (VOP_CHIP(vop_dev) == VOP_RK322X) {
		win_csc = rk3228_vop_win_csc_cfg(dev_drv);
	} else if (VOP_CHIP(vop_dev) == VOP_RK3399) {
		win_csc = rk3399_vop_win_csc_cfg(dev_drv);

		/*
		 * RK3399 not support post csc config.
		 */
		goto done;
	}

	val = V_YUV2YUV_POST_Y2R_EN(0) | V_YUV2YUV_POST_EN(0) |
		V_YUV2YUV_POST_R2Y_EN(0);
	/* Y2R */
	if (win_csc == COLOR_YCBCR && output_color == COLOR_YCBCR_BT2020) {
		val |= V_YUV2YUV_POST_Y2R_EN(1);
		vop_load_csc_table(vop_dev, POST_YUV2YUV_Y2R_COE,
				   csc_y2r_bt709_full);
	}
	if (win_csc == COLOR_YCBCR_BT2020 &&
	    output_color != COLOR_YCBCR_BT2020) {
		val |= V_YUV2YUV_POST_Y2R_EN(1);
		vop_load_csc_table(vop_dev, POST_YUV2YUV_Y2R_COE,
				   csc_y2r_bt2020);
	}

	/* R2R */
	if ((win_csc == COLOR_YCBCR ||
	     win_csc == COLOR_YCBCR_BT709 ||
	     win_csc == COLOR_RGB) && output_color == COLOR_YCBCR_BT2020) {
		val |= V_YUV2YUV_POST_EN(1);
		vop_load_csc_table(vop_dev, POST_YUV2YUV_3x3_COE,
				   csc_r2r_bt709to2020);
	}
	if (win_csc == COLOR_YCBCR_BT2020 &&
	    (output_color == COLOR_YCBCR ||
	     output_color == COLOR_YCBCR_BT709 ||
	     output_color == COLOR_RGB)) {
		val |= V_YUV2YUV_POST_EN(1);
		vop_load_csc_table(vop_dev, POST_YUV2YUV_3x3_COE,
				   csc_r2r_bt2020to709);
	}

	/* Y2R */
	if (output_color != COLOR_RGB) {
		val |= V_YUV2YUV_POST_R2Y_EN(1);

		if (output_color == COLOR_YCBCR_BT2020)
			vop_load_csc_table(vop_dev, POST_YUV2YUV_R2Y_COE,
					   csc_r2y_bt2020);
		else
			vop_load_csc_table(vop_dev, POST_YUV2YUV_R2Y_COE,
					   csc_r2y_bt709_full);
	}

	DBG(1, "win_csc=%d output_color=%d val=%llx\n",
	    win_csc, output_color, val);
	vop_msk_reg(vop_dev, YUV2YUV_POST, val);
done:
	overlay_mode = (win_csc != COLOR_RGB) ? VOP_YUV_DOMAIN : VOP_RGB_DOMAIN;
	vop_msk_reg(vop_dev, SYS_CTRL, V_OVERLAY_MODE(overlay_mode));

	return 0;
}

static int vop_post_cfg(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
	u16 x_res = screen->mode.xres;
	u16 y_res = screen->mode.yres;
	u64 val;
	u16 h_total, v_total;
	u16 post_hsd_en, post_vsd_en;
	u16 post_dsp_hact_st, post_dsp_hact_end;
	u16 post_dsp_vact_st, post_dsp_vact_end;
	u16 post_dsp_vact_st_f1, post_dsp_vact_end_f1;
	u16 post_h_fac, post_v_fac;

	screen->post_dsp_stx = x_res * (100 - dev_drv->overscan.left) / 200;
	screen->post_dsp_sty = y_res * (100 - dev_drv->overscan.top) / 200;
	screen->post_xsize = x_res *
	    (dev_drv->overscan.left + dev_drv->overscan.right) / 200;
	screen->post_ysize = y_res *
	    (dev_drv->overscan.top + dev_drv->overscan.bottom) / 200;

	h_total = screen->mode.hsync_len + screen->mode.left_margin +
	    x_res + screen->mode.right_margin;
	v_total = screen->mode.vsync_len + screen->mode.upper_margin +
	    y_res + screen->mode.lower_margin;

	if (screen->post_dsp_stx + screen->post_xsize > x_res) {
		dev_warn(vop_dev->dev, "post:stx[%d]+xsize[%d]>x_res[%d]\n",
			 screen->post_dsp_stx, screen->post_xsize, x_res);
		screen->post_dsp_stx = x_res - screen->post_xsize;
	}
	if (screen->x_mirror == 0) {
		post_dsp_hact_st = screen->post_dsp_stx +
		    screen->mode.hsync_len + screen->mode.left_margin;
		post_dsp_hact_end = post_dsp_hact_st + screen->post_xsize;
	} else {
		post_dsp_hact_end = h_total - screen->mode.right_margin -
		    screen->post_dsp_stx;
		post_dsp_hact_st = post_dsp_hact_end - screen->post_xsize;
	}
	if ((screen->post_xsize < x_res) && (screen->post_xsize != 0)) {
		post_hsd_en = 1;
		post_h_fac =
		    GET_SCALE_FACTOR_BILI_DN(x_res, screen->post_xsize);
	} else {
		post_hsd_en = 0;
		post_h_fac = 0x1000;
	}

	if (screen->post_dsp_sty + screen->post_ysize > y_res) {
		dev_warn(vop_dev->dev, "post:sty[%d]+ysize[%d]> y_res[%d]\n",
			 screen->post_dsp_sty, screen->post_ysize, y_res);
		screen->post_dsp_sty = y_res - screen->post_ysize;
	}

	if ((screen->post_ysize < y_res) && (screen->post_ysize != 0)) {
		post_vsd_en = 1;
		post_v_fac = GET_SCALE_FACTOR_BILI_DN(y_res,
						      screen->post_ysize);
	} else {
		post_vsd_en = 0;
		post_v_fac = 0x1000;
	}

	if (screen->mode.vmode & FB_VMODE_INTERLACED) {
		post_dsp_vact_st = screen->post_dsp_sty / 2 +
					screen->mode.vsync_len +
					screen->mode.upper_margin;
		post_dsp_vact_end = post_dsp_vact_st +
					screen->post_ysize / 2;

		post_dsp_vact_st_f1 = screen->mode.vsync_len +
					screen->mode.upper_margin +
					y_res / 2 +
					screen->mode.lower_margin +
					screen->mode.vsync_len +
					screen->mode.upper_margin +
					screen->post_dsp_sty / 2 +
					1;
		post_dsp_vact_end_f1 = post_dsp_vact_st_f1 +
					screen->post_ysize / 2;
	} else {
		if (screen->y_mirror == 0) {
			post_dsp_vact_st = screen->post_dsp_sty +
			    screen->mode.vsync_len +
			    screen->mode.upper_margin;
			post_dsp_vact_end = post_dsp_vact_st +
				screen->post_ysize;
		} else {
			post_dsp_vact_end = v_total -
				screen->mode.lower_margin -
			    screen->post_dsp_sty;
			post_dsp_vact_st = post_dsp_vact_end -
				screen->post_ysize;
		}
		post_dsp_vact_st_f1 = 0;
		post_dsp_vact_end_f1 = 0;
	}
	DBG(1, "post:xsize=%d,ysize=%d,xpos=%d",
	    screen->post_xsize, screen->post_ysize, screen->xpos);
	DBG(1, ",ypos=%d,hsd_en=%d,h_fac=%d,vsd_en=%d,v_fac=%d\n",
	    screen->ypos, post_hsd_en, post_h_fac, post_vsd_en, post_v_fac);
	val = V_DSP_HACT_END_POST(post_dsp_hact_end) |
	    V_DSP_HACT_ST_POST(post_dsp_hact_st);
	vop_msk_reg(vop_dev, POST_DSP_HACT_INFO, val);

	val = V_DSP_VACT_END_POST(post_dsp_vact_end) |
	    V_DSP_VACT_ST_POST(post_dsp_vact_st);
	vop_msk_reg(vop_dev, POST_DSP_VACT_INFO, val);

	val = V_POST_HS_FACTOR_YRGB(post_h_fac) |
	    V_POST_VS_FACTOR_YRGB(post_v_fac);
	vop_msk_reg(vop_dev, POST_SCL_FACTOR_YRGB, val);
	val = V_DSP_VACT_END_POST(post_dsp_vact_end_f1) |
	    V_DSP_VACT_ST_POST(post_dsp_vact_st_f1);
	vop_msk_reg(vop_dev, POST_DSP_VACT_INFO_F1, val);
	val = V_POST_HOR_SD_EN(post_hsd_en) | V_POST_VER_SD_EN(post_vsd_en);
	vop_msk_reg(vop_dev, POST_SCL_CTRL, val);

	vop_post_csc_cfg(dev_drv);

	return 0;
}

static int vop_clr_key_cfg(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win;
	u32 colorkey_r, colorkey_g, colorkey_b;
	int i, key_val;

	for (i = 0; i < dev_drv->lcdc_win_num; i++) {
		win = dev_drv->win[i];
		key_val = win->color_key_val;
		colorkey_r = (key_val & 0xff) << 2;
		colorkey_g = ((key_val >> 8) & 0xff) << 12;
		colorkey_b = ((key_val >> 16) & 0xff) << 22;
		/* color key dither 565/888->aaa */
		key_val = colorkey_r | colorkey_g | colorkey_b;
		switch (i) {
		case 0:
			vop_writel(vop_dev, WIN0_COLOR_KEY, key_val);
			break;
		case 1:
			vop_writel(vop_dev, WIN1_COLOR_KEY, key_val);
			break;
		case 2:
			vop_writel(vop_dev, WIN2_COLOR_KEY, key_val);
			break;
		case 3:
			vop_writel(vop_dev, WIN3_COLOR_KEY, key_val);
			break;
		default:
			pr_info("%s:un support win num:%d\n",
				__func__, i);
			break;
		}
	}
	return 0;
}

static int vop_alpha_cfg(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = dev_drv->win[win_id];
	struct alpha_config alpha_config;
	u64 val;
	int ppixel_alpha = 0, global_alpha = 0, i;
	u32 src_alpha_ctl = 0, dst_alpha_ctl = 0;
	int alpha_en = 1;

	memset(&alpha_config, 0, sizeof(struct alpha_config));
	for (i = 0; i < win->area_num; i++) {
		ppixel_alpha |= ((win->area[i].format == ARGB888) ||
				 (win->area[i].format == FBDC_ARGB_888) ||
				 (win->area[i].format == FBDC_ABGR_888) ||
				 (win->area[i].format == ABGR888)) ? 1 : 0;
	}

	global_alpha = (win->g_alpha_val == 0) ? 0 : 1;

	for (i = 0; i < dev_drv->lcdc_win_num; i++) {
		if (!dev_drv->win[i]->state)
			continue;
		if (win->z_order > dev_drv->win[i]->z_order)
			break;
	}

	/*
	 * The bottom layer not support ppixel_alpha mode.
	 */
	if (i == dev_drv->lcdc_win_num)
		ppixel_alpha = 0;
	alpha_config.src_global_alpha_val = win->g_alpha_val;
	win->alpha_mode = AB_SRC_OVER;

	switch (win->alpha_mode) {
	case AB_USER_DEFINE:
		break;
	case AB_CLEAR:
		alpha_config.src_factor_mode = AA_ZERO;
		alpha_config.dst_factor_mode = AA_ZERO;
		break;
	case AB_SRC:
		alpha_config.src_factor_mode = AA_ONE;
		alpha_config.dst_factor_mode = AA_ZERO;
		break;
	case AB_DST:
		alpha_config.src_factor_mode = AA_ZERO;
		alpha_config.dst_factor_mode = AA_ONE;
		break;
	case AB_SRC_OVER:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		if (global_alpha)
			alpha_config.src_factor_mode = AA_SRC_GLOBAL;
		else
			alpha_config.src_factor_mode = AA_ONE;
		alpha_config.dst_factor_mode = AA_SRC_INVERSE;
		break;
	case AB_DST_OVER:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC_INVERSE;
		alpha_config.dst_factor_mode = AA_ONE;
		break;
	case AB_SRC_IN:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC;
		alpha_config.dst_factor_mode = AA_ZERO;
		break;
	case AB_DST_IN:
		alpha_config.src_factor_mode = AA_ZERO;
		alpha_config.dst_factor_mode = AA_SRC;
		break;
	case AB_SRC_OUT:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC_INVERSE;
		alpha_config.dst_factor_mode = AA_ZERO;
		break;
	case AB_DST_OUT:
		alpha_config.src_factor_mode = AA_ZERO;
		alpha_config.dst_factor_mode = AA_SRC_INVERSE;
		break;
	case AB_SRC_ATOP:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC;
		alpha_config.dst_factor_mode = AA_SRC_INVERSE;
		break;
	case AB_DST_ATOP:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC_INVERSE;
		alpha_config.dst_factor_mode = AA_SRC;
		break;
	case XOR:
		alpha_config.src_color_mode = AA_SRC_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC_INVERSE;
		alpha_config.dst_factor_mode = AA_SRC_INVERSE;
		break;
	case AB_SRC_OVER_GLOBAL:
		alpha_config.src_global_alpha_mode = AA_PER_PIX_GLOBAL;
		alpha_config.src_color_mode = AA_SRC_NO_PRE_MUL;
		alpha_config.src_factor_mode = AA_SRC_GLOBAL;
		alpha_config.dst_factor_mode = AA_SRC_INVERSE;
		break;
	default:
		pr_err("alpha mode error\n");
		break;
	}
	if ((ppixel_alpha == 1) && (global_alpha == 1))
		alpha_config.src_global_alpha_mode = AA_PER_PIX_GLOBAL;
	else if (ppixel_alpha == 1)
		alpha_config.src_global_alpha_mode = AA_PER_PIX;
	else if (global_alpha == 1)
		alpha_config.src_global_alpha_mode = AA_GLOBAL;
	else
		alpha_en = 0;
	alpha_config.src_alpha_mode = AA_STRAIGHT;
	alpha_config.src_alpha_cal_m0 = AA_NO_SAT;

	switch (win_id) {
	case 0:
		src_alpha_ctl = 0x60;
		dst_alpha_ctl = 0x64;
		break;
	case 1:
		src_alpha_ctl = 0xa0;
		dst_alpha_ctl = 0xa4;
		break;
	case 2:
		src_alpha_ctl = 0xdc;
		dst_alpha_ctl = 0xec;
		break;
	case 3:
		src_alpha_ctl = 0x12c;
		dst_alpha_ctl = 0x13c;
		break;
	case 4:
		src_alpha_ctl = 0x160;
		dst_alpha_ctl = 0x164;
		break;
	}
	val = V_WIN0_DST_FACTOR_MODE(alpha_config.dst_factor_mode);
	vop_msk_reg(vop_dev, dst_alpha_ctl, val);
	val = V_WIN0_SRC_ALPHA_EN(alpha_en) |
	    V_WIN0_SRC_COLOR_MODE(alpha_config.src_color_mode) |
	    V_WIN0_SRC_ALPHA_MODE(alpha_config.src_alpha_mode) |
	    V_WIN0_SRC_BLEND_MODE(alpha_config.src_global_alpha_mode) |
	    V_WIN0_SRC_ALPHA_CAL_MODE(alpha_config.src_alpha_cal_m0) |
	    V_WIN0_SRC_FACTOR_MODE(alpha_config.src_factor_mode) |
	    V_WIN0_SRC_GLOBAL_ALPHA(alpha_config.src_global_alpha_val);

	vop_msk_reg(vop_dev, src_alpha_ctl, val);

	return 0;
}

static int vop_axi_gather_cfg(struct vop_device *vop_dev,
			      struct rk_lcdc_win *win)
{
	u64 val;
	u16 yrgb_gather_num = 3;
	u16 cbcr_gather_num = 1;

	switch (win->area[0].format) {
	case XRGB888:
	case ARGB888:
	case XBGR888:
	case ABGR888:
	case FBDC_ARGB_888:
	case FBDC_RGBX_888:
	case FBDC_ABGR_888:
		yrgb_gather_num = 3;
		break;
	case RGB888:
	case BGR888:
	case RGB565:
	case BGR565:
	case FBDC_RGB_565:
		yrgb_gather_num = 2;
		break;
	case YUV444:
	case YUV422:
	case YUV420:
	case YUV420_A:
	case YUV422_A:
	case YUV444_A:
	case YUV420_NV21:
	case YUYV420:
	case UYVY420:
		yrgb_gather_num = 1;
		cbcr_gather_num = 2;
		break;
	case YUYV422:
	case UYVY422:
		yrgb_gather_num = 2;
		cbcr_gather_num = 2;
		break;
	default:
		dev_err(vop_dev->driver.dev, "%s:un supported format[%d]\n",
			__func__, win->area[0].format);
		return -EINVAL;
	}

	if ((win->id == VOP_WIN0) || (win->id == VOP_WIN1)) {
		val = V_WIN0_YRGB_AXI_GATHER_EN(1) |
			V_WIN0_CBR_AXI_GATHER_EN(1) |
			V_WIN0_YRGB_AXI_GATHER_NUM(yrgb_gather_num) |
			V_WIN0_CBR_AXI_GATHER_NUM(cbcr_gather_num);
		vop_msk_reg(vop_dev, WIN0_CTRL1 + (win->id * 0x40), val);
	} else if ((win->id == VOP_WIN2) || (win->id == VOP_WIN3)) {
		val = V_WIN2_AXI_GATHER_EN(1) |
			V_WIN2_AXI_GATHER_NUM(yrgb_gather_num);
		vop_msk_reg(vop_dev, WIN2_CTRL1 + ((win->id - 2) * 0x50), val);
	} else if (win->id == VOP_HWC) {
		val = V_HWC_AXI_GATHER_EN(1) |
			V_HWC_AXI_GATHER_NUM(yrgb_gather_num);
		vop_msk_reg(vop_dev, HWC_CTRL1, val);
	}
	return 0;
}

static int vop_fbdc_reg_update(struct vop_device *vop_dev, int win_id)
{
	struct rk_lcdc_win *win = vop_dev->driver.win[win_id];
	u64 val;

	val = V_VOP_FBDC_WIN_SEL(win_id) |
		V_AFBCD_HREG_PIXEL_PACKING_FMT(win->area[0].fbdc_fmt_cfg) |
		V_AFBCD_HREG_BLOCK_SPLIT(win->area[0].fbdc_cor_en);
	vop_msk_reg(vop_dev, AFBCD0_CTRL, val);

	val = V_AFBCD_HREG_PIC_WIDTH(win->area[0].fbdc_mb_width - 1) |
		V_AFBCD_HREG_PIC_HEIGHT(win->area[0].fbdc_mb_height - 1);
	vop_msk_reg(vop_dev, AFBCD0_PIC_SIZE, val);

	return 0;
}

static int vop_init_fbdc_config(struct vop_device *vop_dev, int win_id)
{
	struct rk_lcdc_driver *vop_drv = &vop_dev->driver;
	struct rk_lcdc_win *win = vop_drv->win[win_id];
	struct rk_screen *screen = vop_drv->cur_screen;

	if (screen->mode.flag & FB_VMODE_INTERLACED) {
		dev_err(vop_dev->dev, "unsupport fbdc+interlace!\n");
		return 0;
	}

	if (VOP_CHIP(vop_dev) != VOP_RK3399) {
		pr_err("soc: 0x%08x not support FBDC\n", VOP_CHIP(vop_dev));
		return 0;
	}

	win->area[0].fbdc_mb_width = win->area[0].xvir;
	win->area[0].fbdc_mb_height = win->area[0].yact;
	win->area[0].fbdc_cor_en = 0; /* hreg_block_split */
	win->area[0].fbdc_fmt_cfg |= AFBDC_YUV_COLOR_TRANSFORM << 4;

	return 0;
}

static int vop_win_0_1_reg_update(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = dev_drv->win[win_id];
	u64 val;
	u32 off;
	int format;
	struct rk_win_property *win_property =
				&dev_drv->win[win_id]->property;

	off = win_id * 0x40;

	if (win->state == 1) {
		if (!(win_property->feature & SUPPORT_HW_EXIST)) {
			pr_err("vop[%d] win[%d] hardware unsupport\n",
			       vop_dev->id, win_id);
			return 0;
		}
		vop_axi_gather_cfg(vop_dev, win);
		if (win->area[0].fbdc_en)
			vop_fbdc_reg_update(vop_dev, win_id);
		/*
		 * rk322x have a bug on windows 0 and 1:
		 *
		 * When switch win format from RGB to YUV, would flash
		 * some green lines on the top of the windows.
		 *
		 * Use bg_en show one blank frame to skip the error frame.
		 */
		if (IS_YUV(win->area[0].fmt_cfg)) {
			val = vop_readl(vop_dev, WIN0_CTRL0);
			format = (val & MASK(WIN0_DATA_FMT)) >> 1;

			if (!IS_YUV(format)) {
				if (dev_drv->overlay_mode == VOP_YUV_DOMAIN) {
					val = V_WIN0_DSP_BG_RED(0x200) |
						V_WIN0_DSP_BG_GREEN(0x40) |
						V_WIN0_DSP_BG_BLUE(0x200) |
						V_WIN0_BG_EN(1);
					vop_msk_reg(vop_dev, WIN0_DSP_BG + off,
						    val);
				} else {
					val = V_WIN0_DSP_BG_RED(0) |
						V_WIN0_DSP_BG_GREEN(0) |
						V_WIN0_DSP_BG_BLUE(0) |
						V_WIN0_BG_EN(1);
					vop_msk_reg(vop_dev, WIN0_DSP_BG + off,
						    val);
				}
			} else {
				val = V_WIN0_BG_EN(0);
				vop_msk_reg(vop_dev, WIN0_DSP_BG + off, val);
			}
		} else {
			val = V_WIN0_BG_EN(0);
			vop_msk_reg(vop_dev, WIN0_DSP_BG + off, val);
		}

		val = V_WIN0_EN(win->state) |
			V_WIN0_DATA_FMT(win->area[0].fmt_cfg) |
			V_WIN0_FMT_10(win->fmt_10) |
			V_WIN0_LB_MODE(win->win_lb_mode) |
			V_WIN0_RB_SWAP(win->area[0].swap_rb) |
			V_WIN0_X_MIR_EN(win->xmirror) |
			V_WIN0_Y_MIR_EN(win->ymirror) |
			V_WIN0_UV_SWAP(win->area[0].swap_uv);
		if (VOP_CHIP(vop_dev) == VOP_RK3399)
			val |= V_WIN0_YUYV(win->area[0].yuyv_fmt);
		vop_msk_reg(vop_dev, WIN0_CTRL0 + off, val);
		val = V_WIN0_BIC_COE_SEL(win->bic_coe_el) |
		    V_WIN0_VSD_YRGB_GT4(win->vsd_yrgb_gt4) |
		    V_WIN0_VSD_YRGB_GT2(win->vsd_yrgb_gt2) |
		    V_WIN0_VSD_CBR_GT4(win->vsd_cbr_gt4) |
		    V_WIN0_VSD_CBR_GT2(win->vsd_cbr_gt2) |
		    V_WIN0_YRGB_HOR_SCL_MODE(win->yrgb_hor_scl_mode) |
		    V_WIN0_YRGB_VER_SCL_MODE(win->yrgb_ver_scl_mode) |
		    V_WIN0_YRGB_HSD_MODE(win->yrgb_hsd_mode) |
		    V_WIN0_YRGB_VSU_MODE(win->yrgb_vsu_mode) |
		    V_WIN0_YRGB_VSD_MODE(win->yrgb_vsd_mode) |
		    V_WIN0_CBR_HOR_SCL_MODE(win->cbr_hor_scl_mode) |
		    V_WIN0_CBR_VER_SCL_MODE(win->cbr_ver_scl_mode) |
		    V_WIN0_CBR_HSD_MODE(win->cbr_hsd_mode) |
		    V_WIN0_CBR_VSU_MODE(win->cbr_vsu_mode) |
		    V_WIN0_CBR_VSD_MODE(win->cbr_vsd_mode);
		vop_msk_reg(vop_dev, WIN0_CTRL1 + off, val);
		val = V_WIN0_VIR_STRIDE(win->area[0].y_vir_stride) |
		    V_WIN0_VIR_STRIDE_UV(win->area[0].uv_vir_stride);
		vop_writel(vop_dev, WIN0_VIR + off, val);
		val = V_WIN0_ACT_WIDTH(win->area[0].xact - 1) |
		    V_WIN0_ACT_HEIGHT(win->area[0].yact - 1);
		vop_writel(vop_dev, WIN0_ACT_INFO + off, val);

		val = V_WIN0_DSP_WIDTH(win->area[0].xsize - 1) |
		    V_WIN0_DSP_HEIGHT(win->area[0].ysize - 1);
		vop_writel(vop_dev, WIN0_DSP_INFO + off, val);

		val = V_WIN0_DSP_XST(win->area[0].dsp_stx) |
		    V_WIN0_DSP_YST(win->area[0].dsp_sty);
		vop_writel(vop_dev, WIN0_DSP_ST + off, val);

		val = V_WIN0_HS_FACTOR_YRGB(win->scale_yrgb_x) |
		    V_WIN0_VS_FACTOR_YRGB(win->scale_yrgb_y);
		vop_writel(vop_dev, WIN0_SCL_FACTOR_YRGB + off, val);

		val = V_WIN0_HS_FACTOR_CBR(win->scale_cbcr_x) |
		    V_WIN0_VS_FACTOR_CBR(win->scale_cbcr_y);
		vop_writel(vop_dev, WIN0_SCL_FACTOR_CBR + off, val);
	} else {
		val = V_WIN0_EN(win->state);
		vop_msk_reg(vop_dev, WIN0_CTRL0 + off, val);
	}

	return 0;
}

static int area_xst(struct rk_lcdc_win *win, int area_num)
{
	struct rk_lcdc_win_area area_temp;
	int i, j;

	for (i = 0; i < area_num; i++) {
		for (j = i + 1; j < area_num; j++) {
			if (win->area[i].dsp_stx >  win->area[j].dsp_stx) {
				memcpy(&area_temp, &win->area[i],
				       sizeof(struct rk_lcdc_win_area));
				memcpy(&win->area[i], &win->area[j],
				       sizeof(struct rk_lcdc_win_area));
				memcpy(&win->area[j], &area_temp,
				       sizeof(struct rk_lcdc_win_area));
			}
		}
	}

	return 0;
}

static int vop_win_2_3_reg_update(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = dev_drv->win[win_id];
	unsigned int off;
	u64 val;
	struct rk_win_property *win_property =
				&dev_drv->win[win_id]->property;

	off = (win_id - 2) * 0x50;
	area_xst(win, win->area_num);

	if (win->state == 1) {
		if (!(win_property->feature & SUPPORT_HW_EXIST)) {
			pr_err("vop[%d] win[%d] hardware unsupport\n",
			       vop_dev->id, win_id);
			return 0;
		}
		vop_axi_gather_cfg(vop_dev, win);
		if (win->area[0].fbdc_en)
			vop_fbdc_reg_update(vop_dev, win_id);
		val = V_WIN2_EN(1) | V_WIN1_CSC_MODE(win->csc_mode);
		vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
		/* area 0 */
		if (win->area[0].state == 1) {
			val = V_WIN2_MST0_EN(win->area[0].state) |
			    V_WIN2_DATA_FMT0(win->area[0].fmt_cfg) |
			    V_WIN2_RB_SWAP0(win->area[0].swap_rb);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);

			val = V_WIN2_VIR_STRIDE0(win->area[0].y_vir_stride);
			vop_msk_reg(vop_dev, WIN2_VIR0_1 + off, val);

			val = V_WIN2_DSP_WIDTH0(win->area[0].xsize - 1) |
			    V_WIN2_DSP_HEIGHT0(win->area[0].ysize - 1);
			vop_writel(vop_dev, WIN2_DSP_INFO0 + off, val);
			val = V_WIN2_DSP_XST0(win->area[0].dsp_stx) |
			    V_WIN2_DSP_YST0(win->area[0].dsp_sty);
			vop_writel(vop_dev, WIN2_DSP_ST0 + off, val);
		} else {
			val = V_WIN2_MST0_EN(0);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
		}
		/* area 1 */
		if (win->area[1].state == 1) {
			val = V_WIN2_MST1_EN(win->area[1].state) |
			    V_WIN2_DATA_FMT1(win->area[1].fmt_cfg) |
			    V_WIN2_RB_SWAP1(win->area[1].swap_rb);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);

			val = V_WIN2_VIR_STRIDE1(win->area[1].y_vir_stride);
			vop_msk_reg(vop_dev, WIN2_VIR0_1 + off, val);

			val = V_WIN2_DSP_WIDTH1(win->area[1].xsize - 1) |
			    V_WIN2_DSP_HEIGHT1(win->area[1].ysize - 1);
			vop_writel(vop_dev, WIN2_DSP_INFO1 + off, val);
			val = V_WIN2_DSP_XST1(win->area[1].dsp_stx) |
			    V_WIN2_DSP_YST1(win->area[1].dsp_sty);
			vop_writel(vop_dev, WIN2_DSP_ST1 + off, val);
		} else {
			val = V_WIN2_MST1_EN(0);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
		}
		/* area 2 */
		if (win->area[2].state == 1) {
			val = V_WIN2_MST2_EN(win->area[2].state) |
			    V_WIN2_DATA_FMT2(win->area[2].fmt_cfg) |
			    V_WIN2_RB_SWAP2(win->area[2].swap_rb);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);

			val = V_WIN2_VIR_STRIDE2(win->area[2].y_vir_stride);
			vop_msk_reg(vop_dev, WIN2_VIR2_3 + off, val);

			val = V_WIN2_DSP_WIDTH2(win->area[2].xsize - 1) |
			    V_WIN2_DSP_HEIGHT2(win->area[2].ysize - 1);
			vop_writel(vop_dev, WIN2_DSP_INFO2 + off, val);
			val = V_WIN2_DSP_XST2(win->area[2].dsp_stx) |
			    V_WIN2_DSP_YST2(win->area[2].dsp_sty);
			vop_writel(vop_dev, WIN2_DSP_ST2 + off, val);
		} else {
			val = V_WIN2_MST2_EN(0);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
		}
		/* area 3 */
		if (win->area[3].state == 1) {
			val = V_WIN2_MST3_EN(win->area[3].state) |
			    V_WIN2_DATA_FMT3(win->area[3].fmt_cfg) |
			    V_WIN2_RB_SWAP3(win->area[3].swap_rb);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);

			val = V_WIN2_VIR_STRIDE3(win->area[3].y_vir_stride);
			vop_msk_reg(vop_dev, WIN2_VIR2_3 + off, val);

			val = V_WIN2_DSP_WIDTH3(win->area[3].xsize - 1) |
			    V_WIN2_DSP_HEIGHT3(win->area[3].ysize - 1);
			vop_writel(vop_dev, WIN2_DSP_INFO3 + off, val);
			val = V_WIN2_DSP_XST3(win->area[3].dsp_stx) |
			    V_WIN2_DSP_YST3(win->area[3].dsp_sty);
			vop_writel(vop_dev, WIN2_DSP_ST3 + off, val);
		} else {
			val = V_WIN2_MST3_EN(0);
			vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
		}
	} else {
		val = V_WIN2_EN(win->state) | V_WIN2_MST0_EN(0) |
		    V_WIN2_MST1_EN(0) | V_WIN2_MST2_EN(0) | V_WIN2_MST3_EN(0);
		vop_msk_reg(vop_dev, WIN2_CTRL0 + off, val);
	}

	return 0;
}

static int vop_hwc_reg_update(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = dev_drv->win[win_id];
	unsigned int hwc_size = 0;
	u64 val;

	if ((win->area[0].xsize == 32) && (win->area[0].ysize == 32)) {
		hwc_size = 0;
	} else if ((win->area[0].xsize == 64) && (win->area[0].ysize == 64)) {
		hwc_size = 1;
	} else if ((win->area[0].xsize == 96) && (win->area[0].ysize == 96)) {
		hwc_size = 2;
	} else if ((win->area[0].xsize == 128) &&
		   (win->area[0].ysize == 128)) {
		hwc_size = 3;
	} else {
		dev_err(vop_dev->dev, "un supported hwc size[%dx%d]!\n",
				win->area[0].xsize, win->area[0].ysize);
		return -EINVAL;
	}

	if (win->state == 1) {
		vop_axi_gather_cfg(vop_dev, win);
		val = V_HWC_EN(1) | V_HWC_DATA_FMT(win->area[0].fmt_cfg) |
		    V_HWC_RB_SWAP(win->area[0].swap_rb);
		vop_msk_reg(vop_dev, HWC_CTRL0, val);

		val = V_HWC_SIZE(hwc_size);
		vop_msk_reg(vop_dev, HWC_CTRL0, val);

		val = V_HWC_DSP_XST(win->area[0].dsp_stx) |
		    V_HWC_DSP_YST(win->area[0].dsp_sty);
		vop_msk_reg(vop_dev, HWC_DSP_ST, val);
	} else {
		val = V_HWC_EN(win->state);
		vop_msk_reg(vop_dev, HWC_CTRL0, val);
	}

	return 0;
}

static int vop_layer_update_regs(struct vop_device *vop_dev,
				 struct rk_lcdc_win *win)
{
	struct rk_lcdc_driver *dev_drv = &vop_dev->driver;

	if (likely(vop_dev->clk_on)) {
		vop_msk_reg(vop_dev, SYS_CTRL,
			    V_VOP_STANDBY_EN(vop_dev->standby));
		if ((win->id == VOP_WIN0) || (win->id == VOP_WIN1))
			vop_win_0_1_reg_update(dev_drv, win->id);
		else if ((win->id == VOP_WIN2) || (win->id == VOP_WIN3))
			vop_win_2_3_reg_update(dev_drv, win->id);
		else if (win->id == VOP_HWC)
			vop_hwc_reg_update(dev_drv, win->id);
		vop_cfg_done(vop_dev);
	}

	DBG(2, "%s for lcdc%d\n", __func__, vop_dev->id);
	return 0;
}

static int __maybe_unused vop_mmu_en(struct rk_lcdc_driver *dev_drv)
{
	u64 val;
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	if (dev_drv->iommu_enabled) {
		if (!vop_dev->iommu_status && dev_drv->mmu_dev) {
			if (likely(vop_dev->clk_on)) {
				val = V_VOP_MMU_EN(1);
				vop_msk_reg(vop_dev, SYS_CTRL, val);
				val = V_AXI_OUTSTANDING_MAX_NUM(31) |
					V_AXI_MAX_OUTSTANDING_EN(1);
				vop_msk_reg(vop_dev, SYS_CTRL1, val);
			}
			vop_dev->iommu_status = 1;
			rockchip_iovmm_activate(dev_drv->dev);
		}
	}
	return 0;
}

static int vop_set_dclk(struct rk_lcdc_driver *dev_drv, int reset_rate)
{
	int ret = 0, fps = 0;
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
#ifdef CONFIG_RK_FPGA
	return 0;
#endif
	if (reset_rate)
		ret = clk_set_rate(vop_dev->dclk, screen->mode.pixclock);
	if (ret)
		dev_err(dev_drv->dev, "set lcdc%d dclk[%d] failed\n",
			vop_dev->id, screen->mode.pixclock);
	vop_dev->pixclock =
	    div_u64(1000000000000llu, clk_get_rate(vop_dev->dclk));
	vop_dev->driver.pixclock = vop_dev->pixclock;

	fps = rk_fb_calc_fps(screen, vop_dev->pixclock);
	screen->ft = 1000 / fps;
	dev_info(vop_dev->dev, "%s: dclk:%lu>>fps:%d ",
		 vop_dev->driver.name, clk_get_rate(vop_dev->dclk), fps);
	return 0;
}

static int vop_config_timing(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
	u16 hsync_len = screen->mode.hsync_len;
	u16 left_margin = screen->mode.left_margin;
	u16 right_margin = screen->mode.right_margin;
	u16 vsync_len = screen->mode.vsync_len;
	u16 upper_margin = screen->mode.upper_margin;
	u16 lower_margin = screen->mode.lower_margin;
	u16 x_res = screen->mode.xres;
	u16 y_res = screen->mode.yres;
	u64 val;
	u16 h_total, v_total;
	u16 vact_end_f1, vact_st_f1, vs_end_f1, vs_st_f1;

	h_total = hsync_len + left_margin + x_res + right_margin;
	v_total = vsync_len + upper_margin + y_res + lower_margin;

	val = V_DSP_HS_END(hsync_len) | V_DSP_HTOTAL(h_total);
	vop_msk_reg(vop_dev, DSP_HTOTAL_HS_END, val);

	val = V_DSP_HACT_END(hsync_len + left_margin + x_res) |
	    V_DSP_HACT_ST(hsync_len + left_margin);
	vop_msk_reg(vop_dev, DSP_HACT_ST_END, val);

	if (screen->mode.vmode & FB_VMODE_INTERLACED) {
		/* First Field Timing */
		val = V_DSP_VS_END(vsync_len) |
		    V_DSP_VTOTAL(2 * (vsync_len + upper_margin +
				      lower_margin) + y_res + 1);
		vop_msk_reg(vop_dev, DSP_VTOTAL_VS_END, val);

		val = V_DSP_VACT_END(vsync_len + upper_margin + y_res / 2) |
		    V_DSP_VACT_ST(vsync_len + upper_margin);
		vop_msk_reg(vop_dev, DSP_VACT_ST_END, val);

		/* Second Field Timing */
		vs_st_f1 = vsync_len + upper_margin + y_res / 2 + lower_margin;
		vs_end_f1 = 2 * vsync_len + upper_margin + y_res / 2 +
		    lower_margin;
		val = V_DSP_VS_ST_F1(vs_st_f1) | V_DSP_VS_END_F1(vs_end_f1);
		vop_msk_reg(vop_dev, DSP_VS_ST_END_F1, val);

		vact_end_f1 = 2 * (vsync_len + upper_margin) + y_res +
		    lower_margin + 1;
		vact_st_f1 = 2 * (vsync_len + upper_margin) + y_res / 2 +
		    lower_margin + 1;
		val = V_DSP_VACT_END_F1(vact_end_f1) |
			V_DSP_VACT_ST_F1(vact_st_f1);
		vop_msk_reg(vop_dev, DSP_VACT_ST_END_F1, val);
		vop_msk_reg(vop_dev, DSP_CTRL0,
			    V_DSP_INTERLACE(1) | V_DSP_FIELD_POL(0));

		val = V_DSP_LINE_FLAG_NUM_0(lower_margin ?
					    vact_end_f1 : vact_end_f1 - 1);

		val |= V_DSP_LINE_FLAG_NUM_1(lower_margin ?
					     vact_end_f1 : vact_end_f1 - 1);
		vop_msk_reg(vop_dev, LINE_FLAG, val);
	} else {
		val = V_DSP_VS_END(vsync_len) | V_DSP_VTOTAL(v_total);
		vop_msk_reg(vop_dev, DSP_VTOTAL_VS_END, val);

		val = V_DSP_VACT_END(vsync_len + upper_margin + y_res) |
		    V_DSP_VACT_ST(vsync_len + upper_margin);
		vop_msk_reg(vop_dev, DSP_VACT_ST_END, val);

		vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_INTERLACE(0) |
			    V_DSP_FIELD_POL(0));

		val = V_DSP_LINE_FLAG_NUM_0(vsync_len + upper_margin + y_res) |
			V_DSP_LINE_FLAG_NUM_1(vsync_len + upper_margin + y_res);
		vop_msk_reg(vop_dev, LINE_FLAG, val);
	}
	vop_post_cfg(dev_drv);
	if ((x_res <= VOP_INPUT_MAX_WIDTH / 2) && (vop_dev->id == 0))
		vop_msk_reg(vop_dev, SYS_CTRL, V_POST_LB_MODE(1));
	else
		vop_msk_reg(vop_dev, SYS_CTRL, V_POST_LB_MODE(0));

	return 0;
}

static void vop_bcsh_path_sel(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u32 bcsh_ctrl;

	vop_msk_reg(vop_dev, SYS_CTRL, V_OVERLAY_MODE(dev_drv->overlay_mode));
	if (dev_drv->overlay_mode == VOP_YUV_DOMAIN) {
		if (IS_YUV_COLOR(dev_drv->output_color))	/* bypass */
			vop_msk_reg(vop_dev, BCSH_CTRL,
				    V_BCSH_Y2R_EN(0) | V_BCSH_R2Y_EN(0));
		else		/* YUV2RGB */
			vop_msk_reg(vop_dev, BCSH_CTRL, V_BCSH_Y2R_EN(1) |
				    V_BCSH_Y2R_CSC_MODE(VOP_Y2R_CSC_MPEG) |
				    V_BCSH_R2Y_EN(0));
	} else {
		/* overlay_mode=VOP_RGB_DOMAIN */
		/* bypass  --need check,if bcsh close? */
		if (dev_drv->output_color == COLOR_RGB) {
			bcsh_ctrl = vop_readl(vop_dev, BCSH_CTRL);
			if (((bcsh_ctrl & MASK(BCSH_EN)) == 1) ||
			    (dev_drv->bcsh.enable == 1))/*bcsh enabled */
				vop_msk_reg(vop_dev, BCSH_CTRL,
					    V_BCSH_R2Y_EN(1) |
					    V_BCSH_Y2R_EN(1));
			else
				vop_msk_reg(vop_dev, BCSH_CTRL,
					    V_BCSH_R2Y_EN(0) |
					    V_BCSH_Y2R_EN(0));
		} else {
			/* RGB2YUV */
			vop_msk_reg(vop_dev, BCSH_CTRL,
				    V_BCSH_R2Y_EN(1) |
				    V_BCSH_R2Y_CSC_MODE(VOP_Y2R_CSC_MPEG) |
				    V_BCSH_Y2R_EN(0));
		}
	}
}

static int vop_get_dspbuf_info(struct rk_lcdc_driver *dev_drv, u16 *xact,
			       u16 *yact, int *format, u32 *dsp_addr,
			       int *ymirror)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	u32 val;

	spin_lock(&vop_dev->reg_lock);

	val = vop_readl(vop_dev, WIN0_ACT_INFO);
	*xact = (val & MASK(WIN0_ACT_WIDTH)) + 1;
	*yact = ((val & MASK(WIN0_ACT_HEIGHT)) >> 16) + 1;

	val = vop_readl(vop_dev, WIN0_CTRL0);
	*format = (val & MASK(WIN0_DATA_FMT)) >> 1;
	*ymirror = (val & MASK(WIN0_Y_MIR_EN)) >> 22;
	*dsp_addr = vop_readl(vop_dev, WIN0_YRGB_MST);

	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_post_dspbuf(struct rk_lcdc_driver *dev_drv, u32 rgb_mst,
			   int format, u16 xact, u16 yact, u16 xvir,
			   int ymirror)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	int swap = (format == RGB888) ? 1 : 0;
	struct rk_lcdc_win *win = dev_drv->win[0];
	u64 val;

	val = V_WIN0_DATA_FMT(format) | V_WIN0_RB_SWAP(swap) |
		V_WIN0_Y_MIR_EN(ymirror);
	vop_msk_reg(vop_dev, WIN0_CTRL0, val);

	vop_msk_reg(vop_dev, WIN0_VIR, V_WIN0_VIR_STRIDE(xvir));
	vop_writel(vop_dev, WIN0_ACT_INFO, V_WIN0_ACT_WIDTH(xact - 1) |
		   V_WIN0_ACT_HEIGHT(yact - 1));

	vop_writel(vop_dev, WIN0_YRGB_MST, rgb_mst);

	vop_cfg_done(vop_dev);

	if (format == RGB888)
		win->area[0].format = BGR888;
	else
		win->area[0].format = format;

	win->ymirror = ymirror;
	win->state = 1;
	win->last_state = 1;

	return 0;
}

static int vop_load_screen(struct rk_lcdc_driver *dev_drv, bool initscreen)
{
	u16 face = 0;
	u16 dclk_ddr = 0;
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
	u64 val = 0;

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}

	if (!vop_dev->standby && initscreen && (dev_drv->first_frame != 1))
		flush_kthread_worker(&dev_drv->update_regs_worker);

	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		switch (screen->face) {
		case OUT_P565:
			face = OUT_P565;
			val = V_DITHER_DOWN_EN(1) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(1) | V_DITHER_DOWN_MODE(0);
			break;
		case OUT_P666:
			face = OUT_P666;
			val = V_DITHER_DOWN_EN(1) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(1) | V_DITHER_DOWN_MODE(1);
			break;
		case OUT_D888_P565:
			face = OUT_P888;
			val = V_DITHER_DOWN_EN(1) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(1) | V_DITHER_DOWN_MODE(0);
			break;
		case OUT_D888_P666:
			face = OUT_P888;
			val = V_DITHER_DOWN_EN(1) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(1) | V_DITHER_DOWN_MODE(1);
			break;
		case OUT_P888:
			face = OUT_P888;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1)
				| V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(0) | V_DITHER_DOWN_MODE(0);
			break;
		case OUT_S888x:
			face = OUT_S888x;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1)
				| V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(0) | V_DITHER_DOWN_MODE(0);
			break;
		case OUT_S888:
			face = OUT_S888;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1)
				| V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(0) | V_DITHER_DOWN_MODE(0);
			break;
		case OUT_YUV_420:
			face = OUT_YUV_420;
			dclk_ddr = 1;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(0) |
				V_DITHER_DOWN_MODE(0);
			break;
		case OUT_YUV_420_10BIT:
			face = OUT_YUV_420;
			dclk_ddr = 1;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(0) |
				V_DITHER_DOWN_SEL(0) |
				V_DITHER_DOWN_MODE(0);
			break;
		case OUT_YUV_422:
			face = OUT_YUV_422;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(1) |
				V_DITHER_DOWN_SEL(0) |
				V_DITHER_DOWN_MODE(0);
			break;
		case OUT_YUV_422_10BIT:
			face = OUT_YUV_422;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(0) |
				V_DITHER_DOWN_SEL(0) |
				V_DITHER_DOWN_MODE(0);
			break;
		case OUT_P101010:
			face = OUT_P101010;
			val = V_DITHER_DOWN_EN(0) | V_DITHER_UP_EN(1) |
				V_PRE_DITHER_DOWN_EN(0) |
				V_DITHER_DOWN_SEL(0) |
				V_DITHER_DOWN_MODE(0);
			break;
		default:
			dev_err(vop_dev->dev, "un supported screen face[%d]!\n",
				screen->face);
			break;
		}

		vop_msk_reg(vop_dev, DSP_CTRL1, val);
		switch (screen->type) {
		case SCREEN_TVOUT:
			val = V_SW_UV_OFFSET_EN(1) | V_SW_IMD_TVE_DCLK_EN(1) |
				V_SW_IMD_TVE_DCLK_EN(1) |
				V_SW_IMD_TVE_DCLK_POL(1) |
				V_SW_GENLOCK(1) | V_SW_DAC_SEL(1);
			if (screen->mode.xres == 720 &&
			    screen->mode.yres == 576)
				val |= V_SW_TVE_MODE(1);
			else
				val |= V_SW_TVE_MODE(0);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			break;
		case SCREEN_HDMI:
			if ((VOP_CHIP(vop_dev) == VOP_RK3399) &&
			    ((screen->face == OUT_P888) ||
			     (screen->face == OUT_P101010))) {
				if (vop_dev->id == 0)
					face = OUT_P101010; /*RGB 10bit output*/
				else
					face = OUT_P888;
			}
			val = V_HDMI_OUT_EN(1) | V_SW_UV_OFFSET_EN(0);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			val = V_HDMI_HSYNC_POL(screen->pin_hsync) |
				V_HDMI_VSYNC_POL(screen->pin_vsync) |
				V_HDMI_DEN_POL(screen->pin_den) |
				V_HDMI_DCLK_POL(screen->pin_dclk);
			/*hsync vsync den dclk polo,dither */
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
			break;
		case SCREEN_RGB:
		case SCREEN_LVDS:
			val = V_RGB_OUT_EN(1);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			break;
		case SCREEN_MIPI:
			val = V_MIPI_OUT_EN(1);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			val = V_MIPI_HSYNC_POL(screen->pin_hsync) |
				V_MIPI_VSYNC_POL(screen->pin_vsync) |
				V_MIPI_DEN_POL(screen->pin_den) |
				V_MIPI_DCLK_POL(screen->pin_dclk);
			/*hsync vsync den dclk polo,dither */
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
			break;
		case SCREEN_DUAL_MIPI:
			val = V_MIPI_OUT_EN(1) | V_MIPI_DUAL_CHANNEL_EN(1);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			val = V_MIPI_HSYNC_POL(screen->pin_hsync) |
				V_MIPI_VSYNC_POL(screen->pin_vsync) |
				V_MIPI_DEN_POL(screen->pin_den) |
				V_MIPI_DCLK_POL(screen->pin_dclk);
			/*hsync vsync den dclk polo,dither */
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
			break;
		case SCREEN_EDP:
			if (VOP_CHIP(vop_dev) == VOP_RK3399) {
				if (vop_dev->id == 0)
					face = OUT_P101010;
				else
					face = OUT_P888;
			}
			val = V_EDP_OUT_EN(1);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			val = V_EDP_HSYNC_POL(screen->pin_hsync) |
				V_EDP_VSYNC_POL(screen->pin_vsync) |
				V_EDP_DEN_POL(screen->pin_den) |
				V_EDP_DCLK_POL(screen->pin_dclk);
			/*hsync vsync den dclk polo,dither */
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
			break;
		case SCREEN_DP:
			dclk_ddr = 0;
			if ((VOP_CHIP(vop_dev) == VOP_RK3399) &&
			    ((screen->face == OUT_P888) ||
			     (screen->face == OUT_P101010))) {
				if (vop_dev->id == 0)
					face = OUT_P101010;
				else
					face = OUT_P888;
			}
			val = V_DP_OUT_EN(1);
			vop_msk_reg(vop_dev, SYS_CTRL, val);
			val = V_DP_HSYNC_POL(screen->pin_hsync) |
				V_DP_VSYNC_POL(screen->pin_vsync) |
				V_DP_DEN_POL(screen->pin_den) |
				V_DP_DCLK_POL(screen->pin_dclk);
			/*hsync vsync den dclk polo,dither */
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
			break;
		default:
			dev_err(vop_dev->dev, "un supported interface[%d]!\n",
				screen->type);
			break;
		}

		if (screen->color_mode == COLOR_RGB)
			dev_drv->overlay_mode = VOP_RGB_DOMAIN;
		else
			dev_drv->overlay_mode = VOP_YUV_DOMAIN;

#ifndef CONFIG_RK_FPGA
		/*
		 * Todo:
		 * writel_relaxed(v, RK_GRF_VIRT + vop_GRF_SOC_CON7);
		 *  move to  lvds driver
		 */
		/*GRF_SOC_CON7 bit[15]:0->dsi/lvds mode,1->ttl mode */
#endif
		val = V_DSP_OUT_MODE(face) | V_DSP_DCLK_DDR(dclk_ddr) |
		    V_DSP_BG_SWAP(screen->swap_gb) |
		    V_DSP_RB_SWAP(screen->swap_rb) |
		    V_DSP_RG_SWAP(screen->swap_rg) |
		    V_DSP_DELTA_SWAP(screen->swap_delta) |
		    V_DSP_DUMMY_SWAP(screen->swap_dumy) | V_DSP_OUT_ZERO(0) |
		    V_DSP_BLANK_EN(0) | V_DSP_BLACK_EN(0) |
		    V_DSP_X_MIR_EN(screen->x_mirror) |
		    V_DSP_Y_MIR_EN(screen->y_mirror);
		val |= V_SW_CORE_DCLK_SEL(!!screen->pixelrepeat);
		if (screen->mode.vmode & FB_VMODE_INTERLACED)
			val |= V_SW_HDMI_CLK_I_SEL(1);
		else
			val |= V_SW_HDMI_CLK_I_SEL(0);
		vop_msk_reg(vop_dev, DSP_CTRL0, val);

		if (screen->mode.vmode & FB_VMODE_INTERLACED)
			vop_msk_reg(vop_dev, SYS_CTRL1, V_REG_DONE_FRM(1));
		else
			vop_msk_reg(vop_dev, SYS_CTRL1, V_REG_DONE_FRM(0));
		/* BG color */
		if (dev_drv->overlay_mode == VOP_YUV_DOMAIN) {
			val = V_DSP_OUT_RGB_YUV(1);
			vop_msk_reg(vop_dev, POST_SCL_CTRL, val);
			val = V_DSP_BG_BLUE(0x200) | V_DSP_BG_GREEN(0x40) |
				V_DSP_BG_RED(0x200);
			vop_msk_reg(vop_dev, DSP_BG, val);
		} else {
			val = V_DSP_OUT_RGB_YUV(0);
			vop_msk_reg(vop_dev, POST_SCL_CTRL, val);
			val = V_DSP_BG_BLUE(0x55) | V_DSP_BG_GREEN(0x55) |
				V_DSP_BG_RED(0x55);
			vop_msk_reg(vop_dev, DSP_BG, val);
		}
		dev_drv->output_color = screen->color_mode;
		vop_bcsh_path_sel(dev_drv);
		vop_config_timing(dev_drv);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);
	vop_set_dclk(dev_drv, 1);
	if (screen->type != SCREEN_HDMI && screen->type != SCREEN_TVOUT &&
	    dev_drv->trsm_ops && dev_drv->trsm_ops->enable)
		dev_drv->trsm_ops->enable();
	if (screen->init)
		screen->init();

	return 0;
}

static int vop_early_suspend(struct rk_lcdc_driver *dev_drv);
static int vop_early_resume(struct rk_lcdc_driver *dev_drv);
/*enable layer,open:1,enable;0 disable*/
static void vop_layer_enable(struct vop_device *vop_dev,
			     unsigned int win_id, bool open)
{
	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on) &&
	    vop_dev->driver.win[win_id]->state != open) {
		if (open) {
			if (!vop_dev->atv_layer_cnt) {
				dev_info(vop_dev->dev,
					 "wakeup from standby!\n");
				vop_dev->standby = 0;
			}
			vop_dev->atv_layer_cnt |= (1 << win_id);
		} else {
			if (vop_dev->atv_layer_cnt & (1 << win_id))
				vop_dev->atv_layer_cnt &= ~(1 << win_id);
		}
		vop_dev->driver.win[win_id]->state = open;
		if (!open) {
			vop_layer_update_regs(vop_dev,
					      vop_dev->driver.win[win_id]);
			vop_cfg_done(vop_dev);
		}
	}
	spin_unlock(&vop_dev->reg_lock);
	/* if no layer used,disable lcdc */
	if (vop_dev->prop == EXTEND) {
		if (!vop_dev->atv_layer_cnt && !open) {
			if (!wait_event_timeout(vop_dev->wait_dmc_queue,
						!vop_dev->dmc_in_process, HZ / 5))
				dev_warn(vop_dev->dev,
					 "Timeout waiting for dmc when vop disable\n");

			vop_dev->vop_switch_status = 1;
			vop_early_suspend(&vop_dev->driver);
			dev_info(vop_dev->dev,
				 "no layer is used,go to standby!\n");
			vop_dev->standby = 1;

			vop_dev->vop_switch_status = 0;
			wake_up(&vop_dev->wait_vop_switch_queue);
			/*
			 * if clsoe enxtend vop need to enable dmc again.
			 */
			if (vop_dev->devfreq) {
				if (vop_dev->devfreq_event_dev)
					devfreq_event_enable_edev(vop_dev->devfreq_event_dev);
				devfreq_resume_device(vop_dev->devfreq);
			}
		} else if (open) {
			vop_early_resume(&vop_dev->driver);
			vop_dev->vop_switch_status = 0;
			wake_up(&vop_dev->wait_vop_switch_queue);
			/* if enable two vop, need to disable dmc */
			if (vop_dev->devfreq) {
				if (vop_dev->devfreq_event_dev)
					devfreq_event_disable_edev(vop_dev->devfreq_event_dev);
				devfreq_suspend_device(vop_dev->devfreq);
			}
			dev_info(vop_dev->dev, "wake up from standby!\n");
		}
	} else if (vop_dev->prop == PRMRY) {
		if ((open) && (!vop_dev->atv_layer_cnt)) {
			vop_dev->vop_switch_status = 0;
			wake_up(&vop_dev->wait_vop_switch_queue);
		}
	}
}

static int vop_enable_irq(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev = container_of(dev_drv,
						    struct vop_device, driver);
	u64 val;
	/* struct rk_screen *screen = dev_drv->cur_screen; */

	vop_mask_writel(vop_dev, INTR_CLEAR0, INTR_MASK, INTR_MASK);

	val = INTR_FS | INTR_LINE_FLAG0 | INTR_BUS_ERROR | INTR_LINE_FLAG1 |
		INTR_WIN0_EMPTY | INTR_WIN1_EMPTY | INTR_HWC_EMPTY |
		INTR_POST_BUF_EMPTY;
	val |= val << 16;

	vop_msk_reg(vop_dev, INTR_EN0, val);

	return 0;
}

static int dmc_notify(struct notifier_block *nb, unsigned long event,
		      void *data)
{
	struct vop_device *vop = container_of(nb, struct vop_device, dmc_nb);

	if (event == DEVFREQ_PRECHANGE) {

		/*
		 * check if vop in enable or disable process,
		 * if yes, wait until it finish, use 200ms as
		 * timeout.
		 */
		if (!wait_event_timeout(vop->wait_vop_switch_queue,
					!vop->vop_switch_status, HZ / 5))
			dev_warn(vop->dev,
				 "Timeout waiting for vop swtich status\n");
		vop->dmc_in_process = 1;
	} else if (event == DEVFREQ_POSTCHANGE) {
		vop->dmc_in_process = 0;
		wake_up(&vop->wait_dmc_queue);
	}

	return NOTIFY_OK;
}

static int vop_open(struct rk_lcdc_driver *dev_drv, int win_id,
		    bool open)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (dev_drv->shutdown_flag)
		return 0;
	/* enable clk,when first layer open */
	if ((open) && (!vop_dev->atv_layer_cnt)) {
		/* rockchip_set_system_status(sys_status); */
		if (!wait_event_timeout(vop_dev->wait_dmc_queue,
				!vop_dev->dmc_in_process, HZ / 5))
			dev_warn(vop_dev->dev,
				 "Timeout waiting for dmc when vop enable\n");
		vop_dev->vop_switch_status = 1;
		if (vop_pre_init(dev_drv))
			return -1;
		vop_clk_enable(vop_dev);
		vop_enable_irq(dev_drv);
		if (dev_drv->iommu_enabled) {
			if (!dev_drv->mmu_dev) {
				dev_drv->mmu_dev =
				    rk_fb_get_sysmmu_device_by_compatible
				    (dev_drv->mmu_dts_name);
				if (dev_drv->mmu_dev) {
					rk_fb_platform_set_sysmmu
					    (dev_drv->mmu_dev, dev_drv->dev);
				} else {
					dev_err(dev_drv->dev,
						"fail get rk iommu device\n");
					return -1;
				}
			}
		}
		if ((support_uboot_display() && (vop_dev->prop == PRMRY)))
			vop_set_dclk(dev_drv, 0);
		else
			vop_load_screen(dev_drv, 1);
		if (dev_drv->bcsh.enable)
			vop_set_bcsh(dev_drv, 1);
		vop_set_lut(dev_drv, dev_drv->cur_screen->dsp_lut);
		vop_set_cabc(dev_drv, dev_drv->cur_screen->cabc_lut);
	}

	if (win_id < dev_drv->lcdc_win_num)
		vop_layer_enable(vop_dev, win_id, open);
	else
		dev_err(vop_dev->dev, "invalid win id:%d\n", win_id);

	dev_drv->first_frame = 0;
	return 0;
}

static int win_0_1_display(struct vop_device *vop_dev,
			   struct rk_lcdc_win *win)
{
	u32 y_addr;
	u32 uv_addr;
	unsigned int off;

	off = win->id * 0x40;
	/*win->smem_start + win->y_offset; */
	y_addr = win->area[0].smem_start + win->area[0].y_offset;
	uv_addr = win->area[0].cbr_start + win->area[0].c_offset;
	DBG(2, "lcdc[%d]:win[%d]>>:y_addr:0x%x>>uv_addr:0x%x",
	    vop_dev->id, win->id, y_addr, uv_addr);
	DBG(2, ">>y_offset:0x%x>>c_offset=0x%x\n",
	    win->area[0].y_offset, win->area[0].c_offset);
	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		win->area[0].y_addr = y_addr;
		win->area[0].uv_addr = uv_addr;
		vop_writel(vop_dev, WIN0_YRGB_MST + off, win->area[0].y_addr);
		vop_writel(vop_dev, WIN0_CBR_MST + off, win->area[0].uv_addr);
		if (win->area[0].fbdc_en == 1)
			vop_writel(vop_dev, AFBCD0_HDR_PTR,
				   win->area[0].y_addr);
	}
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int win_2_3_display(struct vop_device *vop_dev,
			   struct rk_lcdc_win *win)
{
	u32 i, y_addr;
	unsigned int off;

	off = (win->id - 2) * 0x50;
	y_addr = win->area[0].smem_start + win->area[0].y_offset;
	DBG(2, "lcdc[%d]:win[%d]:", vop_dev->id, win->id);

	if (likely(vop_dev->clk_on)) {
		for (i = 0; i < win->area_num; i++) {
			DBG(2, "area[%d]:yaddr:0x%x>>offset:0x%x>>\n",
			    i, win->area[i].y_addr, win->area[i].y_offset);
			win->area[i].y_addr =
			    win->area[i].smem_start + win->area[i].y_offset;
			}
		spin_lock(&vop_dev->reg_lock);
		vop_writel(vop_dev, WIN2_MST0 + off, win->area[0].y_addr);
		vop_writel(vop_dev, WIN2_MST1 + off, win->area[1].y_addr);
		vop_writel(vop_dev, WIN2_MST2 + off, win->area[2].y_addr);
		vop_writel(vop_dev, WIN2_MST3 + off, win->area[3].y_addr);
		if (win->area[0].fbdc_en == 1)
			vop_writel(vop_dev, AFBCD0_HDR_PTR,
				   win->area[0].y_addr);
		spin_unlock(&vop_dev->reg_lock);
	}
	return 0;
}

static int hwc_display(struct vop_device *vop_dev, struct rk_lcdc_win *win)
{
	u32 y_addr;

	y_addr = win->area[0].smem_start + win->area[0].y_offset;
	DBG(2, "lcdc[%d]:hwc>>%s>>y_addr:0x%x>>\n",
	    vop_dev->id, __func__, y_addr);
	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		win->area[0].y_addr = y_addr;
		vop_writel(vop_dev, HWC_MST, win->area[0].y_addr);
	}
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_pan_display(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = NULL;
	struct rk_screen *screen = dev_drv->cur_screen;

	win = dev_drv->win[win_id];
	if (!screen) {
		dev_err(dev_drv->dev, "screen is null!\n");
		return -ENOENT;
	}
	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	if (win_id == 0) {
		win_0_1_display(vop_dev, win);
	} else if (win_id == 1) {
		win_0_1_display(vop_dev, win);
	} else if (win_id == 2) {
		win_2_3_display(vop_dev, win);
	} else if (win_id == 3) {
		win_2_3_display(vop_dev, win);
	} else if (win_id == 4) {
		hwc_display(vop_dev, win);
	} else {
		dev_err(dev_drv->dev, "invalid win number:%d!\n", win_id);
		return -EINVAL;
	}

	return 0;
}

static int vop_cal_scl_fac(struct rk_lcdc_win *win, struct rk_screen *screen)
{
	u16 srcW = 0;
	u16 srcH = 0;
	u16 dstW = 0;
	u16 dstH = 0;
	u16 yrgb_srcW = 0;
	u16 yrgb_srcH = 0;
	u16 yrgb_dstW = 0;
	u16 yrgb_dstH = 0;
	u32 yrgb_vscalednmult = 0;
	u32 yrgb_xscl_factor = 0;
	u32 yrgb_yscl_factor = 0;
	u8 yrgb_vsd_bil_gt2 = 0;
	u8 yrgb_vsd_bil_gt4 = 0;

	u16 cbcr_srcW = 0;
	u16 cbcr_srcH = 0;
	u16 cbcr_dstW = 0;
	u16 cbcr_dstH = 0;
	u32 cbcr_vscalednmult = 0;
	u32 cbcr_xscl_factor = 0;
	u32 cbcr_yscl_factor = 0;
	u8 cbcr_vsd_bil_gt2 = 0;
	u8 cbcr_vsd_bil_gt4 = 0;
	u8 yuv_fmt = 0;

	srcW = win->area[0].xact;
	if ((screen->mode.vmode & FB_VMODE_INTERLACED) &&
	    (win->area[0].yact == 2 * win->area[0].ysize)) {
		srcH = win->area[0].yact / 2;
		yrgb_vsd_bil_gt2 = 1;
		cbcr_vsd_bil_gt2 = 1;
	} else {
		srcH = win->area[0].yact;
	}
	dstW = win->area[0].xsize;
	dstH = win->area[0].ysize;

	/*yrgb scl mode */
	yrgb_srcW = srcW;
	yrgb_srcH = srcH;
	yrgb_dstW = dstW;
	yrgb_dstH = dstH;
	if ((yrgb_dstW * 8 <= yrgb_srcW) || (yrgb_dstH * 8 <= yrgb_srcH)) {
		pr_err("ERROR: yrgb scale exceed 8,");
		pr_err("srcW=%d,srcH=%d,dstW=%d,dstH=%d\n",
		       yrgb_srcW, yrgb_srcH, yrgb_dstW, yrgb_dstH);
	}
	if (yrgb_srcW < yrgb_dstW)
		win->yrgb_hor_scl_mode = SCALE_UP;
	else if (yrgb_srcW > yrgb_dstW)
		win->yrgb_hor_scl_mode = SCALE_DOWN;
	else
		win->yrgb_hor_scl_mode = SCALE_NONE;

	if (yrgb_srcH < yrgb_dstH)
		win->yrgb_ver_scl_mode = SCALE_UP;
	else if (yrgb_srcH > yrgb_dstH)
		win->yrgb_ver_scl_mode = SCALE_DOWN;
	else
		win->yrgb_ver_scl_mode = SCALE_NONE;

	/*cbcr scl mode */
	switch (win->area[0].format) {
	case YUV422:
	case YUYV422:
	case UYVY422:
	case YUV422_A:
		cbcr_srcW = srcW / 2;
		cbcr_dstW = dstW;
		cbcr_srcH = srcH;
		cbcr_dstH = dstH;
		yuv_fmt = 1;
		break;
	case YUV420:
	case YUYV420:
	case UYVY420:
	case YUV420_A:
	case YUV420_NV21:
		cbcr_srcW = srcW / 2;
		cbcr_dstW = dstW;
		cbcr_srcH = srcH / 2;
		cbcr_dstH = dstH;
		yuv_fmt = 1;
		break;
	case YUV444:
	case YUV444_A:
		cbcr_srcW = srcW;
		cbcr_dstW = dstW;
		cbcr_srcH = srcH;
		cbcr_dstH = dstH;
		yuv_fmt = 1;
		break;
	default:
		cbcr_srcW = 0;
		cbcr_dstW = 0;
		cbcr_srcH = 0;
		cbcr_dstH = 0;
		yuv_fmt = 0;
		break;
	}
	if (yuv_fmt) {
		if ((cbcr_dstW * 8 <= cbcr_srcW) ||
		    (cbcr_dstH * 8 <= cbcr_srcH)) {
			pr_err("ERROR: cbcr scale exceed 8,");
			pr_err("srcW=%d,srcH=%d,dstW=%d,dstH=%d\n", cbcr_srcW,
			       cbcr_srcH, cbcr_dstW, cbcr_dstH);
		}
	}

	if (cbcr_srcW < cbcr_dstW)
		win->cbr_hor_scl_mode = SCALE_UP;
	else if (cbcr_srcW > cbcr_dstW)
		win->cbr_hor_scl_mode = SCALE_DOWN;
	else
		win->cbr_hor_scl_mode = SCALE_NONE;

	if (cbcr_srcH < cbcr_dstH)
		win->cbr_ver_scl_mode = SCALE_UP;
	else if (cbcr_srcH > cbcr_dstH)
		win->cbr_ver_scl_mode = SCALE_DOWN;
	else
		win->cbr_ver_scl_mode = SCALE_NONE;

	/* line buffer mode */
	if ((win->area[0].format == YUV422) ||
	    (win->area[0].format == YUV420) ||
	    (win->area[0].format == YUYV422) ||
	    (win->area[0].format == YUYV420) ||
	    (win->area[0].format == UYVY422) ||
	    (win->area[0].format == UYVY420) ||
	    (win->area[0].format == YUV420_NV21) ||
	    (win->area[0].format == YUV422_A) ||
	    (win->area[0].format == YUV420_A)) {
		if (win->cbr_hor_scl_mode == SCALE_DOWN) {
			if ((cbcr_dstW > VOP_INPUT_MAX_WIDTH / 2) ||
			    (cbcr_dstW == 0))
				pr_err("ERROR cbcr_dstW = %d,exceeds 2048\n",
				       cbcr_dstW);
			else if (cbcr_dstW > 1280)
				win->win_lb_mode = LB_YUV_3840X5;
			else
				win->win_lb_mode = LB_YUV_2560X8;
		} else {	/* SCALE_UP or SCALE_NONE */
			if ((cbcr_srcW > VOP_INPUT_MAX_WIDTH / 2) ||
			    (cbcr_srcW == 0))
				pr_err("ERROR cbcr_srcW = %d,exceeds 2048\n",
				       cbcr_srcW);
			else if (cbcr_srcW > 1280)
				win->win_lb_mode = LB_YUV_3840X5;
			else
				win->win_lb_mode = LB_YUV_2560X8;
		}
	} else {
		if (win->yrgb_hor_scl_mode == SCALE_DOWN) {
			if ((yrgb_dstW > VOP_INPUT_MAX_WIDTH) ||
			    (yrgb_dstW == 0))
				pr_err("ERROR yrgb_dstW = %d\n", yrgb_dstW);
			else if (yrgb_dstW > 2560)
				win->win_lb_mode = LB_RGB_3840X2;
			else if (yrgb_dstW > 1920)
				win->win_lb_mode = LB_RGB_2560X4;
			else if (yrgb_dstW > 1280)
				win->win_lb_mode = LB_RGB_1920X5;
			else
				win->win_lb_mode = LB_RGB_1280X8;
		} else {	/* SCALE_UP or SCALE_NONE */
			if ((yrgb_srcW > VOP_INPUT_MAX_WIDTH) ||
			    (yrgb_srcW == 0))
				pr_err("ERROR yrgb_srcW = %d\n", yrgb_srcW);
			else if (yrgb_srcW > 2560)
				win->win_lb_mode = LB_RGB_3840X2;
			else if (yrgb_srcW > 1920)
				win->win_lb_mode = LB_RGB_2560X4;
			else if (yrgb_srcW > 1280)
				win->win_lb_mode = LB_RGB_1920X5;
			else
				win->win_lb_mode = LB_RGB_1280X8;
		}
	}
	DBG(1, "win->win_lb_mode = %d;\n", win->win_lb_mode);

	/* vsd/vsu scale ALGORITHM */
	win->yrgb_hsd_mode = SCALE_DOWN_BIL;	/*not to specify */
	win->cbr_hsd_mode = SCALE_DOWN_BIL;	/*not to specify */
	win->yrgb_vsd_mode = SCALE_DOWN_BIL;	/*not to specify */
	win->cbr_vsd_mode = SCALE_DOWN_BIL;	/*not to specify */

	/* if (VOP_CHIP(vop_dev) == VOP_RK3399) { */
	if ((win->area[0].format == YUYV422) ||
	    (win->area[0].format == YUYV420) ||
	    (win->area[0].format == UYVY422) ||
	    (win->area[0].format == UYVY420)) {
		yrgb_vscalednmult =
			vop_get_hard_ware_vskiplines(yrgb_srcH, yrgb_dstH);
		if (yrgb_vscalednmult == 4) {
			yrgb_vsd_bil_gt4 = 1;
			yrgb_vsd_bil_gt2 = 0;
		} else if (yrgb_vscalednmult == 2) {
			yrgb_vsd_bil_gt4 = 0;
			yrgb_vsd_bil_gt2 = 1;
		} else {
			yrgb_vsd_bil_gt4 = 0;
			yrgb_vsd_bil_gt2 = 0;
		}
		if ((win->area[0].format == YUYV420) ||
		    (win->area[0].format == UYVY420)) {
			if ((yrgb_vsd_bil_gt4 == 1) || (yrgb_vsd_bil_gt2 == 1))
				win->yrgb_vsd_mode = SCALE_DOWN_AVG;
		}

		cbcr_vscalednmult =
			vop_get_hard_ware_vskiplines(cbcr_srcH, cbcr_dstH);
		if (cbcr_vscalednmult == 4) {
			cbcr_vsd_bil_gt4 = 1;
			cbcr_vsd_bil_gt2 = 0;
		} else if (cbcr_vscalednmult == 2) {
			cbcr_vsd_bil_gt4 = 0;
			cbcr_vsd_bil_gt2 = 1;
		} else {
			cbcr_vsd_bil_gt4 = 0;
			cbcr_vsd_bil_gt2 = 0;
		}
		if ((win->area[0].format == YUYV420) ||
		    (win->area[0].format == UYVY420)) {
			if ((cbcr_vsd_bil_gt4 == 1) || (cbcr_vsd_bil_gt2 == 1))
				win->cbr_vsd_mode = SCALE_DOWN_AVG;
		}
		/* CBCR vsd_mode must same to YRGB for YUYV when gt2 or gt4 */
		if ((cbcr_vsd_bil_gt4 == 1) || (cbcr_vsd_bil_gt2 == 1)) {
			if (win->yrgb_vsd_mode != win->cbr_vsd_mode)
				win->cbr_vsd_mode = win->yrgb_vsd_mode;
		}
	}
	/* 3399 yuyv support*/
	if (win->ymirror == 1) {
		if (win->yrgb_vsd_mode == SCALE_DOWN_AVG)
			pr_info("y_mirror enable, y-vsd AVG mode unsupprot\n");
		win->yrgb_vsd_mode = SCALE_DOWN_BIL;
	}
	if (screen->mode.vmode & FB_VMODE_INTERLACED) {
		if (win->yrgb_vsd_mode == SCALE_DOWN_AVG)
			pr_info("interlace mode, y-vsd AVG mode unsupprot\n");
		/* interlace mode must bill */
		win->yrgb_vsd_mode = SCALE_DOWN_BIL;
		win->cbr_vsd_mode = SCALE_DOWN_BIL;
	}
	switch (win->win_lb_mode) {
	case LB_YUV_3840X5:
	case LB_YUV_2560X8:
	case LB_RGB_1920X5:
	case LB_RGB_1280X8:
		win->yrgb_vsu_mode = SCALE_UP_BIC;
		win->cbr_vsu_mode = SCALE_UP_BIC;
		break;
	case LB_RGB_3840X2:
		if (win->yrgb_ver_scl_mode != SCALE_NONE)
			pr_err("ERROR : not allow yrgb ver scale\n");
		if (win->cbr_ver_scl_mode != SCALE_NONE)
			pr_err("ERROR : not allow cbcr ver scale\n");
		break;
	case LB_RGB_2560X4:
		win->yrgb_vsu_mode = SCALE_UP_BIL;
		win->cbr_vsu_mode = SCALE_UP_BIL;
		break;
	default:
		pr_info("%s:un supported win_lb_mode:%d\n",
			__func__, win->win_lb_mode);
		break;
	}

	if ((win->yrgb_ver_scl_mode == SCALE_DOWN) &&
	    (win->area[0].fbdc_en == 1)) {
		/* in this pattern,use bil mode,not support souble scd,
		 * use avg mode, support double scd, but aclk should be
		 * bigger than dclk.
		 */
		if (yrgb_srcH >= 2 * yrgb_dstH) {
			pr_err("ERROR : fbdc mode,not support y scale down:");
			pr_err("srcH[%d] > 2 *dstH[%d]\n",
			       yrgb_srcH, yrgb_dstH);
		}
	}
	DBG(1, "yrgb:hsd=%d,vsd=%d,vsu=%d;cbcr:hsd=%d,vsd=%d,vsu=%d\n",
	    win->yrgb_hsd_mode, win->yrgb_vsd_mode, win->yrgb_vsu_mode,
	    win->cbr_hsd_mode, win->cbr_vsd_mode, win->cbr_vsu_mode);

	/* SCALE FACTOR */

	/* (1.1)YRGB HOR SCALE FACTOR */
	switch (win->yrgb_hor_scl_mode) {
	case SCALE_NONE:
		yrgb_xscl_factor = (1 << SCALE_FACTOR_DEFAULT_FIXPOINT_SHIFT);
		break;
	case SCALE_UP:
		yrgb_xscl_factor = GET_SCALE_FACTOR_BIC(yrgb_srcW, yrgb_dstW);
		break;
	case SCALE_DOWN:
		switch (win->yrgb_hsd_mode) {
		case SCALE_DOWN_BIL:
			yrgb_xscl_factor =
			    GET_SCALE_FACTOR_BILI_DN(yrgb_srcW, yrgb_dstW);
			break;
		case SCALE_DOWN_AVG:
			yrgb_xscl_factor =
			    GET_SCALE_FACTOR_AVRG(yrgb_srcW, yrgb_dstW);
			break;
		default:
			pr_info("%s:un supported yrgb_hsd_mode:%d\n", __func__,
				win->yrgb_hsd_mode);
			break;
		}
		break;
	default:
		pr_info("%s:un supported yrgb_hor_scl_mode:%d\n",
			__func__, win->yrgb_hor_scl_mode);
		break;
	}

	/* (1.2)YRGB VER SCALE FACTOR */
	switch (win->yrgb_ver_scl_mode) {
	case SCALE_NONE:
		yrgb_yscl_factor = (1 << SCALE_FACTOR_DEFAULT_FIXPOINT_SHIFT);
		break;
	case SCALE_UP:
		switch (win->yrgb_vsu_mode) {
		case SCALE_UP_BIL:
			yrgb_yscl_factor =
			    GET_SCALE_FACTOR_BILI_UP(yrgb_srcH, yrgb_dstH);
			break;
		case SCALE_UP_BIC:
			if (yrgb_srcH < 3) {
				pr_err("yrgb_srcH should be");
				pr_err(" greater than 3 !!!\n");
			}
			yrgb_yscl_factor = GET_SCALE_FACTOR_BIC(yrgb_srcH,
								yrgb_dstH);
			break;
		default:
			pr_info("%s:un support yrgb_vsu_mode:%d\n",
				__func__, win->yrgb_vsu_mode);
			break;
		}
		break;
	case SCALE_DOWN:
		switch (win->yrgb_vsd_mode) {
		case SCALE_DOWN_BIL:
			yrgb_vscalednmult =
			    vop_get_hard_ware_vskiplines(yrgb_srcH, yrgb_dstH);
			yrgb_yscl_factor =
			    GET_SCALE_FACTOR_BILI_DN_VSKIP(yrgb_srcH, yrgb_dstH,
							   yrgb_vscalednmult);
			if (yrgb_yscl_factor >= 0x2000) {
				pr_err("yrgb_yscl_factor should less 0x2000");
				pr_err("yrgb_yscl_factor=%4x;\n",
				       yrgb_yscl_factor);
			}
			if (yrgb_vscalednmult == 4) {
				yrgb_vsd_bil_gt4 = 1;
				yrgb_vsd_bil_gt2 = 0;
			} else if (yrgb_vscalednmult == 2) {
				yrgb_vsd_bil_gt4 = 0;
				yrgb_vsd_bil_gt2 = 1;
			} else {
				yrgb_vsd_bil_gt4 = 0;
				yrgb_vsd_bil_gt2 = 0;
			}
			break;
		case SCALE_DOWN_AVG:
			yrgb_yscl_factor = GET_SCALE_FACTOR_AVRG(yrgb_srcH,
								 yrgb_dstH);
			break;
		default:
			pr_info("%s:un support yrgb_vsd_mode:%d\n",
				__func__, win->yrgb_vsd_mode);
			break;
		}		/*win->yrgb_vsd_mode */
		break;
	default:
		pr_info("%s:un supported yrgb_ver_scl_mode:%d\n",
			__func__, win->yrgb_ver_scl_mode);
		break;
	}
	win->scale_yrgb_x = yrgb_xscl_factor;
	win->scale_yrgb_y = yrgb_yscl_factor;
	win->vsd_yrgb_gt4 = yrgb_vsd_bil_gt4;
	win->vsd_yrgb_gt2 = yrgb_vsd_bil_gt2;
	DBG(1, "yrgb:h_fac=%d, V_fac=%d,gt4=%d, gt2=%d\n", yrgb_xscl_factor,
	    yrgb_yscl_factor, yrgb_vsd_bil_gt4, yrgb_vsd_bil_gt2);

	/*(2.1)CBCR HOR SCALE FACTOR */
	switch (win->cbr_hor_scl_mode) {
	case SCALE_NONE:
		cbcr_xscl_factor = (1 << SCALE_FACTOR_DEFAULT_FIXPOINT_SHIFT);
		break;
	case SCALE_UP:
		cbcr_xscl_factor = GET_SCALE_FACTOR_BIC(cbcr_srcW, cbcr_dstW);
		break;
	case SCALE_DOWN:
		switch (win->cbr_hsd_mode) {
		case SCALE_DOWN_BIL:
			cbcr_xscl_factor =
			    GET_SCALE_FACTOR_BILI_DN(cbcr_srcW, cbcr_dstW);
			break;
		case SCALE_DOWN_AVG:
			cbcr_xscl_factor =
			    GET_SCALE_FACTOR_AVRG(cbcr_srcW, cbcr_dstW);
			break;
		default:
			pr_info("%s:un support cbr_hsd_mode:%d\n",
				__func__, win->cbr_hsd_mode);
			break;
		}
		break;
	default:
		pr_info("%s:un supported cbr_hor_scl_mode:%d\n",
			__func__, win->cbr_hor_scl_mode);
		break;
	}			/*win->cbr_hor_scl_mode */

	/* (2.2)CBCR VER SCALE FACTOR */
	switch (win->cbr_ver_scl_mode) {
	case SCALE_NONE:
		cbcr_yscl_factor = (1 << SCALE_FACTOR_DEFAULT_FIXPOINT_SHIFT);
		break;
	case SCALE_UP:
		switch (win->cbr_vsu_mode) {
		case SCALE_UP_BIL:
			cbcr_yscl_factor =
			    GET_SCALE_FACTOR_BILI_UP(cbcr_srcH, cbcr_dstH);
			break;
		case SCALE_UP_BIC:
			if (cbcr_srcH < 3) {
				pr_err("cbcr_srcH should be ");
				pr_err("greater than 3 !!!\n");
			}
			cbcr_yscl_factor = GET_SCALE_FACTOR_BIC(cbcr_srcH,
								cbcr_dstH);
			break;
		default:
			pr_info("%s:un support cbr_vsu_mode:%d\n",
				__func__, win->cbr_vsu_mode);
			break;
		}
		break;
	case SCALE_DOWN:
		switch (win->cbr_vsd_mode) {
		case SCALE_DOWN_BIL:
			cbcr_vscalednmult =
			    vop_get_hard_ware_vskiplines(cbcr_srcH, cbcr_dstH);
			cbcr_yscl_factor =
			    GET_SCALE_FACTOR_BILI_DN_VSKIP(cbcr_srcH, cbcr_dstH,
							   cbcr_vscalednmult);
			if (cbcr_yscl_factor >= 0x2000) {
				pr_err("cbcr_yscl_factor should be less ");
				pr_err("than 0x2000,cbcr_yscl_factor=%4x;\n",
				       cbcr_yscl_factor);
			}

			if (cbcr_vscalednmult == 4) {
				cbcr_vsd_bil_gt4 = 1;
				cbcr_vsd_bil_gt2 = 0;
			} else if (cbcr_vscalednmult == 2) {
				cbcr_vsd_bil_gt4 = 0;
				cbcr_vsd_bil_gt2 = 1;
			} else {
				cbcr_vsd_bil_gt4 = 0;
				cbcr_vsd_bil_gt2 = 0;
			}
			break;
		case SCALE_DOWN_AVG:
			cbcr_yscl_factor = GET_SCALE_FACTOR_AVRG(cbcr_srcH,
								 cbcr_dstH);
			break;
		default:
			pr_info("%s:un support cbr_vsd_mode:%d\n",
				__func__, win->cbr_vsd_mode);
			break;
		}
		break;
	default:
		pr_info("%s:un supported cbr_ver_scl_mode:%d\n",
			__func__, win->cbr_ver_scl_mode);
		break;
	}
	win->scale_cbcr_x = cbcr_xscl_factor;
	win->scale_cbcr_y = cbcr_yscl_factor;
	win->vsd_cbr_gt4 = cbcr_vsd_bil_gt4;
	win->vsd_cbr_gt2 = cbcr_vsd_bil_gt2;

	DBG(1, "cbcr:h_fac=%d,v_fac=%d,gt4=%d,gt2=%d\n", cbcr_xscl_factor,
	    cbcr_yscl_factor, cbcr_vsd_bil_gt4, cbcr_vsd_bil_gt2);
	return 0;
}

static int dsp_x_pos(int mirror_en, struct rk_screen *screen,
		     struct rk_lcdc_win_area *area)
{
	int pos;

	if (screen->x_mirror && mirror_en)
		pr_err("not support both win and global mirror\n");

	if ((!mirror_en) && (!screen->x_mirror))
		pos = area->xpos + screen->mode.left_margin +
			screen->mode.hsync_len;
	else
		pos = screen->mode.xres - area->xpos -
			area->xsize + screen->mode.left_margin +
			screen->mode.hsync_len;

	return pos;
}

static int dsp_y_pos(int mirror_en, struct rk_screen *screen,
		     struct rk_lcdc_win_area *area)
{
	int pos;

	if (screen->y_mirror && mirror_en)
		pr_err("not support both win and global mirror\n");

	if ((!mirror_en) && (!screen->y_mirror))
		pos = area->ypos + screen->mode.upper_margin +
			screen->mode.vsync_len;
	else
		pos = screen->mode.yres - area->ypos -
			area->ysize + screen->mode.upper_margin +
			screen->mode.vsync_len;

	return pos;
}

static int win_0_1_set_par(struct vop_device *vop_dev,
			   struct rk_screen *screen, struct rk_lcdc_win *win)
{
	u32 xact = 0, yact = 0, xvir = 0, yvir = 0, xpos = 0, ypos = 0;
	u8 fmt_cfg = 0, swap_rb = 0, swap_uv = 0;
	char fmt[9] = "NULL";

	xpos = dsp_x_pos(win->xmirror, screen, &win->area[0]);
	ypos = dsp_y_pos(win->ymirror, screen, &win->area[0]);

	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		vop_cal_scl_fac(win, screen);
		switch (win->area[0].format) {
		case FBDC_RGB_565:
			fmt_cfg = 2;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].fbdc_fmt_cfg = AFBDC_FMT_RGB565;
			break;
		case FBDC_ARGB_888:
			fmt_cfg = 0;
			swap_rb = 1;
			win->fmt_10 = 0;
			win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
			break;
		case FBDC_ABGR_888:
			fmt_cfg = 0;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
			break;
		case FBDC_RGBX_888:
			fmt_cfg = 0;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
			break;
		case ARGB888:
			fmt_cfg = 0;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case XBGR888:
		case ABGR888:
			fmt_cfg = 0;
			swap_rb = 1;
			win->fmt_10 = 0;
			break;
		case BGR888:
			fmt_cfg = 1;
			swap_rb = 1;
			win->fmt_10 = 0;
			break;
		case RGB888:
			fmt_cfg = 1;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case RGB565:
			fmt_cfg = 2;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case YUV422:
			fmt_cfg = 5;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case YUV420:
			fmt_cfg = 4;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case YUV420_NV21:
			fmt_cfg = 4;
			swap_rb = 0;
			swap_uv = 1;
			win->fmt_10 = 0;
			break;
		case YUV444:
			fmt_cfg = 6;
			swap_rb = 0;
			win->fmt_10 = 0;
			break;
		case YUV422_A:
			fmt_cfg = 5;
			swap_rb = 0;
			win->fmt_10 = 1;
			break;
		case YUV420_A:
			fmt_cfg = 4;
			swap_rb = 0;
			win->fmt_10 = 1;
			break;
		case YUV444_A:
			fmt_cfg = 6;
			swap_rb = 0;
			win->fmt_10 = 1;
			break;
		case YUYV422:
			fmt_cfg = 0;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].yuyv_fmt = 1;
			break;
		case YUYV420:
			fmt_cfg = 1;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].yuyv_fmt = 1;
			break;
		case UYVY422:
			fmt_cfg = 2;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].yuyv_fmt = 1;
			break;
		case UYVY420:
			fmt_cfg = 3;
			swap_rb = 0;
			win->fmt_10 = 0;
			win->area[0].yuyv_fmt = 1;
			break;
		default:
			dev_err(vop_dev->dev, "%s:unsupport format[%d]!\n",
				__func__, win->area[0].format);
			break;
		}
		win->area[0].fmt_cfg = fmt_cfg;
		win->area[0].swap_rb = swap_rb;
		win->area[0].swap_uv = swap_uv;
		win->area[0].dsp_stx = xpos;
		win->area[0].dsp_sty = ypos;
		xact = win->area[0].xact;
		yact = win->area[0].yact;
		xvir = win->area[0].xvir;
		yvir = win->area[0].yvir;
	}
	if (win->area[0].fbdc_en)
		vop_init_fbdc_config(vop_dev, win->id);
	vop_win_0_1_reg_update(&vop_dev->driver, win->id);
	spin_unlock(&vop_dev->reg_lock);

	DBG(1, "lcdc[%d]:win[%d]\n>>format:%s>>>xact:%d>>yact:%d>>xsize:%d",
	    vop_dev->id, win->id, get_format_string(win->area[0].format, fmt),
	    xact, yact, win->area[0].xsize);
	DBG(1, ">>ysize:%d>>xvir:%d>>yvir:%d>>xpos:%d>>ypos:%d>>\n",
	    win->area[0].ysize, xvir, yvir, xpos, ypos);

	return 0;
}

static int win_2_3_set_par(struct vop_device *vop_dev,
			   struct rk_screen *screen, struct rk_lcdc_win *win)
{
	int i;
	u8 fmt_cfg = 0, swap_rb = 0;
	char fmt[9] = "NULL";

	if (VOP_CHIP(vop_dev) == VOP_RK322X) {
		pr_err("rk3228 not support win2/3 set par\n");
		return -EINVAL;
	}
	if (win->ymirror) {
		pr_err("win[%d] not support y mirror\n", win->id);
		return -EINVAL;
	}
	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		DBG(2, "lcdc[%d]:win[%d]>>\n>\n", vop_dev->id, win->id);
		for (i = 0; i < win->area_num; i++) {
			switch (win->area[i].format) {
			case FBDC_RGB_565:
				fmt_cfg = 2;
				swap_rb = 0;
				win->fmt_10 = 0;
				win->area[0].fbdc_fmt_cfg = AFBDC_FMT_RGB565;
				break;
			case FBDC_ARGB_888:
				fmt_cfg = 0;
				swap_rb = 1;
				win->fmt_10 = 0;
				win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
				break;
			case FBDC_ABGR_888:
				fmt_cfg = 0;
				swap_rb = 0;
				win->fmt_10 = 0;
				win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
				break;
			case FBDC_RGBX_888:
				fmt_cfg = 0;
				swap_rb = 0;
				win->fmt_10 = 0;
				win->area[0].fbdc_fmt_cfg = AFBDC_FMT_U8U8U8U8;
				break;
			case ARGB888:
				fmt_cfg = 0;
				swap_rb = 0;
				break;
			case XBGR888:
			case ABGR888:
				fmt_cfg = 0;
				swap_rb = 1;
				break;
			case RGB888:
				fmt_cfg = 1;
				swap_rb = 0;
				break;
			case RGB565:
				fmt_cfg = 2;
				swap_rb = 0;
				break;
			default:
				dev_err(vop_dev->driver.dev,
					"%s:un supported format!\n", __func__);
				spin_unlock(&vop_dev->reg_lock);
				return -EINVAL;
			}
			win->area[i].fmt_cfg = fmt_cfg;
			win->area[i].swap_rb = swap_rb;
			win->area[i].dsp_stx = dsp_x_pos(win->xmirror, screen,
							 &win->area[i]);
			win->area[i].dsp_sty = dsp_y_pos(win->ymirror, screen,
							 &win->area[i]);
			if (((win->area[i].xact != win->area[i].xsize) ||
			     (win->area[i].yact != win->area[i].ysize)) &&
			    (screen->mode.vmode == FB_VMODE_NONINTERLACED)) {
				pr_err("win[%d]->area[%d],not support scale\n",
				       win->id, i);
				pr_err("xact=%d,yact=%d,xsize=%d,ysize=%d\n",
				       win->area[i].xact, win->area[i].yact,
				       win->area[i].xsize, win->area[i].ysize);
				win->area[i].xsize = win->area[i].xact;
				win->area[i].ysize = win->area[i].yact;
			}
			DBG(2, "fmt:%s:xsize:%d>>ysize:%d>>xpos:%d>>ypos:%d\n",
			    get_format_string(win->area[i].format, fmt),
			    win->area[i].xsize, win->area[i].ysize,
			    win->area[i].xpos, win->area[i].ypos);
		}
	}
	if (win->area[0].fbdc_en)
		vop_init_fbdc_config(vop_dev, win->id);
	vop_win_2_3_reg_update(&vop_dev->driver, win->id);
	spin_unlock(&vop_dev->reg_lock);
	return 0;
}

static int hwc_set_par(struct vop_device *vop_dev,
		       struct rk_screen *screen, struct rk_lcdc_win *win)
{
	u32 xact = 0, yact = 0, xvir = 0, yvir = 0, xpos = 0, ypos = 0;
	u8 fmt_cfg = 0, swap_rb = 0;
	char fmt[9] = "NULL";

	xpos = win->area[0].xpos + screen->mode.left_margin +
	    screen->mode.hsync_len;
	ypos = win->area[0].ypos + screen->mode.upper_margin +
	    screen->mode.vsync_len;

	spin_lock(&vop_dev->reg_lock);
	if (likely(vop_dev->clk_on)) {
		switch (win->area[0].format) {
		case ARGB888:
			fmt_cfg = 0;
			swap_rb = 0;
			break;
		case XBGR888:
		case ABGR888:
			fmt_cfg = 0;
			swap_rb = 1;
			break;
		case RGB888:
			fmt_cfg = 1;
			swap_rb = 0;
			break;
		case RGB565:
			fmt_cfg = 2;
			swap_rb = 0;
			break;
		default:
			dev_err(vop_dev->dev, "%s:un supported format[%d]!\n",
				__func__, win->area[0].format);
			break;
		}
		win->area[0].fmt_cfg = fmt_cfg;
		win->area[0].swap_rb = swap_rb;
		win->area[0].dsp_stx = xpos;
		win->area[0].dsp_sty = ypos;
		xact = win->area[0].xact;
		yact = win->area[0].yact;
		xvir = win->area[0].xvir;
		yvir = win->area[0].yvir;
	}
	vop_hwc_reg_update(&vop_dev->driver, 4);
	spin_unlock(&vop_dev->reg_lock);

	DBG(1, "lcdc[%d]:hwc>>%s\n>>format:%s>>>xact:%d>>yact:%d>>xsize:%d",
	    vop_dev->id, __func__, get_format_string(win->area[0].format, fmt),
	    xact, yact, win->area[0].xsize);
	DBG(1, ">>ysize:%d>>xvir:%d>>yvir:%d>>xpos:%d>>ypos:%d>>\n",
	    win->area[0].ysize, xvir, yvir, xpos, ypos);
	return 0;
}

static int vop_set_par(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = NULL;
	struct rk_screen *screen = dev_drv->cur_screen;

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	win = dev_drv->win[win_id];
	if (win)
	switch (win_id) {
	case 0:
		win_0_1_set_par(vop_dev, screen, win);
		break;
	case 1:
		win_0_1_set_par(vop_dev, screen, win);
		break;
	case 2:
		win_2_3_set_par(vop_dev, screen, win);
		break;
	case 3:
		win_2_3_set_par(vop_dev, screen, win);
		break;
	case 4:
		hwc_set_par(vop_dev, screen, win);
		break;
	default:
		dev_err(dev_drv->dev, "unsupported win number:%d\n", win_id);
		break;
	}
	return 0;
}

static int vop_set_writeback(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int output_color = dev_drv->output_color;
	struct rk_screen *screen = dev_drv->cur_screen;
	struct rk_fb_reg_wb_data *wb_data;
	int xact = screen->mode.xres;
	int yact = screen->mode.yres;
	u32 fmt_cfg;
	int xsize, ysize;
	u64 v;

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	wb_data = &dev_drv->wb_data;
	if ((wb_data->xsize == 0) || (wb_data->ysize == 0))
		return 0;

	xsize = wb_data->xsize;
	ysize = wb_data->ysize;

	/*
	 * RGB overlay mode support ARGB888, RGB888, RGB565, NV12,
	 * but YUV overlay mode only support NV12, it's hard to judge RGB
	 * or YUV overlay mode by userspace, so here force only support
	 * NV12 mode.
	 */
	if (wb_data->data_format != YUV420 && output_color != COLOR_RGB) {
		pr_err("writeback only support NV12 when overlay is not RGB\n");
		return -EINVAL;
	}

	if (ysize != yact && ysize != (yact / 2)) {
		pr_err("WriteBack only support yact=%d, ysize=%d\n",
		       yact, ysize);
		return -EINVAL;
	}

	switch (wb_data->data_format) {
	case ARGB888:
	case ABGR888:
	case XRGB888:
	case XBGR888:
		fmt_cfg = 0;
		break;
	case RGB888:
	case BGR888:
		fmt_cfg = 1;
		break;
	case RGB565:
	case BGR565:
		fmt_cfg = 2;
		break;
	case YUV420:
		fmt_cfg = 8;
		break;
	default:
		pr_info("unsupport fmt: %d\n", wb_data->data_format);
		return -EINVAL;
	}

	v = V_WB_EN(wb_data->state) | V_WB_FMT(fmt_cfg) | V_WB_RGB2YUV_MODE(1) |
		V_WB_XPSD_BIL_EN(xact != xsize) |
		V_WB_YTHROW_EN(ysize == (yact / 2)) |
		V_WB_YTHROW_MODE(0);

	v |= V_WB_RGB2YUV_EN((output_color == COLOR_RGB) &&
			     (wb_data->data_format == YUV420));

	vop_msk_reg(vop_dev, WB_CTRL0, v);

	v = V_WB_WIDTH(xsize) | V_WB_XPSD_BIL_FACTOR((xact << 12) / xsize);

	vop_msk_reg(vop_dev, WB_CTRL1, v);

	vop_writel(vop_dev, WB_YRGB_MST, wb_data->smem_start);
	if (wb_data->data_format == YUV420)
		vop_writel(vop_dev, WB_CBR_MST, wb_data->smem_start);

	return 0;
}

static int vop_ioctl(struct rk_lcdc_driver *dev_drv, unsigned int cmd,
		     unsigned long arg, int win_id)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	u32 panel_size[2];
	void __user *argp = (void __user *)arg;
	struct color_key_cfg clr_key_cfg;

	switch (cmd) {
	case RK_FBIOGET_PANEL_SIZE:
		panel_size[0] = vop_dev->screen->mode.xres;
		panel_size[1] = vop_dev->screen->mode.yres;
		if (copy_to_user(argp, panel_size, 8))
			return -EFAULT;
		break;
	case RK_FBIOPUT_COLOR_KEY_CFG:
		if (copy_from_user(&clr_key_cfg, argp, sizeof(clr_key_cfg)))
			return -EFAULT;
		vop_clr_key_cfg(dev_drv);
		vop_writel(vop_dev, WIN0_COLOR_KEY,
			   clr_key_cfg.win0_color_key_cfg);
		vop_writel(vop_dev, WIN1_COLOR_KEY,
			   clr_key_cfg.win1_color_key_cfg);
		break;

	default:
		break;
	}
	return 0;
}

static int vop_get_backlight_device(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev = container_of(dev_drv,
						    struct vop_device, driver);
	struct device_node *backlight;
	struct property *prop;
	u32 *brightness_levels;
	u32 length, max, last;

	if (vop_dev->backlight)
		return 0;
	backlight = of_parse_phandle(vop_dev->dev->of_node, "backlight", 0);
	if (backlight) {
		vop_dev->backlight = of_find_backlight_by_node(backlight);
		if (!vop_dev->backlight)
			dev_info(vop_dev->dev, "No find backlight device\n");
	} else {
		dev_info(vop_dev->dev, "No find backlight device node\n");
	}
	prop = of_find_property(backlight, "brightness-levels", &length);
	if (!prop)
		return -EINVAL;
	max = length / sizeof(u32);
	last = max - 1;
	brightness_levels = kmalloc(256, GFP_KERNEL);
	if (!brightness_levels)
		return -ENOMEM;

	if (!of_property_read_u32_array(backlight, "brightness-levels",
					brightness_levels, max)) {
		if (brightness_levels[0] > brightness_levels[last])
			dev_drv->cabc_pwm_pol = 1;/*negative*/
		else
			dev_drv->cabc_pwm_pol = 0;/*positive*/
	} else {
		dev_info(vop_dev->dev,
			 "Can not read brightness-levels value\n");
	}

	kfree(brightness_levels);

	return 0;
}

static int vop_early_suspend(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (dev_drv->suspend_flag)
		return 0;

	dev_drv->suspend_flag = 1;
	/* ensure suspend_flag take effect on multi process */
	smp_wmb();
	flush_kthread_worker(&dev_drv->update_regs_worker);

	if (dev_drv->trsm_ops && dev_drv->trsm_ops->disable)
		dev_drv->trsm_ops->disable();

	if (likely(vop_dev->clk_on)) {
		spin_lock(&vop_dev->reg_lock);
		vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_BLANK_EN(1));
		vop_mask_writel(vop_dev, INTR_CLEAR0, INTR_MASK, INTR_MASK);
		vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_OUT_ZERO(1));
		vop_msk_reg(vop_dev, SYS_CTRL, V_VOP_STANDBY_EN(1));
		if (VOP_CHIP(vop_dev) == VOP_RK3399) {
			vop_msk_reg(vop_dev, WIN0_CTRL0, V_WIN0_EN(0));
			vop_msk_reg(vop_dev, WIN1_CTRL0, V_WIN1_EN(0));
			vop_msk_reg(vop_dev, WIN2_CTRL0, V_WIN2_EN(0));
			vop_msk_reg(vop_dev, WIN3_CTRL0, V_WIN3_EN(0));
			vop_msk_reg(vop_dev, AFBCD0_CTRL, V_VOP_FBDC_EN(0));
		}
		vop_cfg_done(vop_dev);

		if (dev_drv->iommu_enabled && dev_drv->mmu_dev) {
			mdelay(50);
			rockchip_iovmm_deactivate(dev_drv->dev);
		}

		spin_unlock(&vop_dev->reg_lock);
	}

	vop_clk_disable(vop_dev);
	rk_disp_pwr_disable(dev_drv);

	return 0;
}

static int vop_early_resume(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (!dev_drv->suspend_flag)
		return 0;
	rk_disp_pwr_enable(dev_drv);

	vop_clk_enable(vop_dev);
	spin_lock(&vop_dev->reg_lock);
	memcpy(vop_dev->regs, vop_dev->regsbak, vop_dev->len);
	spin_unlock(&vop_dev->reg_lock);

	vop_set_lut(dev_drv, dev_drv->cur_screen->dsp_lut);
	vop_set_cabc(dev_drv, dev_drv->cur_screen->cabc_lut);
	spin_lock(&vop_dev->reg_lock);

	vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_OUT_ZERO(0));
	vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_BLANK_EN(0));
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);

	if (dev_drv->iommu_enabled && dev_drv->mmu_dev) {
		/* win address maybe effect after next frame start,
		 * but mmu maybe effect right now, so we delay 50ms
		 */
		mdelay(50);
		rockchip_iovmm_activate(dev_drv->dev);
	}

	spin_lock(&vop_dev->reg_lock);
	vop_msk_reg(vop_dev, SYS_CTRL, V_VOP_STANDBY_EN(0));
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);

	dev_drv->suspend_flag = 0;

	if (dev_drv->trsm_ops && dev_drv->trsm_ops->enable)
		dev_drv->trsm_ops->enable();

	return 0;
}

static int vop_blank(struct rk_lcdc_driver *dev_drv, int win_id, int blank_mode)
{
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		vop_early_resume(dev_drv);
		break;
	case FB_BLANK_NORMAL:
		vop_early_suspend(dev_drv);
		break;
	default:
		vop_early_suspend(dev_drv);
		break;
	}

	dev_info(dev_drv->dev, "blank mode:%d\n", blank_mode);

	return 0;
}

static int vop_get_win_state(struct rk_lcdc_driver *dev_drv,
			     int win_id, int area_id)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	u32 area_status = 0, state = 0;

	switch (win_id) {
	case 0:
		area_status = vop_read_bit(vop_dev, WIN0_CTRL0, V_WIN0_EN(0));
		break;
	case 1:
		area_status = vop_read_bit(vop_dev, WIN1_CTRL0, V_WIN1_EN(0));
		break;
	case 2:
		if (area_id == 0)
			area_status = vop_read_bit(vop_dev, WIN2_CTRL0,
						   V_WIN2_MST0_EN(0));
		if (area_id == 1)
			area_status = vop_read_bit(vop_dev, WIN2_CTRL0,
						   V_WIN2_MST1_EN(0));
		if (area_id == 2)
			area_status = vop_read_bit(vop_dev, WIN2_CTRL0,
						   V_WIN2_MST2_EN(0));
		if (area_id == 3)
			area_status = vop_read_bit(vop_dev, WIN2_CTRL0,
						   V_WIN2_MST3_EN(0));
		break;
	case 3:
		if (area_id == 0)
			area_status = vop_read_bit(vop_dev, WIN3_CTRL0,
						   V_WIN3_MST0_EN(0));
		if (area_id == 1)
			area_status = vop_read_bit(vop_dev, WIN3_CTRL0,
						   V_WIN3_MST1_EN(0));
		if (area_id == 2)
			area_status = vop_read_bit(vop_dev, WIN3_CTRL0,
						   V_WIN3_MST2_EN(0));
		if (area_id == 3)
			area_status = vop_read_bit(vop_dev, WIN3_CTRL0,
						   V_WIN3_MST3_EN(0));
		break;
	case 4:
		area_status = vop_read_bit(vop_dev, HWC_CTRL0, V_HWC_EN(0));
		break;
	default:
		pr_err("!!!%s,win[%d]area[%d],unsupport!!!\n",
		       __func__, win_id, area_id);
		break;
	}

	state = (area_status > 0) ? 1 : 0;
	return state;
}

static int vop_get_area_num(struct rk_lcdc_driver *dev_drv,
			    unsigned int *area_support)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	area_support[0] = 1;
	area_support[1] = 1;

	if (VOP_CHIP(vop_dev) == VOP_RK3399) {
		area_support[2] = 4;
		area_support[3] = 4;
	}

	return 0;
}

/*overlay will be do at regupdate*/
static int vop_ovl_mgr(struct rk_lcdc_driver *dev_drv, int swap, bool set)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_lcdc_win *win = NULL;
	int i, ovl = 0;
	u64 val;
	int z_order_num = 0;
	int layer0_sel = 0, layer1_sel = 1, layer2_sel = 2, layer3_sel = 3;

	if (swap == 0) {
		for (i = 0; i < dev_drv->lcdc_win_num; i++) {
			win = dev_drv->win[i];
			if (win->state == 1)
				z_order_num++;
		}
		for (i = 0; i < dev_drv->lcdc_win_num; i++) {
			win = dev_drv->win[i];
			if (win->state == 0)
				win->z_order = z_order_num++;
			switch (win->z_order) {
			case 0:
				layer0_sel = win->id;
				break;
			case 1:
				layer1_sel = win->id;
				break;
			case 2:
				layer2_sel = win->id;
				break;
			case 3:
				layer3_sel = win->id;
				break;
			default:
				break;
			}
		}
	} else {
		layer0_sel = swap % 10;
		layer1_sel = swap / 10 % 10;
		layer2_sel = swap / 100 % 10;
		layer3_sel = swap / 1000;
	}

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		if (set) {
			val = V_DSP_LAYER0_SEL(layer0_sel) |
			    V_DSP_LAYER1_SEL(layer1_sel) |
			    V_DSP_LAYER2_SEL(layer2_sel) |
			    V_DSP_LAYER3_SEL(layer3_sel);
			vop_msk_reg(vop_dev, DSP_CTRL1, val);
		} else {
			layer0_sel = vop_read_bit(vop_dev, DSP_CTRL1,
						  V_DSP_LAYER0_SEL(0));
			layer1_sel = vop_read_bit(vop_dev, DSP_CTRL1,
						  V_DSP_LAYER1_SEL(0));
			layer2_sel = vop_read_bit(vop_dev, DSP_CTRL1,
						  V_DSP_LAYER2_SEL(0));
			layer3_sel = vop_read_bit(vop_dev, DSP_CTRL1,
						  V_DSP_LAYER3_SEL(0));
			ovl = layer3_sel * 1000 + layer2_sel * 100 +
			    layer1_sel * 10 + layer0_sel;
		}
	} else {
		ovl = -EPERM;
	}
	spin_unlock(&vop_dev->reg_lock);

	return ovl;
}

static char *vop_format_to_string(int format, char *fmt)
{
	if (!fmt)
		return NULL;

	switch (format) {
	case 0:
		strcpy(fmt, "ARGB888");
		break;
	case 1:
		strcpy(fmt, "RGB888");
		break;
	case 2:
		strcpy(fmt, "RGB565");
		break;
	case 4:
		strcpy(fmt, "YCbCr420");
		break;
	case 5:
		strcpy(fmt, "YCbCr422");
		break;
	case 6:
		strcpy(fmt, "YCbCr444");
	case 8:
		strcpy(fmt, "YUYV422");
		break;
	case 9:
		strcpy(fmt, "YUYV420");
		break;
	case 10:
		strcpy(fmt, "UYVY422");
		break;
	case 11:
		strcpy(fmt, "UYVY420");
		break;
	default:
		strcpy(fmt, "invalid\n");
		break;
	}
	return fmt;
}

static ssize_t vop_get_disp_info(struct rk_lcdc_driver *dev_drv,
				 char *buf, int win_id)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
	u16 hsync_len = screen->mode.hsync_len;
	u16 left_margin = screen->mode.left_margin;
	u16 vsync_len = screen->mode.vsync_len;
	u16 upper_margin = screen->mode.upper_margin;
	u32 h_pw_bp = hsync_len + left_margin;
	u32 v_pw_bp = vsync_len + upper_margin;
	u32 fmt_id;
	char format_w0[9] = "NULL";
	char format_w1[9] = "NULL";
	char format_w2_0[9] = "NULL";
	char format_w2_1[9] = "NULL";
	char format_w2_2[9] = "NULL";
	char format_w2_3[9] = "NULL";
	char format_w3_0[9] = "NULL";
	char format_w3_1[9] = "NULL";
	char format_w3_2[9] = "NULL";
	char format_w3_3[9] = "NULL";
	char dsp_buf[100];
	u32 win_ctrl, zorder, vir_info, act_info, dsp_info, dsp_st;
	u32 y_factor, uv_factor;
	u8 layer0_sel, layer1_sel, layer2_sel, layer3_sel;
	u8 w0_state, w1_state, w2_state, w3_state;
	u8 w2_0_state, w2_1_state, w2_2_state, w2_3_state;
	u8 w3_0_state, w3_1_state, w3_2_state, w3_3_state;

	u32 w0_vir_y, w0_vir_uv, w0_act_x, w0_act_y, w0_dsp_x, w0_dsp_y;
	u32 w0_st_x = h_pw_bp, w0_st_y = v_pw_bp;
	u32 w1_vir_y, w1_vir_uv, w1_act_x, w1_act_y, w1_dsp_x, w1_dsp_y;
	u32 w1_st_x = h_pw_bp, w1_st_y = v_pw_bp;
	u32 w0_y_h_fac, w0_y_v_fac, w0_uv_h_fac, w0_uv_v_fac;
	u32 w1_y_h_fac, w1_y_v_fac, w1_uv_h_fac, w1_uv_v_fac;

	u32 w2_0_vir_y, w2_1_vir_y, w2_2_vir_y, w2_3_vir_y;
	u32 w2_0_dsp_x, w2_1_dsp_x, w2_2_dsp_x, w2_3_dsp_x;
	u32 w2_0_dsp_y, w2_1_dsp_y, w2_2_dsp_y, w2_3_dsp_y;
	u32 w2_0_st_x = h_pw_bp, w2_1_st_x = h_pw_bp;
	u32 w2_2_st_x = h_pw_bp, w2_3_st_x = h_pw_bp;
	u32 w2_0_st_y = v_pw_bp, w2_1_st_y = v_pw_bp;
	u32 w2_2_st_y = v_pw_bp, w2_3_st_y = v_pw_bp;

	u32 w3_0_vir_y, w3_1_vir_y, w3_2_vir_y, w3_3_vir_y;
	u32 w3_0_dsp_x, w3_1_dsp_x, w3_2_dsp_x, w3_3_dsp_x;
	u32 w3_0_dsp_y, w3_1_dsp_y, w3_2_dsp_y, w3_3_dsp_y;
	u32 w3_0_st_x = h_pw_bp, w3_1_st_x = h_pw_bp;
	u32 w3_2_st_x = h_pw_bp, w3_3_st_x = h_pw_bp;
	u32 w3_0_st_y = v_pw_bp, w3_1_st_y = v_pw_bp;
	u32 w3_2_st_y = v_pw_bp, w3_3_st_y = v_pw_bp;
	u32 dclk_freq;
	int size = 0;

	dclk_freq = screen->mode.pixclock;
	/*vop_reg_dump(dev_drv); */

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		zorder = vop_readl(vop_dev, DSP_CTRL1);
		layer0_sel = (zorder & MASK(DSP_LAYER0_SEL)) >> 8;
		layer1_sel = (zorder & MASK(DSP_LAYER1_SEL)) >> 10;
		layer2_sel = (zorder & MASK(DSP_LAYER2_SEL)) >> 12;
		layer3_sel = (zorder & MASK(DSP_LAYER3_SEL)) >> 14;
		/* WIN0 */
		win_ctrl = vop_readl(vop_dev, WIN0_CTRL0);
		w0_state = win_ctrl & MASK(WIN0_EN);
		fmt_id = (win_ctrl & MASK(WIN0_DATA_FMT)) >> 1;
		fmt_id |= (win_ctrl & MASK(WIN0_YUYV)) >> 14; /* yuyv*/
		vop_format_to_string(fmt_id, format_w0);
		vir_info = vop_readl(vop_dev, WIN0_VIR);
		act_info = vop_readl(vop_dev, WIN0_ACT_INFO);
		dsp_info = vop_readl(vop_dev, WIN0_DSP_INFO);
		dsp_st = vop_readl(vop_dev, WIN0_DSP_ST);
		y_factor = vop_readl(vop_dev, WIN0_SCL_FACTOR_YRGB);
		uv_factor = vop_readl(vop_dev, WIN0_SCL_FACTOR_CBR);
		w0_vir_y = vir_info & MASK(WIN0_VIR_STRIDE);
		w0_vir_uv = (vir_info & MASK(WIN0_VIR_STRIDE_UV)) >> 16;
		w0_act_x = (act_info & MASK(WIN0_ACT_WIDTH)) + 1;
		w0_act_y = ((act_info & MASK(WIN0_ACT_HEIGHT)) >> 16) + 1;
		w0_dsp_x = (dsp_info & MASK(WIN0_DSP_WIDTH)) + 1;
		w0_dsp_y = ((dsp_info & MASK(WIN0_DSP_HEIGHT)) >> 16) + 1;
		if (w0_state) {
			w0_st_x = dsp_st & MASK(WIN0_DSP_XST);
			w0_st_y = (dsp_st & MASK(WIN0_DSP_YST)) >> 16;
		}
		w0_y_h_fac = y_factor & MASK(WIN0_HS_FACTOR_YRGB);
		w0_y_v_fac = (y_factor & MASK(WIN0_VS_FACTOR_YRGB)) >> 16;
		w0_uv_h_fac = uv_factor & MASK(WIN0_HS_FACTOR_CBR);
		w0_uv_v_fac = (uv_factor & MASK(WIN0_VS_FACTOR_CBR)) >> 16;

		/* WIN1 */
		win_ctrl = vop_readl(vop_dev, WIN1_CTRL0);
		w1_state = win_ctrl & MASK(WIN1_EN);
		fmt_id = (win_ctrl & MASK(WIN1_DATA_FMT)) >> 1;
		fmt_id |= (win_ctrl & MASK(WIN1_YUYV)) >> 14; /* yuyv*/
		vop_format_to_string(fmt_id, format_w1);
		vir_info = vop_readl(vop_dev, WIN1_VIR);
		act_info = vop_readl(vop_dev, WIN1_ACT_INFO);
		dsp_info = vop_readl(vop_dev, WIN1_DSP_INFO);
		dsp_st = vop_readl(vop_dev, WIN1_DSP_ST);
		y_factor = vop_readl(vop_dev, WIN1_SCL_FACTOR_YRGB);
		uv_factor = vop_readl(vop_dev, WIN1_SCL_FACTOR_CBR);
		w1_vir_y = vir_info & MASK(WIN1_VIR_STRIDE);
		w1_vir_uv = (vir_info & MASK(WIN1_VIR_STRIDE_UV)) >> 16;
		w1_act_x = (act_info & MASK(WIN1_ACT_WIDTH)) + 1;
		w1_act_y = ((act_info & MASK(WIN1_ACT_HEIGHT)) >> 16) + 1;
		w1_dsp_x = (dsp_info & MASK(WIN1_DSP_WIDTH)) + 1;
		w1_dsp_y = ((dsp_info & MASK(WIN1_DSP_HEIGHT)) >> 16) + 1;
		if (w1_state) {
			w1_st_x = dsp_st & MASK(WIN1_DSP_XST);
			w1_st_y = (dsp_st & MASK(WIN1_DSP_YST)) >> 16;
		}
		w1_y_h_fac = y_factor & MASK(WIN1_HS_FACTOR_YRGB);
		w1_y_v_fac = (y_factor & MASK(WIN1_VS_FACTOR_YRGB)) >> 16;
		w1_uv_h_fac = uv_factor & MASK(WIN1_HS_FACTOR_CBR);
		w1_uv_v_fac = (uv_factor & MASK(WIN1_VS_FACTOR_CBR)) >> 16;

		/*WIN2 */
		win_ctrl = vop_readl(vop_dev, WIN2_CTRL0);
		w2_state = win_ctrl & MASK(WIN2_EN);
		w2_0_state = (win_ctrl & 0x10) >> 4;
		w2_1_state = (win_ctrl & 0x100) >> 8;
		w2_2_state = (win_ctrl & 0x1000) >> 12;
		w2_3_state = (win_ctrl & 0x10000) >> 16;
		vir_info = vop_readl(vop_dev, WIN2_VIR0_1);
		w2_0_vir_y = vir_info & MASK(WIN2_VIR_STRIDE0);
		w2_1_vir_y = (vir_info & MASK(WIN2_VIR_STRIDE1)) >> 16;
		vir_info = vop_readl(vop_dev, WIN2_VIR2_3);
		w2_2_vir_y = vir_info & MASK(WIN2_VIR_STRIDE2);
		w2_3_vir_y = (vir_info & MASK(WIN2_VIR_STRIDE3)) >> 16;

		fmt_id = (win_ctrl & MASK(WIN2_DATA_FMT0)) >> 5;
		vop_format_to_string(fmt_id, format_w2_0);
		fmt_id = (win_ctrl & MASK(WIN2_DATA_FMT1)) >> 9;
		vop_format_to_string(fmt_id, format_w2_1);
		fmt_id = (win_ctrl & MASK(WIN2_DATA_FMT2)) >> 13;
		vop_format_to_string(fmt_id, format_w2_2);
		fmt_id = (win_ctrl & MASK(WIN2_DATA_FMT3)) >> 17;
		vop_format_to_string(fmt_id, format_w2_3);

		dsp_info = vop_readl(vop_dev, WIN2_DSP_INFO0);
		dsp_st = vop_readl(vop_dev, WIN2_DSP_ST0);
		w2_0_dsp_x = (dsp_info & MASK(WIN2_DSP_WIDTH0)) + 1;
		w2_0_dsp_y = ((dsp_info & MASK(WIN2_DSP_HEIGHT0)) >> 16) + 1;
		if (w2_0_state) {
			w2_0_st_x = dsp_st & MASK(WIN2_DSP_XST0);
			w2_0_st_y = (dsp_st & MASK(WIN2_DSP_YST0)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN2_DSP_INFO1);
		dsp_st = vop_readl(vop_dev, WIN2_DSP_ST1);
		w2_1_dsp_x = (dsp_info & MASK(WIN2_DSP_WIDTH1)) + 1;
		w2_1_dsp_y = ((dsp_info & MASK(WIN2_DSP_HEIGHT1)) >> 16) + 1;
		if (w2_1_state) {
			w2_1_st_x = dsp_st & MASK(WIN2_DSP_XST1);
			w2_1_st_y = (dsp_st & MASK(WIN2_DSP_YST1)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN2_DSP_INFO2);
		dsp_st = vop_readl(vop_dev, WIN2_DSP_ST2);
		w2_2_dsp_x = (dsp_info & MASK(WIN2_DSP_WIDTH2)) + 1;
		w2_2_dsp_y = ((dsp_info & MASK(WIN2_DSP_HEIGHT2)) >> 16) + 1;
		if (w2_2_state) {
			w2_2_st_x = dsp_st & MASK(WIN2_DSP_XST2);
			w2_2_st_y = (dsp_st & MASK(WIN2_DSP_YST2)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN2_DSP_INFO3);
		dsp_st = vop_readl(vop_dev, WIN2_DSP_ST3);
		w2_3_dsp_x = (dsp_info & MASK(WIN2_DSP_WIDTH3)) + 1;
		w2_3_dsp_y = ((dsp_info & MASK(WIN2_DSP_HEIGHT3)) >> 16) + 1;
		if (w2_3_state) {
			w2_3_st_x = dsp_st & MASK(WIN2_DSP_XST3);
			w2_3_st_y = (dsp_st & MASK(WIN2_DSP_YST3)) >> 16;
		}

		/*WIN3 */
		win_ctrl = vop_readl(vop_dev, WIN3_CTRL0);
		w3_state = win_ctrl & MASK(WIN3_EN);
		w3_0_state = (win_ctrl & 0x10) >> 4;
		w3_1_state = (win_ctrl & 0x100) >> 8;
		w3_2_state = (win_ctrl & 0x1000) >> 12;
		w3_3_state = (win_ctrl & 0x10000) >> 16;
		vir_info = vop_readl(vop_dev, WIN3_VIR0_1);
		w3_0_vir_y = vir_info & MASK(WIN3_VIR_STRIDE0);
		w3_1_vir_y = (vir_info & MASK(WIN3_VIR_STRIDE1)) >> 16;
		vir_info = vop_readl(vop_dev, WIN3_VIR2_3);
		w3_2_vir_y = vir_info & MASK(WIN3_VIR_STRIDE2);
		w3_3_vir_y = (vir_info & MASK(WIN3_VIR_STRIDE3)) >> 16;

		fmt_id = (win_ctrl & MASK(WIN3_DATA_FMT0)) >> 5;
		vop_format_to_string(fmt_id, format_w3_0);
		fmt_id = (win_ctrl & MASK(WIN3_DATA_FMT1)) >> 9;
		vop_format_to_string(fmt_id, format_w3_1);
		fmt_id = (win_ctrl & MASK(WIN3_DATA_FMT2)) >> 13;
		vop_format_to_string(fmt_id, format_w3_2);
		fmt_id = (win_ctrl & MASK(WIN3_DATA_FMT3)) >> 17;
		vop_format_to_string(fmt_id, format_w3_3);

		dsp_info = vop_readl(vop_dev, WIN3_DSP_INFO0);
		dsp_st = vop_readl(vop_dev, WIN3_DSP_ST0);
		w3_0_dsp_x = (dsp_info & MASK(WIN3_DSP_WIDTH0)) + 1;
		w3_0_dsp_y = ((dsp_info & MASK(WIN3_DSP_HEIGHT0)) >> 16) + 1;
		if (w3_0_state) {
			w3_0_st_x = dsp_st & MASK(WIN3_DSP_XST0);
			w3_0_st_y = (dsp_st & MASK(WIN3_DSP_YST0)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN3_DSP_INFO1);
		dsp_st = vop_readl(vop_dev, WIN3_DSP_ST1);
		w3_1_dsp_x = (dsp_info & MASK(WIN3_DSP_WIDTH1)) + 1;
		w3_1_dsp_y = ((dsp_info & MASK(WIN3_DSP_HEIGHT1)) >> 16) + 1;
		if (w3_1_state) {
			w3_1_st_x = dsp_st & MASK(WIN3_DSP_XST1);
			w3_1_st_y = (dsp_st & MASK(WIN3_DSP_YST1)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN3_DSP_INFO2);
		dsp_st = vop_readl(vop_dev, WIN3_DSP_ST2);
		w3_2_dsp_x = (dsp_info & MASK(WIN3_DSP_WIDTH2)) + 1;
		w3_2_dsp_y = ((dsp_info & MASK(WIN3_DSP_HEIGHT2)) >> 16) + 1;
		if (w3_2_state) {
			w3_2_st_x = dsp_st & MASK(WIN3_DSP_XST2);
			w3_2_st_y = (dsp_st & MASK(WIN3_DSP_YST2)) >> 16;
		}
		dsp_info = vop_readl(vop_dev, WIN3_DSP_INFO3);
		dsp_st = vop_readl(vop_dev, WIN3_DSP_ST3);
		w3_3_dsp_x = (dsp_info & MASK(WIN3_DSP_WIDTH3)) + 1;
		w3_3_dsp_y = ((dsp_info & MASK(WIN3_DSP_HEIGHT3)) >> 16) + 1;
		if (w3_3_state) {
			w3_3_st_x = dsp_st & MASK(WIN3_DSP_XST3);
			w3_3_st_y = (dsp_st & MASK(WIN3_DSP_YST3)) >> 16;
		}
	} else {
		spin_unlock(&vop_dev->reg_lock);
		return -EPERM;
	}
	spin_unlock(&vop_dev->reg_lock);
	size += snprintf(dsp_buf, 80,
		"z-order:\n  win[%d]\n  win[%d]\n  win[%d]\n  win[%d]\n",
		layer3_sel, layer2_sel, layer1_sel, layer0_sel);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	/* win0 */
	size += snprintf(dsp_buf, 80,
		 "win0:\n  state:%d, fmt:%7s\n  y_vir:%4d, uv_vir:%4d,",
		 w0_state, format_w0, w0_vir_y, w0_vir_uv);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 " x_act  :%5d, y_act  :%5d, dsp_x   :%5d, dsp_y   :%5d\n",
		 w0_act_x, w0_act_y, w0_dsp_x, w0_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 "  x_st :%4d, y_st  :%4d, y_h_fac:%5d, y_v_fac:%5d, ",
		 w0_st_x - h_pw_bp, w0_st_y - v_pw_bp, w0_y_h_fac, w0_y_v_fac);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 "uv_h_fac:%5d, uv_v_fac:%5d\n  y_addr:0x%08x,    uv_addr:0x%08x\n",
		 w0_uv_h_fac, w0_uv_v_fac, vop_readl(vop_dev, WIN0_YRGB_MST),
		 vop_readl(vop_dev, WIN0_CBR_MST));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/* win1 */
	size += snprintf(dsp_buf, 80,
		 "win1:\n  state:%d, fmt:%7s\n  y_vir:%4d, uv_vir:%4d,",
		 w1_state, format_w1, w1_vir_y, w1_vir_uv);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 " x_act  :%5d, y_act  :%5d, dsp_x   :%5d, dsp_y   :%5d\n",
		 w1_act_x, w1_act_y, w1_dsp_x, w1_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 "  x_st :%4d, y_st  :%4d, y_h_fac:%5d, y_v_fac:%5d, ",
		 w1_st_x - h_pw_bp, w1_st_y - v_pw_bp, w1_y_h_fac, w1_y_v_fac);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	size += snprintf(dsp_buf, 80,
		 "uv_h_fac:%5d, uv_v_fac:%5d\n  y_addr:0x%08x,    uv_addr:0x%08x\n",
		 w1_uv_h_fac, w1_uv_v_fac, vop_readl(vop_dev, WIN1_YRGB_MST),
		 vop_readl(vop_dev, WIN1_CBR_MST));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*win2*/
	size += snprintf(dsp_buf, 80,
		 "win2:\n  state:%d\n",
		 w2_state);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	/*area 0*/
	size += snprintf(dsp_buf, 80,
		 "  area0: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w2_0_state, format_w2_0, w2_0_dsp_x, w2_0_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w2_0_st_x - h_pw_bp, w2_0_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN2_MST0));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 1*/
	size += snprintf(dsp_buf, 80,
		 "  area1: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w2_1_state, format_w2_1, w2_1_dsp_x, w2_1_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w2_1_st_x - h_pw_bp, w2_1_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN2_MST1));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 2*/
	size += snprintf(dsp_buf, 80,
		 "  area2: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w2_2_state, format_w2_2, w2_2_dsp_x, w2_2_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w2_2_st_x - h_pw_bp, w2_2_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN2_MST2));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 3*/
	size += snprintf(dsp_buf, 80,
		 "  area3: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w2_3_state, format_w2_3, w2_3_dsp_x, w2_3_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w2_3_st_x - h_pw_bp, w2_3_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN2_MST3));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*win3*/
	size += snprintf(dsp_buf, 80,
		 "win3:\n  state:%d\n",
		 w3_state);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	/*area 0*/
	size += snprintf(dsp_buf, 80,
		 "  area0: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w3_0_state, format_w3_0, w3_0_dsp_x, w3_0_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w3_0_st_x - h_pw_bp, w3_0_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN3_MST0));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 1*/
	size += snprintf(dsp_buf, 80,
		 "  area1: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w3_1_state, format_w3_1, w3_1_dsp_x, w3_1_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w3_1_st_x - h_pw_bp, w3_1_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN3_MST1));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 2*/
	size += snprintf(dsp_buf, 80,
		 "  area2: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w3_2_state, format_w3_2, w3_2_dsp_x, w3_2_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w3_2_st_x - h_pw_bp, w3_2_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN3_MST2));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	/*area 3*/
	size += snprintf(dsp_buf, 80,
		 "  area3: state:%d, fmt:%7s, dsp_x:%4d, dsp_y:%4d,",
		 w3_3_state, format_w3_3, w3_3_dsp_x, w3_3_dsp_y);
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));
	size += snprintf(dsp_buf, 80,
		 " x_st:%4d, y_st:%4d, y_addr:0x%08x\n",
		 w3_3_st_x - h_pw_bp, w3_3_st_y - v_pw_bp,
		 vop_readl(vop_dev, WIN3_MST3));
	strcat(buf, dsp_buf);
	memset(dsp_buf, 0, sizeof(dsp_buf));

	return size;
}

static int vop_fps_mgr(struct rk_lcdc_driver *dev_drv, int fps, bool set)
{
	struct vop_device *vop_dev =
		container_of(dev_drv, struct vop_device, driver);
	struct rk_fb_vsync *vsync = &dev_drv->vsync_info;
	int step_fps, old_fps;
	u32 h_total, v_total;
	unsigned long dclk;
	u64 val;
	int ret;

	dclk = clk_get_rate(vop_dev->dclk);

	spin_lock(&vop_dev->reg_lock);

	if (!vop_dev->clk_on) {
		spin_unlock(&vop_dev->reg_lock);
		return 0;
	}

	val = vop_readl(vop_dev, DSP_HTOTAL_HS_END);
	h_total = (val & MASK(DSP_HTOTAL)) >> 16;

	val = vop_readl(vop_dev, DSP_VTOTAL_VS_END);
	v_total = (val & MASK(DSP_VTOTAL)) >> 16;

	spin_unlock(&vop_dev->reg_lock);

	old_fps = div_u64(dclk, v_total * h_total);

	if (!set)
		return old_fps;

	/*
	 * Direct change fps to dest fps would may screen flash,
	 * Every frame change one step fps is safe, screen flash
	 * disappear.
	 */
	step_fps = old_fps;
	while (step_fps != fps) {
		ktime_t timestamp = vsync->timestamp;

		if (step_fps > fps)
			step_fps--;
		else
			step_fps++;
		spin_lock(&vop_dev->reg_lock);
		if (!vop_dev->clk_on) {
			spin_unlock(&vop_dev->reg_lock);
			break;
		}
		h_total = div_u64(dclk, step_fps * v_total);
		val = V_DSP_HTOTAL(h_total);
		vop_msk_reg(vop_dev, DSP_HTOTAL_HS_END, val);
		vop_cfg_done(vop_dev);
		spin_unlock(&vop_dev->reg_lock);

		ret = wait_event_interruptible_timeout(vsync->wait,
			!ktime_equal(timestamp, vsync->timestamp) &&
			(vsync->active > 0 || vsync->irq_stop),
			msecs_to_jiffies(50));
	}

	dev_info(dev_drv->dev, "%s:dclk:%lu, htotal=%d, vtatol=%d, fps:%d\n",
		 __func__, dclk, h_total, v_total, fps);

	return fps;
}

static int vop_fb_win_remap(struct rk_lcdc_driver *dev_drv, u16 order)
{
	mutex_lock(&dev_drv->fb_win_id_mutex);
	if (order == FB_DEFAULT_ORDER)
		order = FB0_WIN0_FB1_WIN1_FB2_WIN2_FB3_WIN3_FB4_HWC;
	dev_drv->fb4_win_id = order / 10000;
	dev_drv->fb3_win_id = (order / 1000) % 10;
	dev_drv->fb2_win_id = (order / 100) % 10;
	dev_drv->fb1_win_id = (order / 10) % 10;
	dev_drv->fb0_win_id = order % 10;
	mutex_unlock(&dev_drv->fb_win_id_mutex);

	return 0;
}

static int vop_get_win_id(struct rk_lcdc_driver *dev_drv, const char *id)
{
	int win_id = 0;

	mutex_lock(&dev_drv->fb_win_id_mutex);
	if (!strcmp(id, "fb0") || !strcmp(id, "fb5"))
		win_id = dev_drv->fb0_win_id;
	else if (!strcmp(id, "fb1") || !strcmp(id, "fb6"))
		win_id = dev_drv->fb1_win_id;
	else if (!strcmp(id, "fb2") || !strcmp(id, "fb7"))
		win_id = dev_drv->fb2_win_id;
	else if (!strcmp(id, "fb3") || !strcmp(id, "fb8"))
		win_id = dev_drv->fb3_win_id;
	else if (!strcmp(id, "fb4") || !strcmp(id, "fb9"))
		win_id = dev_drv->fb4_win_id;
	mutex_unlock(&dev_drv->fb_win_id_mutex);

	return win_id;
}

static int vop_config_done(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int i, fbdc_en = 0;
	u64 val;
	struct rk_lcdc_win *win = NULL;

	spin_lock(&vop_dev->reg_lock);
	vop_post_cfg(dev_drv);
	vop_msk_reg(vop_dev, SYS_CTRL, V_VOP_STANDBY_EN(vop_dev->standby));
	for (i = 0; i < dev_drv->lcdc_win_num; i++) {
		win = dev_drv->win[i];
		vop_alpha_cfg(dev_drv, i);
		fbdc_en |= win->area[0].fbdc_en;
		vop_dev->atv_layer_cnt &= ~(1 << win->id);
		vop_dev->atv_layer_cnt |= (win->state << win->id);
		if ((win->state == 0) && (win->last_state == 1)) {
			switch (win->id) {
			case 0:
				val = V_WIN0_EN(0);
				vop_msk_reg(vop_dev, WIN0_CTRL0, val);
				break;
			case 1:
				val = V_WIN1_EN(0);
				vop_msk_reg(vop_dev, WIN1_CTRL0, val);
				break;
			case 2:
				val = V_WIN2_EN(0) | V_WIN2_MST0_EN(0) |
				    V_WIN2_MST1_EN(0) |
				    V_WIN2_MST2_EN(0) | V_WIN2_MST3_EN(0);
				vop_msk_reg(vop_dev, WIN2_CTRL0, val);
				break;
			case 3:
				val = V_WIN3_EN(0) | V_WIN3_MST0_EN(0) |
				    V_WIN3_MST1_EN(0) |
				    V_WIN3_MST2_EN(0) | V_WIN3_MST3_EN(0);
				vop_msk_reg(vop_dev, WIN3_CTRL0, val);
				break;
			case 4:
				val = V_HWC_EN(0);
				vop_msk_reg(vop_dev, HWC_CTRL0, val);
				break;
			default:
				break;
			}
		}
		win->last_state = win->state;
	}
	if (VOP_CHIP(vop_dev) == VOP_RK3399) {
		val = V_VOP_FBDC_EN(fbdc_en);
		vop_msk_reg(vop_dev, AFBCD0_CTRL, val);
	}
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);
	return 0;
}

static int vop_dpi_open(struct rk_lcdc_driver *dev_drv, bool open)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	spin_lock(&vop_dev->reg_lock);
	vop_msk_reg(vop_dev, SYS_CTRL, V_DIRECT_PATH_EN(open));
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);
	return 0;
}

static int vop_dpi_win_sel(struct rk_lcdc_driver *dev_drv, int win_id)
{
	struct vop_device *vop_dev = container_of(dev_drv,
						    struct vop_device, driver);
	spin_lock(&vop_dev->reg_lock);
	vop_msk_reg(vop_dev, SYS_CTRL, V_DIRECT_PATH_LAYER_SEL(win_id));
	vop_cfg_done(vop_dev);
	spin_unlock(&vop_dev->reg_lock);
	return 0;
}

static int vop_dpi_status(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	int ovl;

	spin_lock(&vop_dev->reg_lock);
	ovl = vop_read_bit(vop_dev, SYS_CTRL, V_DIRECT_PATH_EN(0));
	spin_unlock(&vop_dev->reg_lock);
	return ovl;
}

static int vop_set_irq_to_cpu(struct rk_lcdc_driver *dev_drv, int enable)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	if (enable)
		enable_irq(vop_dev->irq);
	else
		disable_irq_nosync(vop_dev->irq);
	return 0;
}

int vop_poll_vblank(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u32 int_reg;
	int ret;

	if (vop_dev->clk_on && (!dev_drv->suspend_flag)) {
		int_reg = vop_readl(vop_dev, INTR_STATUS0);
		if (int_reg & INTR_LINE_FLAG0) {
			vop_dev->driver.frame_time.last_framedone_t =
			    vop_dev->driver.frame_time.framedone_t;
			vop_dev->driver.frame_time.framedone_t = cpu_clock(0);
			vop_mask_writel(vop_dev, INTR_CLEAR0, INTR_LINE_FLAG0,
					INTR_LINE_FLAG0);
			ret = RK_LF_STATUS_FC;
		} else {
			ret = RK_LF_STATUS_FR;
		}
	} else {
		ret = RK_LF_STATUS_NC;
	}

	return ret;
}

static int vop_get_dsp_addr(struct rk_lcdc_driver *dev_drv,
			    unsigned int dsp_addr[][4])
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		dsp_addr[0][0] = vop_readl(vop_dev, WIN0_YRGB_MST);
		dsp_addr[1][0] = vop_readl(vop_dev, WIN1_YRGB_MST);
		dsp_addr[2][0] = vop_readl(vop_dev, WIN2_MST0);
		dsp_addr[2][1] = vop_readl(vop_dev, WIN2_MST1);
		dsp_addr[2][2] = vop_readl(vop_dev, WIN2_MST2);
		dsp_addr[2][3] = vop_readl(vop_dev, WIN2_MST3);
		dsp_addr[3][0] = vop_readl(vop_dev, WIN3_MST0);
		dsp_addr[3][1] = vop_readl(vop_dev, WIN3_MST1);
		dsp_addr[3][2] = vop_readl(vop_dev, WIN3_MST2);
		dsp_addr[3][3] = vop_readl(vop_dev, WIN3_MST3);
		dsp_addr[4][0] = vop_readl(vop_dev, HWC_MST);
	}
	spin_unlock(&vop_dev->reg_lock);
	return 0;
}


int vop_update_pwm(int bl_pwm_period, int bl_pwm_duty)
{
	/*
	 * TODO:
	 * pwm_period_hpr = bl_pwm_period;
	 * pwm_duty_lpr = bl_pwm_duty;
	 * pr_info("bl_pwm_period_hpr = 0x%x, bl_pwm_duty_lpr = 0x%x\n",
	 * bl_pwm_period, bl_pwm_duty);
	 */

	return 0;
}

/*
 *  a:[-30~0]:
 *    sin_hue = sin(a)*256 +0x100;
 *    cos_hue = cos(a)*256;
 *  a:[0~30]
 *    sin_hue = sin(a)*256;
 *    cos_hue = cos(a)*256;
 */
static int vop_get_bcsh_hue(struct rk_lcdc_driver *dev_drv, bcsh_hue_mode mode)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u32 val = 0;

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		val = vop_readl(vop_dev, BCSH_H);
		switch (mode) {
		case H_SIN:
			val &= MASK(SIN_HUE);
			break;
		case H_COS:
			val &= MASK(COS_HUE);
			val >>= 16;
			break;
		default:
			break;
		}
	}
	spin_unlock(&vop_dev->reg_lock);

	return val;
}

static int vop_set_dsp_cabc(struct rk_lcdc_driver *dev_drv, int mode,
			    int calc, int up, int down, int global)
{
	struct vop_device *vop_dev =
			container_of(dev_drv, struct vop_device, driver);
	struct rk_screen *screen = dev_drv->cur_screen;
	u32 total_pixel, calc_pixel, stage_up, stage_down;
	u32 pixel_num, global_dn;
	u64 val = 0;
	ktime_t timestamp;
	int ret = 0;

	if (!vop_dev->cabc_lut_addr_base) {
		pr_err("vop chip[%d] not supoort cabc\n", VOP_CHIP(vop_dev));
		return 0;
	}

	if (!screen->cabc_lut) {
		pr_err("screen cabc lut not config, so not open cabc\n");
		return 0;
	}

	if (!mode) {
		if (VOP_CHIP(vop_dev) == VOP_RK3399) {
			calc = 0;
			up = 256;
			down = 255;
			global = 0;
		} else {
			spin_lock(&vop_dev->reg_lock);
			if (vop_dev->clk_on) {
				vop_msk_reg(vop_dev, CABC_CTRL0,
					    V_CABC_EN(0) | V_CABC_HANDLE_EN(0));
				vop_cfg_done(vop_dev);
			}
			pr_info("mode = 0, close cabc\n");
			spin_unlock(&vop_dev->reg_lock);
			return 0;
		}
	}

	total_pixel = screen->mode.xres * screen->mode.yres;
	pixel_num = 1000 - calc;
	calc_pixel = (total_pixel * pixel_num) / 1000;
	stage_up = up;
	stage_down = down;
	global_dn = global;
	pr_info("enable cabc:mode=%d, calc=%d, up=%d, down=%d, global=%d\n",
		mode, calc, stage_up, stage_down, global_dn);

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		val = V_PWM_CONFIG_MODE(STAGE_BY_STAGE) |
			V_CABC_CALC_PIXEL_NUM(calc_pixel);
		vop_msk_reg(vop_dev, CABC_CTRL0, val);

		val = V_CABC_LUT_EN(1) | V_CABC_TOTAL_NUM(total_pixel);
		vop_msk_reg(vop_dev, CABC_CTRL1, val);

		val = V_CABC_STAGE_DOWN(stage_down) |
			V_CABC_STAGE_UP(stage_up) |
			V_CABC_STAGE_UP_MODE(0) | V_MAX_SCALE_CFG_VALUE(1) |
			V_MAX_SCALE_CFG_ENABLE(0);
		vop_msk_reg(vop_dev, CABC_CTRL2, val);

		val = V_CABC_GLOBAL_DN(global_dn) |
			V_CABC_GLOBAL_DN_LIMIT_EN(1);
		vop_msk_reg(vop_dev, CABC_CTRL3, val);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);

	timestamp = dev_drv->vsync_info.timestamp;
	ret = wait_event_interruptible_timeout(dev_drv->vsync_info.wait,
			!ktime_equal(timestamp, dev_drv->vsync_info.timestamp),
			msecs_to_jiffies(50));
	if (ret < 0)
		return ret;
	else if (ret == 0)
		pr_err("%s wait vsync time out\n", __func__);

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		val = V_CABC_EN(1) | V_CABC_HANDLE_EN(1);
		vop_msk_reg(vop_dev, CABC_CTRL0, val);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_set_bcsh_hue(struct rk_lcdc_driver *dev_drv,
			    int sin_hue, int cos_hue)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u64 val;

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		val = V_SIN_HUE(sin_hue) | V_COS_HUE(cos_hue);
		vop_msk_reg(vop_dev, BCSH_H, val);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_set_bcsh_bcs(struct rk_lcdc_driver *dev_drv,
			    bcsh_bcs_mode mode, int value)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u64 val = 0;

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		switch (mode) {
		case BRIGHTNESS:
			/*from 0 to 255,typical is 128 */
			if (value < 0x80)
				value += 0x80;
			else if (value >= 0x80)
				value = value - 0x80;
			val = V_BRIGHTNESS(value);
			break;
		case CONTRAST:
			/*from 0 to 510,typical is 256 */
			val = V_CONTRAST(value);
			break;
		case SAT_CON:
			/*from 0 to 1015,typical is 256 */
			val = V_SAT_CON(value);
			break;
		default:
			break;
		}
		vop_msk_reg(vop_dev, BCSH_BCS, val);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);

	return val;
}

static int vop_get_bcsh_bcs(struct rk_lcdc_driver *dev_drv, bcsh_bcs_mode mode)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u64 val = 0;

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		val = vop_readl(vop_dev, BCSH_BCS);
		switch (mode) {
		case BRIGHTNESS:
			val &= MASK(BRIGHTNESS);
			if (val > 0x80)
				val -= 0x80;
			else
				val += 0x80;
			break;
		case CONTRAST:
			val &= MASK(CONTRAST);
			val >>= 8;
			break;
		case SAT_CON:
			val &= MASK(SAT_CON);
			val >>= 20;
			break;
		default:
			break;
		}
	}
	spin_unlock(&vop_dev->reg_lock);
	return val;
}

static int vop_open_bcsh(struct rk_lcdc_driver *dev_drv, bool open)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	spin_lock(&vop_dev->reg_lock);
	if (vop_dev->clk_on) {
		if (open) {
			vop_writel(vop_dev, BCSH_COLOR_BAR, 0x1);
			vop_writel(vop_dev, BCSH_BCS, 0xd0010000);
			vop_writel(vop_dev, BCSH_H, 0x01000000);
			dev_drv->bcsh.enable = 1;
		} else {
			vop_msk_reg(vop_dev, BCSH_COLOR_BAR, V_BCSH_EN(0));
			dev_drv->bcsh.enable = 0;
		}
		vop_bcsh_path_sel(dev_drv);
		vop_cfg_done(vop_dev);
	}
	spin_unlock(&vop_dev->reg_lock);

	return 0;
}

static int vop_set_bcsh(struct rk_lcdc_driver *dev_drv, bool enable)
{
	if (!enable || !dev_drv->bcsh.enable) {
		vop_open_bcsh(dev_drv, false);
		return 0;
	}

	if (dev_drv->bcsh.brightness <= 255 ||
	    dev_drv->bcsh.contrast <= 510 ||
	    dev_drv->bcsh.sat_con <= 1015 ||
	    (dev_drv->bcsh.sin_hue <= 511 && dev_drv->bcsh.cos_hue <= 511)) {
		vop_open_bcsh(dev_drv, true);
		if (dev_drv->bcsh.brightness <= 255)
			vop_set_bcsh_bcs(dev_drv, BRIGHTNESS,
					 dev_drv->bcsh.brightness);
		if (dev_drv->bcsh.contrast <= 510)
			vop_set_bcsh_bcs(dev_drv, CONTRAST,
					 dev_drv->bcsh.contrast);
		if (dev_drv->bcsh.sat_con <= 1015)
			vop_set_bcsh_bcs(dev_drv, SAT_CON,
					 dev_drv->bcsh.sat_con);
		if (dev_drv->bcsh.sin_hue <= 511 &&
		    dev_drv->bcsh.cos_hue <= 511)
			vop_set_bcsh_hue(dev_drv, dev_drv->bcsh.sin_hue,
					 dev_drv->bcsh.cos_hue);
	}

	return 0;
}

static int __maybe_unused
vop_dsp_black(struct rk_lcdc_driver *dev_drv, int enable)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (enable) {
		spin_lock(&vop_dev->reg_lock);
		if (likely(vop_dev->clk_on)) {
			vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_BLACK_EN(1));
			vop_cfg_done(vop_dev);
		}
		spin_unlock(&vop_dev->reg_lock);
	} else {
		spin_lock(&vop_dev->reg_lock);
		if (likely(vop_dev->clk_on)) {
			vop_msk_reg(vop_dev, DSP_CTRL0, V_DSP_BLACK_EN(0));

			vop_cfg_done(vop_dev);
		}
		spin_unlock(&vop_dev->reg_lock);
	}

	return 0;
}

static int vop_backlight_close(struct rk_lcdc_driver *dev_drv, int enable)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	vop_get_backlight_device(dev_drv);

	if (enable) {
		/* close the backlight */
		if (vop_dev->backlight) {
			vop_dev->backlight->props.power = FB_BLANK_POWERDOWN;
			backlight_update_status(vop_dev->backlight);
		}
		if (dev_drv->trsm_ops && dev_drv->trsm_ops->disable)
			dev_drv->trsm_ops->disable();
	} else {
		if (dev_drv->trsm_ops && dev_drv->trsm_ops->enable)
			dev_drv->trsm_ops->enable();
		msleep(100);
		/* open the backlight */
		if (vop_dev->backlight) {
			vop_dev->backlight->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(vop_dev->backlight);
		}
	}

	return 0;
}

static int vop_set_overscan(struct rk_lcdc_driver *dev_drv,
			    struct overscan *overscan)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}
	/*vop_post_cfg(dev_drv);*/

	return 0;
}

static int vop_extern_func(struct rk_lcdc_driver *dev_drv, int cmd)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);
	u64 val;

	if (unlikely(!vop_dev->clk_on)) {
		pr_info("%s,clk_on = %d\n", __func__, vop_dev->clk_on);
		return 0;
	}

	switch (cmd) {
	case UPDATE_CABC_PWM:
		vop_cfg_done(vop_dev);
		break;
	case SET_DSP_MIRROR:
		val = V_DSP_X_MIR_EN(dev_drv->cur_screen->x_mirror) |
			V_DSP_Y_MIR_EN(dev_drv->cur_screen->y_mirror);
		vop_msk_reg(vop_dev, DSP_CTRL0, val);
		pr_info("%s: xmirror: %d, ymirror: %d\n",
			__func__, dev_drv->cur_screen->x_mirror,
			dev_drv->cur_screen->y_mirror);
		break;
	default:
		break;
	}

	return 0;
}

static struct rk_lcdc_drv_ops lcdc_drv_ops = {
	.open = vop_open,
	.win_direct_en = vop_win_direct_en,
	.load_screen = vop_load_screen,
	.get_dspbuf_info = vop_get_dspbuf_info,
	.post_dspbuf = vop_post_dspbuf,
	.set_par = vop_set_par,
	.pan_display = vop_pan_display,
	.set_wb = vop_set_writeback,
	.direct_set_addr = vop_direct_set_win_addr,
	/*.lcdc_reg_update = vop_reg_update,*/
	.blank = vop_blank,
	.ioctl = vop_ioctl,
	.suspend = vop_early_suspend,
	.resume = vop_early_resume,
	.get_win_state = vop_get_win_state,
	.area_support_num = vop_get_area_num,
	.ovl_mgr = vop_ovl_mgr,
	.get_disp_info = vop_get_disp_info,
	.fps_mgr = vop_fps_mgr,
	.fb_get_win_id = vop_get_win_id,
	.fb_win_remap = vop_fb_win_remap,
	.poll_vblank = vop_poll_vblank,
	.dpi_open = vop_dpi_open,
	.dpi_win_sel = vop_dpi_win_sel,
	.dpi_status = vop_dpi_status,
	.get_dsp_addr = vop_get_dsp_addr,
	.set_dsp_lut = vop_set_lut,
	.set_cabc_lut = vop_set_cabc,
	.set_dsp_cabc = vop_set_dsp_cabc,
	.set_dsp_bcsh_hue = vop_set_bcsh_hue,
	.set_dsp_bcsh_bcs = vop_set_bcsh_bcs,
	.get_dsp_bcsh_hue = vop_get_bcsh_hue,
	.get_dsp_bcsh_bcs = vop_get_bcsh_bcs,
	.open_bcsh = vop_open_bcsh,
	.dump_reg = vop_reg_dump,
	.cfg_done = vop_config_done,
	.set_irq_to_cpu = vop_set_irq_to_cpu,
	/*.dsp_black = vop_dsp_black,*/
	.backlight_close = vop_backlight_close,
	.mmu_en    = vop_mmu_en,
	.set_overscan   = vop_set_overscan,
	.extern_func	= vop_extern_func,
};

static irqreturn_t vop_isr(int irq, void *dev_id)
{
	struct vop_device *vop_dev = (struct vop_device *)dev_id;
	ktime_t timestamp = ktime_get();
	u32 intr_status;
	unsigned long flags;

	spin_lock_irqsave(&vop_dev->irq_lock, flags);

	intr_status = vop_readl(vop_dev, INTR_STATUS0);
	vop_mask_writel(vop_dev, INTR_CLEAR0, INTR_MASK, intr_status);

	spin_unlock_irqrestore(&vop_dev->irq_lock, flags);
	/* This is expected for vop iommu irqs, since the irq is shared */
	if (!intr_status)
		return IRQ_NONE;

	if (intr_status & INTR_FS) {
		timestamp = ktime_get();
		if (vop_dev->driver.wb_data.state) {
			u32 wb_status;

			spin_lock_irqsave(&vop_dev->irq_lock, flags);
			wb_status = vop_read_bit(vop_dev, WB_CTRL0, V_WB_EN(0));

			if (wb_status)
				vop_clr_bit(vop_dev, WB_CTRL0, V_WB_EN(0));

			vop_cfg_done(vop_dev);
			vop_dev->driver.wb_data.state = 0;
			spin_unlock_irqrestore(&vop_dev->irq_lock, flags);
		}
		vop_dev->driver.vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&vop_dev->driver.vsync_info.wait);
		intr_status &= ~INTR_FS;
	}

	if (intr_status & INTR_LINE_FLAG0)
		intr_status &= ~INTR_LINE_FLAG0;

	if (intr_status & INTR_LINE_FLAG1)
		intr_status &= ~INTR_LINE_FLAG1;

	if (intr_status & INTR_FS_NEW)
		intr_status &= ~INTR_FS_NEW;

	if (intr_status & INTR_BUS_ERROR) {
		intr_status &= ~INTR_BUS_ERROR;
		dev_warn_ratelimited(vop_dev->dev, "bus error!");
	}

	if (intr_status & INTR_WIN0_EMPTY) {
		intr_status &= ~INTR_WIN0_EMPTY;
		dev_warn_ratelimited(vop_dev->dev, "intr win0 empty!");
	}

	if (intr_status & INTR_WIN1_EMPTY) {
		intr_status &= ~INTR_WIN1_EMPTY;
		dev_warn_ratelimited(vop_dev->dev, "intr win1 empty!");
	}

	if (intr_status & INTR_HWC_EMPTY) {
		intr_status &= ~INTR_HWC_EMPTY;
		dev_warn_ratelimited(vop_dev->dev, "intr hwc empty!");
	}

	if (intr_status & INTR_POST_BUF_EMPTY) {
		intr_status &= ~INTR_POST_BUF_EMPTY;
		dev_warn_ratelimited(vop_dev->dev, "intr post buf empty!");
	}

	if (intr_status)
		dev_err(vop_dev->dev, "Unknown VOP IRQs: %#02x\n", intr_status);

	return IRQ_HANDLED;
}

#if defined(CONFIG_PM)
static int vop_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vop_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define vop_suspend NULL
#define vop_resume  NULL
#endif

static int vop_parse_dt(struct vop_device *vop_dev)
{
	struct device_node *np = vop_dev->dev->of_node;
	struct rk_lcdc_driver *dev_drv = &vop_dev->driver;
	int val;

	if (of_property_read_u32(np, "rockchip,prop", &val))
		vop_dev->prop = PRMRY;	/*default set it as primary */
	else
		vop_dev->prop = val;

	if (of_property_read_u32(np, "rockchip,mirror", &val))
		dev_drv->rotate_mode = NO_MIRROR;
	else
		dev_drv->rotate_mode = val;

	if (of_property_read_u32(np, "rockchip,cabc_mode", &val))
		dev_drv->cabc_mode = 0;	/* default set close cabc */
	else
		dev_drv->cabc_mode = val;

	if (of_property_read_u32(np, "rockchip,pwr18", &val))
		/*default set it as 3.xv power supply */
		vop_dev->pwr18 = false;
	else
		vop_dev->pwr18 = (val ? true : false);

	if (of_property_read_u32(np, "rockchip,fb-win-map", &val))
		dev_drv->fb_win_map = FB_DEFAULT_ORDER;
	else
		dev_drv->fb_win_map = val;

	if (of_property_read_u32(np, "rockchip,bcsh-en", &val))
		dev_drv->bcsh.enable = false;
	else
		dev_drv->bcsh.enable = (val ? true : false);

	if (of_property_read_u32(np, "rockchip,brightness", &val))
		dev_drv->bcsh.brightness = 0xffff;
	else
		dev_drv->bcsh.brightness = val;

	if (of_property_read_u32(np, "rockchip,contrast", &val))
		dev_drv->bcsh.contrast = 0xffff;
	else
		dev_drv->bcsh.contrast = val;

	if (of_property_read_u32(np, "rockchip,sat-con", &val))
		dev_drv->bcsh.sat_con = 0xffff;
	else
		dev_drv->bcsh.sat_con = val;

	if (of_property_read_u32(np, "rockchip,hue", &val)) {
		dev_drv->bcsh.sin_hue = 0xffff;
		dev_drv->bcsh.cos_hue = 0xffff;
	} else {
		dev_drv->bcsh.sin_hue = val & 0xff;
		dev_drv->bcsh.cos_hue = (val >> 8) & 0xff;
	}

	if (of_property_read_u32(np, "rockchip,iommu-enabled", &val))
		dev_drv->iommu_enabled = 0;
	else
		dev_drv->iommu_enabled = val;

	if (of_property_read_u32(np, "rockchip,dsp_mode", &val))
		dev_drv->dsp_mode = DEFAULT_MODE;
	else
		dev_drv->dsp_mode = val;

	return 0;
}

static struct platform_device *rk322x_pdev;

int vop_register_dmc(void)
{
	struct platform_device *pdev = rk322x_pdev;
	struct vop_device *vop_dev;
	struct device *dev = &pdev->dev;
	struct devfreq *devfreq;
	struct devfreq_event_dev *event_dev;

	if (!pdev)
		return -ENODEV;

	vop_dev = platform_get_drvdata(pdev);;
	if (!vop_dev)
		return -ENODEV;

	dev = &pdev->dev;
	devfreq = devfreq_get_devfreq_by_phandle(dev, 0);
	if (IS_ERR(devfreq)) {
		dev_err(vop_dev->dev, "fail to get devfreq for dmc\n");
		return -ENODEV;
	}

	vop_dev->devfreq = devfreq;
	vop_dev->dmc_nb.notifier_call = dmc_notify;
	devfreq_register_notifier(vop_dev->devfreq, &vop_dev->dmc_nb,
				  DEVFREQ_TRANSITION_NOTIFIER);

	event_dev = devfreq_event_get_edev_by_phandle(vop_dev->devfreq->dev.parent,
						      0);
	if (IS_ERR(event_dev)) {
		dev_err(vop_dev->dev, "fail to get edev for dmc\n");
		return -ENODEV;
	}

	vop_dev->devfreq_event_dev = event_dev;
	return 0;
}

static int vop_wms_refresh(struct rk_lcdc_driver *dev_drv)
{
	struct vop_device *vop_dev =
	    container_of(dev_drv, struct vop_device, driver);

	if (unlikely(!vop_dev->clk_on)) {
		dev_info_ratelimited(vop_dev->dev, "%s,clk_on = %d\n",
				     __func__, vop_dev->clk_on);
		return 0;
	}
	vop_msk_reg_nobak(vop_dev, SYS_CTRL, V_EDPI_WMS_FS(1));
	vop_msk_reg(vop_dev, SYS_CTRL, V_EDPI_WMS_MODE(0));
	vop_msk_reg(vop_dev, SYS_CTRL, V_EDPI_WMS_MODE(1));

	if (dev_drv->trsm_ops && dev_drv->trsm_ops->refresh)
		dev_drv->trsm_ops->refresh(0, 0, dev_drv->cur_screen->mode.xres,
					   dev_drv->cur_screen->mode.yres);

	return 0;
}

static irqreturn_t te_irq_handle(int irq, void *dev_id)
{
	struct rk_lcdc_driver *dev_drv  = (struct rk_lcdc_driver *)dev_id;

	vop_wms_refresh(dev_drv);

	return IRQ_HANDLED;
}

static int vop_probe(struct platform_device *pdev)
{
	struct vop_device *vop_dev = NULL;
	struct rk_lcdc_driver *dev_drv;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int prop;
	int ret = 0;
	int te_pin;

	/* if the primary lcdc has not registered ,the extend
	 * lcdc register later
	 */
	of_property_read_u32(np, "rockchip,prop", &prop);
	if (prop == EXTEND) {
		if (!is_prmry_rk_lcdc_registered())
			return -EPROBE_DEFER;
	}
	vop_dev = devm_kzalloc(dev, sizeof(struct vop_device), GFP_KERNEL);
	if (!vop_dev)
		return -ENOMEM;
	of_id = of_match_device(vop_dt_ids, dev);
	vop_dev->data = of_id->data;
	if (VOP_CHIP(vop_dev) != VOP_RK322X && VOP_CHIP(vop_dev) != VOP_RK3399)
		return -ENODEV;
	platform_set_drvdata(pdev, vop_dev);
	vop_dev->dev = dev;
	vop_parse_dt(vop_dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	/* enable power domain */
	pm_runtime_enable(dev);
#endif
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vop_dev->reg_phy_base = res->start;
	vop_dev->len = resource_size(res);
	vop_dev->regs = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
	if (IS_ERR(vop_dev->regs))
		return PTR_ERR(vop_dev->regs);

	dev_info(dev, "vop_dev->regs=0x%lx\n", (long)vop_dev->regs);

	vop_dev->regsbak = devm_kzalloc(dev, vop_dev->len, GFP_KERNEL);
	if (IS_ERR(vop_dev->regsbak))
		return PTR_ERR(vop_dev->regsbak);
	if (VOP_CHIP(vop_dev) == VOP_RK3399) {
		vop_dev->dsp_lut_addr_base = vop_dev->regs + GAMMA_LUT_ADDR;
		vop_dev->cabc_lut_addr_base = vop_dev->regs +
						CABC_GAMMA_LUT_ADDR;
	}
	vop_dev->grf_base =
		syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(vop_dev->grf_base)) {
		dev_err(&pdev->dev, "can't find lcdc grf property\n");
		vop_dev->grf_base = NULL;
	}

	vop_dev->id = vop_get_id(vop_dev, vop_dev->reg_phy_base);
	dev_set_name(vop_dev->dev, "vop%d", vop_dev->id);
	dev_drv = &vop_dev->driver;
	dev_drv->dev = dev;
	dev_drv->prop = prop;
	dev_drv->id = vop_dev->id;
	dev_drv->ops = &lcdc_drv_ops;
	dev_drv->lcdc_win_num = vop_dev->data->n_wins;
	dev_drv->reserved_fb = 0;
	spin_lock_init(&vop_dev->reg_lock);
	spin_lock_init(&vop_dev->irq_lock);
	vop_dev->irq = platform_get_irq(pdev, 0);
	if (vop_dev->irq < 0) {
		dev_err(&pdev->dev, "cannot find IRQ for lcdc%d\n",
			vop_dev->id);
		return -ENXIO;
	}

	ret = devm_request_irq(dev, vop_dev->irq, vop_isr,
			       IRQF_SHARED, dev_name(dev), vop_dev);
	if (ret) {
		dev_err(&pdev->dev, "cannot requeset irq %d - err %d\n",
			vop_dev->irq, ret);
		return ret;
	}
	if (dev_drv->iommu_enabled) {
		if (VOP_CHIP(vop_dev) == VOP_RK322X) {
			strcpy(dev_drv->mmu_dts_name,
			       VOP_IOMMU_COMPATIBLE_NAME);
		} else {
			if (vop_dev->id == 0)
				strcpy(dev_drv->mmu_dts_name,
				       VOPB_IOMMU_COMPATIBLE_NAME);
			else
				strcpy(dev_drv->mmu_dts_name,
				       VOPL_IOMMU_COMPATIBLE_NAME);
		}
	}
	if (VOP_CHIP(vop_dev) == VOP_RK3399)
		dev_drv->property.feature |= SUPPORT_WRITE_BACK | SUPPORT_AFBDC;
	dev_drv->property.feature |= SUPPORT_VOP_IDENTIFY |
					SUPPORT_YUV420_OUTPUT;
	dev_drv->property.max_output_x = 4096;
	dev_drv->property.max_output_y = 2160;

	init_waitqueue_head(&vop_dev->wait_vop_switch_queue);
	vop_dev->vop_switch_status = 0;
	init_waitqueue_head(&vop_dev->wait_dmc_queue);
	vop_dev->dmc_in_process = 0;

	ret = rk_fb_register(dev_drv, vop_dev->data->win, vop_dev->id);
	if (ret < 0) {
		dev_err(dev, "register fb for lcdc%d failed!\n", vop_dev->id);
		return ret;
	}

	if ((VOP_CHIP(vop_dev) == VOP_RK3399) && (vop_dev->id == 1)) {
		dev_drv->win[1]->property.feature &= ~SUPPORT_HW_EXIST;
		dev_drv->win[3]->property.feature &= ~SUPPORT_HW_EXIST;
	}

	vop_dev->screen = dev_drv->screen0;
	dev_info(dev, "lcdc%d probe ok, iommu %s\n",
		 vop_dev->id, dev_drv->iommu_enabled ? "enabled" : "disabled");

	rk322x_pdev = pdev;

	if (dev_drv->cur_screen->refresh_mode == SCREEN_CMD_MODE) {
		te_pin = of_get_named_gpio_flags(np, "te-gpio", 0, NULL);
		if (IS_ERR_VALUE(te_pin)) {
			dev_err(dev, "define te pin for cmd mode!\n");
			return 0;
		}
		ret = devm_gpio_request(dev, te_pin, "vop-te-gpio");
		if (ret) {
			dev_err(dev, "request gpio %d failed\n", te_pin);
			return 0;
		}
		gpio_direction_input(te_pin);
		dev_drv->te_irq = gpio_to_irq(te_pin);
		ret = devm_request_threaded_irq(dev,
						dev_drv->te_irq,
						NULL, te_irq_handle,
						IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT,
						"te_irq", dev_drv);
		if (ret < 0)
			dev_err(dev, "request te irq failed, ret: %d\n", ret);
	}
	return 0;
}

static int vop_remove(struct platform_device *pdev)
{
	return 0;
}

static void vop_shutdown(struct platform_device *pdev)
{
	struct vop_device *vop_dev = platform_get_drvdata(pdev);
	struct rk_lcdc_driver *dev_drv = &vop_dev->driver;

	dev_drv->suspend_flag = 1;
	dev_drv->shutdown_flag = 1;
	/* ensure suspend_flag take effect on multi process */
	smp_wmb();
	flush_kthread_worker(&dev_drv->update_regs_worker);
	kthread_stop(dev_drv->update_regs_thread);
	vop_deint(vop_dev);

	vop_clk_disable(vop_dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	pm_runtime_disable(vop_dev->dev);
#endif
	rk_disp_pwr_disable(dev_drv);
}

static struct platform_driver vop_driver = {
	.probe = vop_probe,
	.remove = vop_remove,
	.driver = {
		   .name = "rk322x-lcdc",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(vop_dt_ids),
		   },
	.suspend = vop_suspend,
	.resume = vop_resume,
	.shutdown = vop_shutdown,
};

static int __init vop_module_init(void)
{
	return platform_driver_register(&vop_driver);
}

static void __exit vop_module_exit(void)
{
	platform_driver_unregister(&vop_driver);
}

fs_initcall(vop_module_init);
module_exit(vop_module_exit);
