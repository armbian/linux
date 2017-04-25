/*
 * Firmware APIs for XRadio drivers
 *
 * Copyright (c) 2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FWIO_H_INCLUDED
#define FWIO_H_INCLUDED

#define XR819_HW_REV0       (8190)
#ifdef USE_VFS_FIRMWARE
#define XR819_BOOTLOADER    ("/system/etc/firmware/boot_xr819.bin")
#define XR819_FIRMWARE      ("/system/etc/firmware/fw_xr819.bin")
#define XR819_SDD_FILE      ("/system/etc/firmware/sdd_xr819.bin")
#else
#define XR819_BOOTLOADER    ("boot_xr819.bin")
#define XR819_FIRMWARE      ("fw_xr819.bin")
#define XR819_SDD_FILE      ("sdd_xr819.bin")
#endif

#define SDD_PTA_CFG_ELT_ID             0xEB
#define SDD_REFERENCE_FREQUENCY_ELT_ID 0xC5
#define FIELD_OFFSET(type, field) ((u8 *)&((type *)0)->field - (u8 *)0)
#define FIND_NEXT_ELT(e) (struct xradio_sdd *)((u8 *)&e->data + e->length)
struct xradio_sdd {
	u8 id;
	u8 length;
	u8 data[];
};

struct xradio_common;
int xradio_load_firmware(struct xradio_common *hw_priv);
int xradio_dev_deinit(struct xradio_common *hw_priv);

#endif
