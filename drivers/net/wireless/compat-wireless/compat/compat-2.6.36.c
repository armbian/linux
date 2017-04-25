/*
 * Copyright 2010    Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.36.
 */

#include <linux/compat.h>
#include <linux/usb.h>

#ifdef CONFIG_COMPAT_USB_URB_THREAD_FIX
/* Callers must hold anchor->lock */
static void __usb_unanchor_urb(struct urb *urb, struct usb_anchor *anchor)
{
	urb->anchor = NULL;
	list_del(&urb->anchor_list);
	usb_put_urb(urb);
	if (list_empty(&anchor->urb_list))
		wake_up(&anchor->wait);
}

/**
 * usb_unlink_anchored_urbs - asynchronously cancel transfer requests en masse
 * @anchor: anchor the requests are bound to
 *
 * this allows all outstanding URBs to be unlinked starting
 * from the back of the queue. This function is asynchronous.
 * The unlinking is just tiggered. It may happen after this
 * function has returned.
 *
 * This routine should not be called by a driver after its disconnect
 * method has returned.
 */
void compat_usb_unlink_anchored_urbs(struct usb_anchor *anchor)
{
	struct urb *victim;

	while ((victim = usb_get_from_anchor(anchor)) != NULL) {
		usb_unlink_urb(victim);
		usb_put_urb(victim);
	}
}
EXPORT_SYMBOL_GPL(compat_usb_unlink_anchored_urbs);

/**
 * usb_get_from_anchor - get an anchor's oldest urb
 * @anchor: the anchor whose urb you want
 *
 * this will take the oldest urb from an anchor,
 * unanchor and return it
 */
struct urb *compat_usb_get_from_anchor(struct usb_anchor *anchor)
{
	struct urb *victim;
	unsigned long flags;

	spin_lock_irqsave(&anchor->lock, flags);
	if (!list_empty(&anchor->urb_list)) {
		victim = list_entry(anchor->urb_list.next, struct urb,
				    anchor_list);
		usb_get_urb(victim);
		__usb_unanchor_urb(victim, anchor);
	} else {
		victim = NULL;
	}
	spin_unlock_irqrestore(&anchor->lock, flags);

	return victim;
}
EXPORT_SYMBOL_GPL(compat_usb_get_from_anchor);

/**
 * usb_scuttle_anchored_urbs - unanchor all an anchor's urbs
 * @anchor: the anchor whose urbs you want to unanchor
 *
 * use this to get rid of all an anchor's urbs
 */
void compat_usb_scuttle_anchored_urbs(struct usb_anchor *anchor)
{
	struct urb *victim;
	unsigned long flags;

	spin_lock_irqsave(&anchor->lock, flags);
	while (!list_empty(&anchor->urb_list)) {
		victim = list_entry(anchor->urb_list.prev, struct urb,
				    anchor_list);
		__usb_unanchor_urb(victim, anchor);
	}
	spin_unlock_irqrestore(&anchor->lock, flags);
}
EXPORT_SYMBOL_GPL(compat_usb_scuttle_anchored_urbs);

#endif /* CONFIG_COMPAT_USB_URB_THREAD_FIX */

struct workqueue_struct *system_nrt_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_nrt_wq);

void compat_system_workqueue_create()
{
	system_nrt_wq = create_singlethread_workqueue("events_nrt");
	WARN_ON(!system_nrt_wq);
}

void compat_system_workqueue_destroy()
{
	destroy_workqueue(system_nrt_wq);
}
