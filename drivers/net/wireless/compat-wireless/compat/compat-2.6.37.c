/*
 * Copyright 2010    Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.37.
 */

#include <linux/compat.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/nsproxy.h>
#include <linux/vmalloc.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
static const void *net_current_ns(void)
{
	return current->nsproxy->net_ns;
}

static const void *net_initial_ns(void)
{
	return &init_net;
}

static const void *net_netlink_ns(struct sock *sk)
{
	return sock_net(sk);
}

struct kobj_ns_type_operations net_ns_type_operations = {
	.type = KOBJ_NS_TYPE_NET,
	.current_ns = net_current_ns,
	.netlink_ns = net_netlink_ns,
	.initial_ns = net_initial_ns,
};
EXPORT_SYMBOL_GPL(net_ns_type_operations);

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)*/ 

#undef genl_info
#undef genl_unregister_family

static LIST_HEAD(compat_nl_fam);

static struct genl_ops *genl_get_cmd(u8 cmd, struct genl_family *family)
{
	struct genl_ops *ops;

	list_for_each_entry(ops, &family->family.ops_list, ops.ops_list)
		if (ops->cmd == cmd)
			return ops;

	return NULL;
}


static int nl_doit_wrapper(struct sk_buff *skb, struct genl_info *info)
{
	struct compat_genl_info compat_info;
	struct genl_family *family;
	struct genl_ops *ops;
	int err;

	list_for_each_entry(family, &compat_nl_fam, list) {
		if (family->id == info->nlhdr->nlmsg_type)
			goto found;
	}
	return -ENOENT;

found:
	ops = genl_get_cmd(info->genlhdr->cmd, family);
	if (!ops)
		return -ENOENT;

	memset(&compat_info.user_ptr, 0, sizeof(compat_info.user_ptr));
	compat_info.info = info;
#define __copy(_field) compat_info._field = info->_field
	__copy(snd_seq);
	__copy(snd_pid);
	__copy(genlhdr);
	__copy(attrs);
#undef __copy
	if (family->pre_doit) {
		err = family->pre_doit(ops, skb, &compat_info);
		if (err)
			return err;
	}

	err = ops->doit(skb, &compat_info);

	if (family->post_doit)
		family->post_doit(ops, skb, &compat_info);

	return err;
}

int compat_genl_register_family_with_ops(struct genl_family *family,
					 struct genl_ops *ops, size_t n_ops)
{
	int i, ret;

#define __copy(_field) family->family._field = family->_field
	__copy(id);
	__copy(hdrsize);
	__copy(version);
	__copy(maxattr);
	strncpy(family->family.name, family->name, sizeof(family->family.name));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	__copy(netnsok);
#endif
#undef __copy

	ret = genl_register_family(&family->family);
	if (ret < 0)
		return ret;

	family->attrbuf = family->family.attrbuf;
	family->id = family->family.id;

	for (i = 0; i < n_ops; i++) {
#define __copy(_field) ops[i].ops._field = ops[i]._field
		__copy(cmd);
		__copy(flags);
		__copy(policy);
		__copy(dumpit);
		__copy(done);
#undef __copy
		if (ops[i].doit)
			ops[i].ops.doit = nl_doit_wrapper;
		ret = genl_register_ops(&family->family, &ops[i].ops);
		if (ret < 0)
			goto error_ops;
	}
	list_add(&family->list, &compat_nl_fam);

	return ret;

error_ops:
	compat_genl_unregister_family(family);
	return ret;
}
EXPORT_SYMBOL(compat_genl_register_family_with_ops);

int compat_genl_unregister_family(struct genl_family *family)
{
	int err;
	err = genl_unregister_family(&family->family);
	list_del(&family->list);
	return err;
}
EXPORT_SYMBOL(compat_genl_unregister_family);

#if defined(CONFIG_LEDS_CLASS) || defined(CONFIG_LEDS_CLASS_MODULE)

#undef led_brightness_set
#undef led_classdev_unregister

static DEFINE_SPINLOCK(led_lock);
static LIST_HEAD(led_timers);

struct led_timer {
	struct list_head list;
	struct led_classdev *cdev;
	struct timer_list blink_timer;
	unsigned long blink_delay_on;
	unsigned long blink_delay_off;
	int blink_brightness;
};

static void led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	led_cdev->brightness = brightness;
	led_cdev->brightness_set(led_cdev, brightness);
}

