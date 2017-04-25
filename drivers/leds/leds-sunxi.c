/*
 ** leds-sunxi.c -- support led for sunxi
 **
 ** Copyright (c) 2016
 ** Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License version 2 as
 ** published by the Free Software Foundation.
 **
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sys_config.h>
#include <asm/io.h>
#include <asm/uaccess.h>
static struct gpio_led *gpio_leds;
static void *led_list_head;
static void *led_list_name;
static int led_num;

struct gpio_led_data {
	struct led_classdev cdev;
	unsigned gpio;
	struct work_struct work;
	u8 new_level;
	u8 can_sleep;
	u8 active_low;
	u8 blinking;
	int (*platform_gpio_blink_set)(unsigned gpio, int state,
			unsigned long *delay_on, unsigned long *delay_off);
};

static void gpio_led_work(struct work_struct *work)
{
	struct gpio_led_data	*led_dat =
		container_of(work, struct gpio_led_data, work);
	if (led_dat->blinking) {
		led_dat->platform_gpio_blink_set(led_dat->gpio,
				led_dat->new_level,
				NULL, NULL);
		led_dat->blinking = 0;
	} else
		gpio_set_value_cansleep(led_dat->gpio, led_dat->new_level);
}

static void gpio_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct gpio_led_data *led_dat =
		container_of(led_cdev, struct gpio_led_data, cdev);
	int level;

	if (value == LED_OFF)
		level = 0;
	else
		level = 1;

	if (led_dat->active_low)
		level = !level;

	/* Setting GPIOs with I2C/etc requires a task context, and we don't
	 * seem to have a reliable way to know if we're already in one; so
	 * let's just assume the worst.
	 */
	if (led_dat->can_sleep) {
		led_dat->new_level = level;
		schedule_work(&led_dat->work);
	} else {
		if (led_dat->blinking) {
			led_dat->platform_gpio_blink_set(led_dat->gpio, level,
					NULL, NULL);
			led_dat->blinking = 0;
		} else
			gpio_set_value(led_dat->gpio, level);
	}
}

static int gpio_blink_set(struct led_classdev *led_cdev,
		unsigned long *delay_on, unsigned long *delay_off)
{
	struct gpio_led_data *led_dat =
		container_of(led_cdev, struct gpio_led_data, cdev);

	led_dat->blinking = 1;
	return led_dat->platform_gpio_blink_set(led_dat->gpio, GPIO_LED_BLINK,
			delay_on, delay_off);
}

static int create_gpio_led(const struct gpio_led *template,
		struct gpio_led_data *led_dat, struct device *parent,
		int (*blink_set)(unsigned, int,
			unsigned long *, unsigned long *))
{
	int ret, state;

	led_dat->gpio = -1;

	/* skip leds that aren't available */
	if (!gpio_is_valid(template->gpio)) {
		dev_info(parent, "Skipping unavailable LED gpio %d (%s)\n",
				template->gpio, template->name);
		return 0;
	}

	ret = devm_gpio_request(parent, template->gpio, template->name);
	if (ret < 0)
		return ret;

	led_dat->cdev.name = template->name;
	led_dat->cdev.default_trigger = template->default_trigger;
	led_dat->gpio = template->gpio;
	led_dat->can_sleep = gpio_cansleep(template->gpio);
	led_dat->active_low = template->active_low;
	led_dat->blinking = 0;
	if (blink_set) {
		led_dat->platform_gpio_blink_set = blink_set;
		led_dat->cdev.blink_set = gpio_blink_set;
	}
	led_dat->cdev.brightness_set = gpio_led_set;
	if (template->default_state == LEDS_GPIO_DEFSTATE_KEEP)
		state = !!gpio_get_value_cansleep(led_dat->gpio)
			^ led_dat->active_low;
	else
		state = (template->default_state == LEDS_GPIO_DEFSTATE_ON);
	led_dat->cdev.brightness = state ? LED_FULL : LED_OFF;
	if (!template->retain_state_suspended)
		led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

	ret = gpio_direction_output(led_dat->gpio,
			led_dat->active_low ^ state);
	if (ret < 0)
		return ret;

	INIT_WORK(&led_dat->work, gpio_led_work);

	ret = led_classdev_register(parent, &led_dat->cdev);
	if (ret < 0)
		return ret;

	return 0;
}

static void delete_gpio_led(struct gpio_led_data *led)
{
	if (!gpio_is_valid(led->gpio))
		return;
	led_classdev_unregister(&led->cdev);
	cancel_work_sync(&led->work);
}

