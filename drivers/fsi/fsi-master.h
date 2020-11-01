/*
 * FSI master definitions. These comprise the core <--> master interface,
 * to allow the core to interact with the (hardware-specific) masters.
 *
 * Copyright (C) IBM Corporation 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DRIVERS_FSI_MASTER_H
#define DRIVERS_FSI_MASTER_H

#include <linux/device.h>

#define FSI_MASTER_FLAG_SWCLOCK		0x1

struct fsi_master {
	struct device	dev;
	int		idx;
	int		n_links;
	int		flags;
	int		(*read)(struct fsi_master *, int link, uint8_t id,
				uint32_t addr, void *val, size_t size);
	int		(*write)(struct fsi_master *, int link, uint8_t id,
				uint32_t addr, const void *val, size_t size);
	int		(*term)(struct fsi_master *, int link, uint8_t id);
	int		(*send_break)(struct fsi_master *, int link);
	int		(*link_enable)(struct fsi_master *, int link);
};

#define dev_to_fsi_master(d) container_of(d, struct fsi_master, dev)

extern int fsi_master_register(struct fsi_master *master);
extern void fsi_master_unregister(struct fsi_master *master);

#endif /* DRIVERS_FSI_MASTER_H */
