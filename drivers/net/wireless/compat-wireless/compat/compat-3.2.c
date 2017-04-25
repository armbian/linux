/*
 * Copyright 2012  Luis R. Rodriguez <mcgrof@frijolero.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 3.2.
 */

#include <linux/kernel.h>
#include <linux/device.h>

int __netdev_printk(const char *level, const struct net_device *dev,
			   struct va_format *vaf)
{
	int r;

	if (dev && dev->dev.parent)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
		r = dev_printk(level, dev->dev.parent, "%s: %pV",
			       netdev_name(dev), vaf);
#else
		/* XXX: this could likely be done better but I'm lazy */
		r = printk("%s%s: %pV", level, netdev_name(dev), vaf);
#endif
	else if (dev)
		r = printk("%s%s: %pV", level, netdev_name(dev), vaf);
	else
		r = printk("%s(NULL net_device): %pV", level, vaf);

	return r;
}
EXPORT_SYMBOL(__netdev_printk);
