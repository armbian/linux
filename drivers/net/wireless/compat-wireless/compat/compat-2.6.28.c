/*
 * Copyright 2007	Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.28.
 */

#include <linux/compat.h>
#include <linux/usb.h>
#include <linux/tty.h>
#include <asm/poll.h>

/* 2.6.28 compat code goes here */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))
#if defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)
/*
 * Compat-wireless notes for USB backport stuff:
 *
 * urb->reject exists on 2.6.27, the poison/unpoison helpers
 * did not though. The anchor poison does not exist so we cannot use them.
 *
 * USB anchor poising seems to exist to prevent future driver sumbissions
 * of usb_anchor_urb() to an anchor marked as poisoned. For older kernels
 * we cannot use that, so new usb_anchor_urb()s will be anchored. The down
 * side to this should be submission of URBs will continue being anchored
 * on an anchor instead of having them being rejected immediately when the
 * driver realized we needed to stop. For ar9170 we poison URBs upon the
 * ar9170 mac80211 stop callback(), don't think this should be so bad.
 * It mean there is period of time in older kernels for which we continue
 * to anchor new URBs to a known stopped anchor. We have two anchors
 * (TX, and RX)
 */

#if 0
/**
 * usb_poison_urb - reliably kill a transfer and prevent further use of an URB
 * @urb: pointer to URB describing a previously submitted request,
 *	may be NULL
 *
 * This routine cancels an in-progress request.  It is guaranteed that
 * upon return all completion handlers will have finished and the URB
 * will be totally idle and cannot be reused.  These features make
 * this an ideal way to stop I/O in a disconnect() callback.
 * If the request has not already finished or been unlinked
 * the completion handler will see urb->status == -ENOENT.
 *
 * After and while the routine runs, attempts to resubmit the URB will fail
 * with error -EPERM.  Thus even if the URB's completion handler always
 * tries to resubmit, it will not succeed and the URB will become idle.
 *
 * This routine may not be used in an interrupt context (such as a bottom
 * half or a completion handler), or when holding a spinlock, or in other
 * situations where the caller can't schedule().
 *
 * This routine should not be called by a driver after its disconnect
 * method has returned.
 */
void usb_poison_urb(struct urb *urb)
{
	might_sleep();
	if (!(urb && urb->dev && urb->ep))
		return;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	spin_lock_irq(&usb_reject_lock);
#endif
	++urb->reject;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	spin_unlock_irq(&usb_reject_lock);
#endif
	/*
	 * XXX: usb_hcd_unlink_urb() needs backporting... this is defined
	 * on usb hcd.c but urb.c gets access to it. That is, older kernels
	 * have usb_hcd_unlink_urb() but its not exported, nor can we
	 * re-implement it exactly. This essentially dequeues the urb from
	 * hw, we need to figure out a way to backport this.
	 */
	//usb_hcd_unlink_urb(urb, -ENOENT);

	wait_event(usb_kill_urb_queue, atomic_read(&urb->use_count) == 0);
}
EXPORT_SYMBOL_GPL(usb_poison_urb);
#endif
#endif /* CONFIG_USB */

#if defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)

#include <pcmcia/ds.h>
struct pcmcia_cfg_mem {
	tuple_t tuple;
	cisparse_t parse;
	u8 buf[256];
	cistpl_cftable_entry_t dflt;
};
/**
 * pcmcia_loop_config() - loop over configuration options
 * @p_dev:	the struct pcmcia_device which we need to loop for.
 * @conf_check:	function to call for each configuration option.
 *		It gets passed the struct pcmcia_device, the CIS data
 *		describing the configuration option, and private data
 *		being passed to pcmcia_loop_config()
 * @priv_data:	private data to be passed to the conf_check function.
 *
 * pcmcia_loop_config() loops over all configuration options, and calls
 * the driver-specific conf_check() for each one, checking whether
 * it is a valid one. Returns 0 on success or errorcode otherwise.
 */