struct gpio_leds_priv {
	int num_leds;
	struct gpio_led_data leds[];
};

static inline int sizeof_gpio_leds_priv(int num_leds)
{
	return sizeof(struct gpio_leds_priv) +
		(sizeof(struct gpio_led_data) * num_leds);
}

static int gpio_led_probe(struct platform_device *pdev)
{
	pr_info("%s start\n", __func__);
	struct device_node *np = pdev->dev.of_node;
	struct gpio_leds_priv *priv;
	int i, ret = -1;

	u32 val;
	u32 led_used = 0;
	char trigger_name[32];
	struct gpio_config config;
	char *trigger_str;

	ret = of_property_read_u32(np, "led_used", &val);
	if (ret < 0) {
		pr_err("%s: led_used get err.\n", __func__);
		return ret;
	}
	led_used = val;

	if (1 != led_used) {
		pr_err("%s: led is unused.\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "led_num", &val);
	if (ret < 0) {
		pr_err("%s: led_num get err.\n", __func__);
		return ret;
	}
	led_num = val;

	led_list_head = kzalloc(led_num * sizeof(struct gpio_led), GFP_KERNEL);
	if (led_list_head == NULL) {
		pr_err("%s: kzalloc led list head failed.\n", __func__);
		return -ENOMEM;
	}
	gpio_leds = (struct gpio_led *) led_list_head;
	char (*led_name)[5];
	led_list_name = kzalloc(led_num * sizeof(char[5]), GFP_KERNEL);
	if (led_list_name == NULL) {
		pr_err("%s: kzalloc led list name failed.\n", __func__);
		return -ENOMEM;
	}
	led_name = (char (*)[5]) led_list_name;

	for (i = 0; i < led_num; i++) {
		sprintf(led_name[i], "%s%d", "led", i+1);
		sprintf(trigger_name, "%s%s", led_name[i], "_trigger");
		ret = of_get_named_gpio_flags(np,
				led_name[i], 0, (enum of_gpio_flags *)&config);
		if (ret < 0) {
			pr_err("%s: device_tree_get_item %s err\n",
					__func__, led_name[i]);
			return ret;
		}
		gpio_leds[i].gpio = config.gpio;
		pr_info("%s gpio number is %d\n",
				led_name[i], gpio_leds[i].gpio);

		ret = of_property_read_string(np, trigger_name, &trigger_str);
		if (ret < 0) {
			pr_err("%s: device_tree_get_item %s err\n",
					__func__, trigger_name);
			return ret;
		}
		gpio_leds[i].default_trigger = trigger_str;
		pr_info("%s led trigger name is %s\n", trigger_name,
				gpio_leds[i].default_trigger);

		gpio_leds[i].name = led_name[i];
		gpio_leds[i].active_low = 0;
		gpio_leds[i].default_state = LEDS_GPIO_DEFSTATE_OFF;
	}

	if (gpio_leds && led_num) {
		priv = devm_kzalloc(&pdev->dev,
				sizeof_gpio_leds_priv(led_num),
				GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->num_leds = led_num;

		for (i = 0; i < priv->num_leds; i++) {
			ret = create_gpio_led(&gpio_leds[i],
					&priv->leds[i],
					&pdev->dev, 0);
			if (ret < 0) {
				/* On failure: unwind the led creations */
				for (i = i - 1; i >= 0; i--)
					delete_gpio_led(&priv->leds[i]);
				kfree(priv);
				return ret;
			}
		}
	}
	platform_set_drvdata(pdev, priv);

	return 0;
}

static int gpio_led_remove(struct platform_device *pdev)
{
	struct gpio_leds_priv *priv = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < priv->num_leds; i++)
		delete_gpio_led(&priv->leds[i]);

	platform_set_drvdata(pdev, NULL);
	kfree(priv);

	return 0;
}

static const struct of_device_id sunxi_leds_ids[] = {
	{ .compatible = "allwinner,sunxi-leds" },
	{ /*  Sentinel */ }
};

static struct platform_driver sunxi_leds_driver = {
	.probe          = gpio_led_probe,
	.remove         = gpio_led_remove,
	.driver         = {
		.name       = "sunxi-leds",
		.owner      = THIS_MODULE,
		.of_match_table = sunxi_leds_ids,
	},

};

module_platform_driver(sunxi_leds_driver);

MODULE_DESCRIPTION("LED driver for tina 2.0");
MODULE_LICENSE("GPL");
