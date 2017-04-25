/*
 * Copyright 2007	Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.27
 */

#include <linux/compat.h>
#include <linux/pci.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#endif

/* rfkill notification chain */
#define RFKILL_STATE_CHANGED            0x0001  /* state of a normal rfkill
							switch has changed */

/*
 * e5899e1b7d73e67de758a32174a859cc2586c0b9 made pci_pme_capable() external,
 * it was defined internally, some drivers want access to this information.
 *
 * Unfortunately the old kernels do not have ->pm_cap or ->pme_support so
 * we have to call the PCI routines directly.
 */

/**
 * pci_pme_capable - check the capability of PCI device to generate PME#
 * @dev: PCI device to handle.
 * @state: PCI state from which device will issue PME#.
 *
 * This is the backport code for older kernels for compat-wireless, we read stuff
 * from the initialization stuff from pci_pm_init().
 */
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state)
{
	int pm;
	u16 pmc = 0;
	u16 pme_support; /* as from the pci dev */
	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (!pm)
		return false;

        if ((pmc & PCI_PM_CAP_VER_MASK) > 3) {
		dev_err(&dev->dev, "unsupported PM cap regs version (%u)\n",
			pmc & PCI_PM_CAP_VER_MASK);
		return false;
        }

	pmc &= PCI_PM_CAP_PME_MASK;

	if (!pmc)
		return false;

	pme_support = pmc >> PCI_PM_CAP_PME_SHIFT;

	/* Check device's ability to generate PME# */

	return !!(pme_support & (1 << state));
}
EXPORT_SYMBOL(pci_pme_capable);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
/**
 *	mmc_align_data_size - pads a transfer size to a more optimal value
 *	@card: the MMC card associated with the data transfer
 *	@sz: original transfer size
 *
 *	Pads the original data size with a number of extra bytes in
 *	order to avoid controller bugs and/or performance hits
 *	(e.g. some controllers revert to PIO for certain sizes).
 *
 *	Returns the improved size, which might be unmodified.
 *
 *	Note that this function is only relevant when issuing a
 *	single scatter gather entry.
 */
unsigned int mmc_align_data_size(struct mmc_card *card, unsigned int sz)
{
	/*
	* FIXME: We don't have a system for the controller to tell
	* the core about its problems yet, so for now we just 32-bit
	* align the size.
	*/
	sz = ((sz + 3) / 4) * 4;

	return sz;
}
EXPORT_SYMBOL(mmc_align_data_size);

/*
 * Calculate the maximum byte mode transfer size
 */
static inline unsigned int sdio_max_byte_size(struct sdio_func *func)
{
	unsigned int mval = (unsigned int) min(func->card->host->max_seg_size,
			    func->card->host->max_blk_size);
	mval = min(mval, func->max_blksize);
	return min(mval, 512u); /* maximum size for byte mode */
}

/**
 *	sdio_align_size - pads a transfer size to a more optimal value
 *	@func: SDIO function
 *	@sz: original transfer size
 *
 *	Pads the original data size with a number of extra bytes in
 *	order to avoid controller bugs and/or performance hits
 *	(e.g. some controllers revert to PIO for certain sizes).
 *
 *	If possible, it will also adjust the size so that it can be
 *	handled in just a single request.
 *
 *	Returns the improved size, which might be unmodified.
 */
unsigned int sdio_align_size(struct sdio_func *func, unsigned int sz)
{
	unsigned int orig_sz;
	unsigned int blk_sz, byte_sz;
	unsigned chunk_sz;

	orig_sz = sz;

	/*
	 * Do a first check with the controller, in case it
	 * wants to increase the size up to a point where it
	 * might need more than one block.
	 */
	sz = mmc_align_data_size(func->card, sz);

	/*
	 * If we can still do this with just a byte transfer, then
	 * we're done.
	 */
	if (sz <= sdio_max_byte_size(func))
		return sz;

	if (func->card->cccr.multi_block) {
		/*
		 * Check if the transfer is already block aligned
		 */
		if ((sz % func->cur_blksize) == 0)
			return sz;

		/*
		 * Realign it so that it can be done with one request,
		 * and recheck if the controller still likes it.
		 */
		blk_sz = ((sz + func->cur_blksize - 1) /
			func->cur_blksize) * func->cur_blksize;
		blk_sz = mmc_align_data_size(func->card, blk_sz);

		/*
		 * This value is only good if it is still just
		 * one request.
		 */
		if ((blk_sz % func->cur_blksize) == 0)
			return blk_sz;

		/*
		 * We failed to do one request, but at least try to
		 * pad the remainder properly.
		 */
		byte_sz = mmc_align_data_size(func->card,
				sz % func->cur_blksize);
		if (byte_sz <= sdio_max_byte_size(func)) {
			blk_sz = sz / func->cur_blksize;
			return blk_sz * func->cur_blksize + byte_sz;
		}
	} else {
		/*
		 * We need multiple requests, so first check that the
		 * controller can handle the chunk size;
		 */
		chunk_sz = mmc_align_data_size(func->card,
				sdio_max_byte_size(func));
		if (chunk_sz == sdio_max_byte_size(func)) {
			/*
			 * Fix up the size of the remainder (if any)
			 */
			byte_sz = orig_sz % chunk_sz;
			if (byte_sz) {
				byte_sz = mmc_align_data_size(func->card,
						byte_sz);
			}

			return (orig_sz / chunk_sz) * chunk_sz + byte_sz;
		}
	}

	/*
	 * The controller is simply incapable of transferring the size
	 * we want in decent manner, so just return the original size.
	 */
	return orig_sz;
}
EXPORT_SYMBOL_GPL(sdio_align_size);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */

#ifdef CONFIG_DEBUG_FS
/*
 * Backport of debugfs_remove_recursive() without using the internals globals
 * which are used by the kernel's version with:
 * simple_release_fs(&debugfs_mount, &debugfs_mount_count);
 */
void debugfs_remove_recursive(struct dentry *dentry)
{
	struct dentry *last = NULL;

	/* Sanity checks */
	if (!dentry || !dentry->d_parent || !dentry->d_parent->d_inode)
		return;

	while (dentry != last) {
		struct dentry *child = dentry;

		/* Find a child without children */
		while (!list_empty(&child->d_subdirs))
			child = list_entry(child->d_subdirs.next,
					   struct dentry,
					   d_u.d_child);

		/* Bail out if we already tried to remove that entry */
		if (child == last)
			return;

		last = child;
		debugfs_remove(child);
	}
}
EXPORT_SYMBOL_GPL(debugfs_remove_recursive);
#endif /* CONFIG_DEBUG_FS */