int pcmcia_loop_config(struct pcmcia_device *p_dev,
		       int	(*conf_check)	(struct pcmcia_device *p_dev,
						 cistpl_cftable_entry_t *cfg,
						 cistpl_cftable_entry_t *dflt,
						 unsigned int vcc,
						 void *priv_data),
		       void *priv_data)
{
	struct pcmcia_cfg_mem *cfg_mem;

	tuple_t *tuple;
	int ret;
	unsigned int vcc;

	cfg_mem = kzalloc(sizeof(struct pcmcia_cfg_mem), GFP_KERNEL);
	if (cfg_mem == NULL)
		return -ENOMEM;

	/* get the current Vcc setting */
	vcc = p_dev->socket->socket.Vcc;

	tuple = &cfg_mem->tuple;
	tuple->TupleData = cfg_mem->buf;
	tuple->TupleDataMax = 255;
	tuple->TupleOffset = 0;
	tuple->DesiredTuple = CISTPL_CFTABLE_ENTRY;
	tuple->Attributes = 0;

	ret = pcmcia_get_first_tuple(p_dev, tuple);
	while (!ret) {
		cistpl_cftable_entry_t *cfg = &cfg_mem->parse.cftable_entry;

		if (pcmcia_get_tuple_data(p_dev, tuple))
			goto next_entry;

		if (pcmcia_parse_tuple(tuple, &cfg_mem->parse))
			goto next_entry;

		/* default values */
		p_dev->conf.ConfigIndex = cfg->index;
		if (cfg->flags & CISTPL_CFTABLE_DEFAULT)
			cfg_mem->dflt = *cfg;

		ret = conf_check(p_dev, cfg, &cfg_mem->dflt, vcc, priv_data);
		if (!ret)
			break;

next_entry:
		ret = pcmcia_get_next_tuple(p_dev, tuple);
	}

	return ret;
}
EXPORT_SYMBOL(pcmcia_loop_config);

#endif /* CONFIG_PCMCIA */

#if defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)

void usb_unpoison_urb(struct urb *urb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	unsigned long flags;
#endif

	if (!urb)
		return;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	spin_lock_irqsave(&usb_reject_lock, flags);
#endif
	--urb->reject;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28))
	spin_unlock_irqrestore(&usb_reject_lock, flags);
#endif
}
EXPORT_SYMBOL_GPL(usb_unpoison_urb);


#if 0
/**
 * usb_poison_anchored_urbs - cease all traffic from an anchor
 * @anchor: anchor the requests are bound to
 *
 * this allows all outstanding URBs to be poisoned starting
 * from the back of the queue. Newly added URBs will also be
 * poisoned
 *
 * This routine should not be called by a driver after its disconnect
 * method has returned.
 */
void usb_poison_anchored_urbs(struct usb_anchor *anchor)
{
	struct urb *victim;

	spin_lock_irq(&anchor->lock);
	// anchor->poisoned = 1; /* XXX: Cannot backport */
	while (!list_empty(&anchor->urb_list)) {
		victim = list_entry(anchor->urb_list.prev, struct urb,
				    anchor_list);
		/* we must make sure the URB isn't freed before we kill it*/
		usb_get_urb(victim);
		spin_unlock_irq(&anchor->lock);
		/* this will unanchor the URB */
		usb_poison_urb(victim);
		usb_put_urb(victim);
		spin_lock_irq(&anchor->lock);
	}
	spin_unlock_irq(&anchor->lock);
}
EXPORT_SYMBOL_GPL(usb_poison_anchored_urbs);
#endif

/**
 * usb_anchor_empty - is an anchor empty
 * @anchor: the anchor you want to query
 *
 * returns 1 if the anchor has no urbs associated with it
 */
int usb_anchor_empty(struct usb_anchor *anchor)
{
	return list_empty(&anchor->urb_list);
}

EXPORT_SYMBOL_GPL(usb_anchor_empty);
#endif /* CONFIG_USB */
#endif

void __iomem *pci_ioremap_bar(struct pci_dev *pdev, int bar)
{
	/*
	 * Make sure the BAR is actually a memory resource, not an IO resource
	 */
	if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM)) {
		WARN_ON(1);
		return NULL;
	}
	return ioremap_nocache(pci_resource_start(pdev, bar),
				     pci_resource_len(pdev, bar));
}
EXPORT_SYMBOL_GPL(pci_ioremap_bar);

static unsigned long round_jiffies_common(unsigned long j, int cpu,
		bool force_up)
{
	int rem;
	unsigned long original = j;

	/*
	 * We don't want all cpus firing their timers at once hitting the
	 * same lock or cachelines, so we skew each extra cpu with an extra
	 * 3 jiffies. This 3 jiffies came originally from the mm/ code which
	 * already did this.
	 * The skew is done by adding 3*cpunr, then round, then subtract this
	 * extra offset again.
	 */
	j += cpu * 3;

	rem = j % HZ;

	/*
	 * If the target jiffie is just after a whole second (which can happen
	 * due to delays of the timer irq, long irq off times etc etc) then
	 * we should round down to the whole second, not up. Use 1/4th second
	 * as cutoff for this rounding as an extreme upper bound for this.
	 * But never round down if @force_up is set.
	 */
	if (rem < HZ/4 && !force_up) /* round down */
		j = j - rem;
	else /* round up */
		j = j - rem + HZ;

	/* now that we have rounded, subtract the extra skew again */
	j -= cpu * 3;

	if (j <= jiffies) /* rounding ate our timeout entirely; */
		return original;
	return j;
}

/**
 * round_jiffies_up - function to round jiffies up to a full second
 * @j: the time in (absolute) jiffies that should be rounded
 *
 * This is the same as round_jiffies() except that it will never
 * round down.  This is useful for timeouts for which the exact time
 * of firing does not matter too much, as long as they don't fire too
 * early.
 */
