/*
 * Entry code of XRadio drivers
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
#include <linux/init.h>
#include <linux/delay.h>

MODULE_AUTHOR("XRadioTech");
MODULE_DESCRIPTION("XRadioTech WLAN driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xradio_wlan");

/* external interfaces */
extern int  xradio_core_init(void);
extern void xradio_core_deinit(void);

/* Init Module function -> Called by insmod */
static int __init xradio_init(void)
{
	int ret = 0;
	printk(KERN_ERR "======== XRADIO WIFI OPEN ========\n");
	ret = xradio_core_init();  /* driver init */
	return ret;
}

/* Called at Driver Unloading */
static void __exit xradio_exit(void)
{
	xradio_core_deinit();
	printk(KERN_ERR "======== XRADIO WIFI CLOSE ========\n");
}

module_init(xradio_init);
module_exit(xradio_exit);
