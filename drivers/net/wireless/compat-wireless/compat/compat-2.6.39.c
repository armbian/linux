/*
 * Copyright 2011    Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.39.
 */

#include <linux/compat.h>
#include <linux/tty.h>
#include <linux/sched.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
/*
 *		Termios Helper Methods
 */
static void unset_locked_termios(struct ktermios *termios,
				 struct ktermios *old,
				 struct ktermios *locked)
{
	int	i;

#define NOSET_MASK(x, y, z) (x = ((x) & ~(z)) | ((y) & (z)))

	if (!locked) {
		printk(KERN_WARNING "Warning?!? termios_locked is NULL.\n");
		return;
	}

	NOSET_MASK(termios->c_iflag, old->c_iflag, locked->c_iflag);
	NOSET_MASK(termios->c_oflag, old->c_oflag, locked->c_oflag);
	NOSET_MASK(termios->c_cflag, old->c_cflag, locked->c_cflag);
	NOSET_MASK(termios->c_lflag, old->c_lflag, locked->c_lflag);
	termios->c_line = locked->c_line ? old->c_line : termios->c_line;
	for (i = 0; i < NCCS; i++)
		termios->c_cc[i] = locked->c_cc[i] ?
			old->c_cc[i] : termios->c_cc[i];
	/* FIXME: What should we do for i/ospeed */
}

/**
 *	tty_set_termios		-	update termios values
 *	@tty: tty to update
 *	@new_termios: desired new value
 *
 *	Perform updates to the termios values set on this terminal. There
 *	is a bit of layering violation here with n_tty in terms of the
 *	internal knowledge of this function.
 *
 *	Locking: termios_mutex
 */
int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios)
{
	struct ktermios old_termios;
	struct tty_ldisc *ld;
	unsigned long flags;

	/*
	 *	Perform the actual termios internal changes under lock.
	 */


	/* FIXME: we need to decide on some locking/ordering semantics
	   for the set_termios notification eventually */
	mutex_lock(&tty->termios_mutex);
	old_termios = *tty->termios;
	*tty->termios = *new_termios;
	unset_locked_termios(tty->termios, &old_termios, tty->termios_locked);

	/* See if packet mode change of state. */
	if (tty->link && tty->link->packet) {
		int extproc = (old_termios.c_lflag & EXTPROC) |
				(tty->termios->c_lflag & EXTPROC);
		int old_flow = ((old_termios.c_iflag & IXON) &&
				(old_termios.c_cc[VSTOP] == '\023') &&
				(old_termios.c_cc[VSTART] == '\021'));
		int new_flow = (I_IXON(tty) &&
				STOP_CHAR(tty) == '\023' &&
				START_CHAR(tty) == '\021');
		if ((old_flow != new_flow) || extproc) {
			spin_lock_irqsave(&tty->ctrl_lock, flags);
			if (old_flow != new_flow) {
				tty->ctrl_status &= ~(TIOCPKT_DOSTOP | TIOCPKT_NOSTOP);
				if (new_flow)
					tty->ctrl_status |= TIOCPKT_DOSTOP;
				else
					tty->ctrl_status |= TIOCPKT_NOSTOP;
			}
			if (extproc)
				tty->ctrl_status |= TIOCPKT_IOCTL;
			spin_unlock_irqrestore(&tty->ctrl_lock, flags);
			wake_up_interruptible(&tty->link->read_wait);
		}
	}

	if (tty->ops->set_termios)
		(*tty->ops->set_termios)(tty, &old_termios);
	else
		tty_termios_copy_hw(tty->termios, &old_termios);

	ld = tty_ldisc_ref(tty);
	if (ld != NULL) {
		if (ld->ops->set_termios)
			(ld->ops->set_termios)(tty, &old_termios);
		tty_ldisc_deref(ld);
	}
	mutex_unlock(&tty->termios_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(tty_set_termios);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)) */