unsigned long round_jiffies_up(unsigned long j)
{
	return round_jiffies_common(j, raw_smp_processor_id(), true);
}
EXPORT_SYMBOL_GPL(round_jiffies_up);

void skb_add_rx_frag(struct sk_buff *skb, int i, struct page *page, int off,
		int size)
{
	skb_fill_page_desc(skb, i, page, off, size);
	skb->len += size;
	skb->data_len += size;
	skb->truesize += size;
}
EXPORT_SYMBOL(skb_add_rx_frag);

void tty_write_unlock(struct tty_struct *tty)
{
	mutex_unlock(&tty->atomic_write_lock);
	wake_up_interruptible_poll(&tty->write_wait, POLLOUT);
}

int tty_write_lock(struct tty_struct *tty, int ndelay)
{
	if (!mutex_trylock(&tty->atomic_write_lock)) {
		if (ndelay)
			return -EAGAIN;
		if (mutex_lock_interruptible(&tty->atomic_write_lock))
			return -ERESTARTSYS;
	}
	return 0;
}

/**
 *	send_prio_char		-	send priority character
 *
 *	Send a high priority character to the tty even if stopped
 *
 *	Locking: none for xchar method, write ordering for write method.
 */

static int send_prio_char(struct tty_struct *tty, char ch)
{
	int	was_stopped = tty->stopped;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	if (tty->ops->send_xchar) {
		tty->ops->send_xchar(tty, ch);
#else
	if (tty->driver->send_xchar) {
		tty->driver->send_xchar(tty, ch);
#endif
		return 0;
	}

	if (tty_write_lock(tty, 0) < 0)
		return -ERESTARTSYS;

	if (was_stopped)
		start_tty(tty);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	tty->ops->write(tty, &ch, 1);
#else
	tty->driver->write(tty, &ch, 1);
#endif
	if (was_stopped)
		stop_tty(tty);
	tty_write_unlock(tty);
	return 0;
}

int n_tty_ioctl_helper(struct tty_struct *tty, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	unsigned long flags;
#endif
	int retval;

	switch (cmd) {
	case TCXONC:
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		switch (arg) {
		case TCOOFF:
			if (!tty->flow_stopped) {
				tty->flow_stopped = 1;
				stop_tty(tty);
			}
			break;
		case TCOON:
			if (tty->flow_stopped) {
				tty->flow_stopped = 0;
				start_tty(tty);
			}
			break;
		case TCIOFF:
			if (STOP_CHAR(tty) != __DISABLED_CHAR)
				return send_prio_char(tty, STOP_CHAR(tty));
			break;
		case TCION:
			if (START_CHAR(tty) != __DISABLED_CHAR)
				return send_prio_char(tty, START_CHAR(tty));
			break;
		default:
			return -EINVAL;
		}
		return 0;
	case TCFLSH:
		return tty_perform_flush(tty, arg);
	case TIOCPKT:
	{
		int pktmode;

		if (tty->driver->type != TTY_DRIVER_TYPE_PTY ||
		    tty->driver->subtype != PTY_TYPE_MASTER)
			return -ENOTTY;
		if (get_user(pktmode, (int __user *) arg))
			return -EFAULT;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
		spin_lock_irqsave(&tty->ctrl_lock, flags);
#endif
		if (pktmode) {
			if (!tty->packet) {
				tty->packet = 1;
				tty->link->ctrl_status = 0;
			}
		} else
			tty->packet = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
		spin_unlock_irqrestore(&tty->ctrl_lock, flags);
#endif
		return 0;
	}
	default:
		/* Try the mode commands */
		return tty_mode_ioctl(tty, file, cmd, arg);
	}
}
EXPORT_SYMBOL(n_tty_ioctl_helper);

/**
 * pci_wake_from_d3 - enable/disable device to wake up from D3_hot or D3_cold
 * @dev: PCI device to prepare
 * @enable: True to enable wake-up event generation; false to disable
 *
 * Many drivers want the device to wake up the system from D3_hot or D3_cold
 * and this function allows them to set that up cleanly - pci_enable_wake()
 * should not be called twice in a row to enable wake-up due to PCI PM vs ACPI
 * ordering constraints.
 *
 * This function only returns error code if the device is not capable of
 * generating PME# from both D3_hot and D3_cold, and the platform is unable to
 * enable wake-up power for it.
 */
int pci_wake_from_d3(struct pci_dev *dev, bool enable)
{
	return pci_pme_capable(dev, PCI_D3cold) ?
			pci_enable_wake(dev, PCI_D3cold, enable) :
			pci_enable_wake(dev, PCI_D3hot, enable);
}
EXPORT_SYMBOL(pci_wake_from_d3);

