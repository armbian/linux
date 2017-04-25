/*
 * platform interfaces for XRadio drivers
 *
 * Copyright (c) 2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef XRADIO_PLAT_H_INCLUDED
#define XRADIO_PLAT_H_INCLUDED

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mmc/host.h>

/* Select hardware platform.*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
#define PLAT_ALLWINNER_SUNXI
#define MCI_RESCAN_CARD(id)  sunxi_mmc_rescan_card(id)
#define MCI_CHECK_READY(h, t)     sunxi_mmc_check_r1_ready(h, t)

extern void mmc_manual_remove_card(struct mmc_host *host , struct mmc_card *card);
/*extern void sunxi_mci_rescan_card(unsigned id, unsigned insert);*/
extern void sunxi_mmc_rescan_card(unsigned id);
/*extern int sunxi_mci_check_r1_ready(struct mmc_host* mmc, unsigned ms);*/
extern int sunxi_mmc_check_r1_ready(struct mmc_host *mmc, unsigned ms);

#else
#define PLAT_ALLWINNER_SUN6I
#define MCI_RESCAN_CARD(id, ins)  sw_mci_rescan_card(id, ins)
#define MCI_CHECK_READY(h, t)     sw_mci_check_r1_ready(h, t)

extern void sw_mci_rescan_card(unsigned id, unsigned insert);
extern int sw_mci_check_r1_ready(struct mmc_host *mmc, unsigned ms);
#endif

/* platform interfaces */
int xradio_plat_init(void);
void xradio_plat_deinit(void);
int xradio_sdio_detect(int enable);
int xradio_request_irq(struct device *dev, void *sbus_priv);
void xradio_free_irq(struct device *dev, void *sbus_priv);
int xradio_wlan_power(int on);

#endif /* XRADIO_PLAT_H_INCLUDED */