static struct led_timer *led_get_timer(struct led_classdev *led_cdev)
{
	struct led_timer *p;
	unsigned long flags;

	spin_lock_irqsave(&led_lock, flags);
	list_for_each_entry(p, &led_timers, list) {
		if (p->cdev == led_cdev)
			goto found;
	}
	p = NULL;
found:
	spin_unlock_irqrestore(&led_lock, flags);
	return p;
}

static void led_stop_software_blink(struct led_timer *led)
{
	del_timer_sync(&led->blink_timer);
	led->blink_delay_on = 0;
	led->blink_delay_off = 0;
}

static void led_timer_function(unsigned long data)
{
	struct led_timer *led = (struct led_timer *)data;
	unsigned long brightness;
	unsigned long delay;

	if (!led->blink_delay_on || !led->blink_delay_off) {
		led->cdev->brightness_set(led->cdev, LED_OFF);
		return;
	}

	brightness = led->cdev->brightness;
	if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led->blink_brightness;
		delay = led->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led->blink_delay_off;
	}

	led_brightness_set(led->cdev, brightness);
	mod_timer(&led->blink_timer, jiffies + msecs_to_jiffies(delay));
}

static struct led_timer *led_new_timer(struct led_classdev *led_cdev)
{
	struct led_timer *led;
	unsigned long flags;

	led = kzalloc(sizeof(struct led_timer), GFP_ATOMIC);
	if (!led)
		return NULL;

	led->cdev = led_cdev;
	init_timer(&led->blink_timer);
	led->blink_timer.function = led_timer_function;
	led->blink_timer.data = (unsigned long) led;

	spin_lock_irqsave(&led_lock, flags);
	list_add(&led->list, &led_timers);
	spin_unlock_irqrestore(&led_lock, flags);

	return led;
}

void led_blink_set(struct led_classdev *led_cdev,
		   unsigned long *delay_on,
		   unsigned long *delay_off)
{
	struct led_timer *led;
	int current_brightness;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))
	if (led_cdev->blink_set &&
	    !led_cdev->blink_set(led_cdev, delay_on, delay_off))
		return;
#endif

	led = led_get_timer(led_cdev);
	if (!led) {
		led = led_new_timer(led_cdev);
		if (!led)
			return;
	}

	/* blink with 1 Hz as default if nothing specified */
	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;

	if (led->blink_delay_on == *delay_on &&
	    led->blink_delay_off == *delay_off)
		return;

	current_brightness = led_cdev->brightness;
	if (current_brightness)
		led->blink_brightness = current_brightness;
	if (!led->blink_brightness)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
		led->blink_brightness = led_cdev->max_brightness;
#else
		led->blink_brightness = LED_FULL;
#endif

	led_stop_software_blink(led);
	led->blink_delay_on = *delay_on;
	led->blink_delay_off = *delay_off;

	/* never on - don't blink */
	if (!*delay_on)
		return;

	/* never off - just set to brightness */
	if (!*delay_off) {
		led_brightness_set(led_cdev, led->blink_brightness);
		return;
	}

	mod_timer(&led->blink_timer, jiffies + 1);
}
EXPORT_SYMBOL(led_blink_set);

void compat_led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	struct led_timer *led = led_get_timer(led_cdev);

	if (led)
		led_stop_software_blink(led);

	return led_cdev->brightness_set(led_cdev, brightness);
}
EXPORT_SYMBOL(compat_led_brightness_set);

void compat_led_classdev_unregister(struct led_classdev *led_cdev)
{
	struct led_timer *led = led_get_timer(led_cdev);
	unsigned long flags;

	if (led) {
		del_timer_sync(&led->blink_timer);
		spin_lock_irqsave(&led_lock, flags);
		list_del(&led->list);
		spin_unlock_irqrestore(&led_lock, flags);
		kfree(led);
	}

	led_classdev_unregister(led_cdev);
}
EXPORT_SYMBOL(compat_led_classdev_unregister);

/**
 *	vzalloc - allocate virtually contiguous memory with zero fill
 *	@size:	allocation size
 *	Allocate enough pages to cover @size from the page level
 *	allocator and map them into contiguous kernel virtual space.
 *	The memory allocated is set to zero.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */
void *vzalloc(unsigned long size)
{
	void *buf;
	buf = vmalloc(size);
	if (buf)
		memset(buf, 0, size);
	return buf;
}
EXPORT_SYMBOL(vzalloc);

#endif
