/*
 * Platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ioport.h>

#include <linux/regulator/consumer.h>

#include "xradio.h"
#include "platform.h"
#include "sbus.h"
#include <linux/sys_config.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/power/scenelock.h>
#include <linux/power/aw_pm.h>

/*if 1, Use system rf-kill for wifi power manage, default 0. */
#define PLATFORM_RFKILL_PM     1
/*hardware platform config.*/
static int wlan_bus_id;

#ifdef PLATFORM_RFKILL_PM
extern int sunxi_wlan_get_bus_index(void);
extern void sunxi_wlan_set_power(int on_off);
extern int sunxi_wlan_get_irq(void);
#endif

int xradio_plat_init(void)
{
	int ret = 0;
	wlan_bus_id = sunxi_wlan_get_bus_index();
	return ret;
}

void xradio_plat_deinit(void)
{
	return;
}

int xradio_wlan_power(int on)
{
	sunxi_wlan_set_power(on);
	return 0;
}

int xradio_sdio_detect(int enable)
{
	MCI_RESCAN_CARD(wlan_bus_id);
	xradio_dbg(XRADIO_DBG_ALWY, "%s SDIO card %d\n",
		   enable ? "Detect" : "Remove", wlan_bus_id);
	mdelay(10);
	return 0;
}

static u32 irq_handle;
#ifdef PLAT_ALLWINNER_SUNXI
static irqreturn_t xradio_irq_handler(int irq, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return IRQ_HANDLED;
}
#else /*PLAT_ALLWINNER_SUN6I */
static u32 xradio_irq_handler(void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	unsigned long flags;

	SYS_BUG(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
	return 0;
}
#endif

int xradio_request_irq(struct device *dev, void *sbus_priv)
{
	int ret = -1;
	if (!irq_handle) {
		irq_handle = sunxi_wlan_get_irq();
		ret = devm_request_irq(dev, irq_handle,
				       (irq_handler_t) xradio_irq_handler,
				       IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "xradio_irq",
				       sbus_priv);
		if (IS_ERR_VALUE(ret)) {
			irq_handle = 0;
		}
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: error, irq exist already!\n",
			   __func__);
	}
	if (irq_handle) {
		xradio_dbg(XRADIO_DBG_NIY,
			   "%s: request_irq sucess! irq=0x%08x\n", __func__,
			   irq_handle);
		ret = enable_wakeup_src(CPUS_WLAN_SRC, 0);
		if (ret < 0)
			xradio_dbg(XRADIO_DBG_ERROR, \
				"%s: enable_wakeup_src failed\n", __func__);
		else
			xradio_dbg(XRADIO_DBG_NIY, \
				"%s: enable_wakeup_src success\n", __func__);
	} else {
		xradio_dbg(XRADIO_DBG_ERROR, "%s: request_irq err: %d\n",
			   __func__, ret);
		ret = -1;
	}
	return ret;
}

void xradio_free_irq(struct device *dev, void *sbus_priv)
{
	struct sbus_priv *self = (struct sbus_priv *)sbus_priv;
	if (irq_handle) {
		devm_free_irq(dev, irq_handle, self);
		irq_handle = 0;
		disable_wakeup_src(CPUS_WLAN_SRC, 0);
	}
}
