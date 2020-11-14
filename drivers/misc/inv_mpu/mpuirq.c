/*
	$License:
	Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	$
 */
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/signal.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/poll.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/mpu.h>
#include "mpuirq.h"
#include "mldl_cfg.h"

#define MPUIRQ_NAME "mpuirq"

/* function which gets accel data and sends it to MPU */

DECLARE_WAIT_QUEUE_HEAD(mpuirq_wait);

struct mpuirq_dev_data {
	struct i2c_client *mpu_client;
	struct miscdevice *dev;
	int irq;
	int pid;
	int accel_divider;
	int data_ready;
	int timeout;
};

static struct mpuirq_dev_data mpuirq_dev_data;
static struct mpuirq_data mpuirq_data;
static char *interface = MPUIRQ_NAME;

static int mpuirq_open(struct inode *inode, struct file *file)
{
	dev_dbg(mpuirq_dev_data.dev->this_device,
		"%s current->pid %d\n", __func__, current->pid);
	mpuirq_dev_data.pid = current->pid;
	file->private_data = &mpuirq_dev_data;
	return 0;
}

/* close function - called when the "file" /dev/mpuirq is closed in userspace */
static int mpuirq_release(struct inode *inode, struct file *file)
{
	dev_dbg(mpuirq_dev_data.dev->this_device, "mpuirq_release\n");
	return 0;
}

/* read function called when from /dev/mpuirq is read */
static ssize_t mpuirq_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct mpuirq_dev_data *p_mpuirq_dev_data = file->private_data;

	if (!mpuirq_dev_data.data_ready &&
	    mpuirq_dev_data.timeout && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible_timeout(mpuirq_wait,
						 mpuirq_dev_data.data_ready,
						 mpuirq_dev_data.timeout);
	}

	if (mpuirq_dev_data.data_ready && NULL != buf
	    && count >= sizeof(mpuirq_data)) {
		err = copy_to_user(buf, &mpuirq_data, sizeof(mpuirq_data));
		mpuirq_data.data_type = 0;
	} else {
		return 0;
	}
	if (err != 0) {
		dev_err(p_mpuirq_dev_data->dev->this_device,
			"Copy to user returned %d\n", err);
		return -EFAULT;
	}
	mpuirq_dev_data.data_ready = 0;
	len = sizeof(mpuirq_data);
	return len;
}

unsigned int mpuirq_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;

	poll_wait(file, &mpuirq_wait, poll);
	if (mpuirq_dev_data.data_ready)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

/* ioctl - I/O control */
static long mpuirq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int data;

	switch (cmd) {
	case MPUIRQ_SET_TIMEOUT:
		mpuirq_dev_data.timeout = arg;
		break;

	case MPUIRQ_GET_INTERRUPT_CNT:
		data = mpuirq_data.interruptcount - 1;
		if (mpuirq_data.interruptcount > 1)
			mpuirq_data.interruptcount = 1;

		if (copy_to_user((int *)arg, &data, sizeof(int)))
			return -EFAULT;
		break;
	case MPUIRQ_GET_IRQ_TIME:
		if (copy_to_user((int *)arg, &mpuirq_data.irqtime,
				 sizeof(mpuirq_data.irqtime)))
			return -EFAULT;
		mpuirq_data.irqtime = 0;
		break;
	case MPUIRQ_SET_FREQUENCY_DIVIDER:
		mpuirq_dev_data.accel_divider = arg;
		break;
	default:
		retval = -EINVAL;
	}
	return retval;
}

static irqreturn_t mpuirq_handler(int irq, void *dev_id)
{
	static int mycount;
	struct timeval irqtime;
	mycount++;

	mpuirq_data.interruptcount++;

	/* wake up (unblock) for reading data from userspace */
	/* and ignore first interrupt generated in module init */
	mpuirq_dev_data.data_ready = 1;

	do_gettimeofday(&irqtime);
	mpuirq_data.irqtime = (((long long)irqtime.tv_sec) << 32);
	mpuirq_data.irqtime += irqtime.tv_usec;
	mpuirq_data.data_type = MPUIRQ_DATA_TYPE_MPU_IRQ;
	mpuirq_data.data = 0;

	wake_up_interruptible(&mpuirq_wait);

	return IRQ_HANDLED;

}

/* define which file operations are supported */
const struct file_operations mpuirq_fops = {
	.owner = THIS_MODULE,
	.read = mpuirq_read,
	.poll = mpuirq_poll,

	.unlocked_ioctl = mpuirq_ioctl,
	.open = mpuirq_open,
	.release = mpuirq_release,
};

static struct miscdevice mpuirq_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MPUIRQ_NAME,
	.fops = &mpuirq_fops,
};

int mpuirq_init(struct i2c_client *mpu_client, struct mldl_cfg *mldl_cfg)
{

	int res;

	mpuirq_dev_data.mpu_client = mpu_client;

	dev_info(&mpu_client->adapter->dev,
		 "Module Param interface = %s\n", interface);

	mpuirq_dev_data.irq = mpu_client->irq;
	mpuirq_dev_data.pid = 0;
	mpuirq_dev_data.accel_divider = -1;
	mpuirq_dev_data.data_ready = 0;
	mpuirq_dev_data.timeout = 0;
	mpuirq_dev_data.dev = &mpuirq_device;

	if (mpuirq_dev_data.irq) {
		unsigned long flags;
		if (BIT_ACTL_LOW == ((mldl_cfg->pdata->int_config) & BIT_ACTL))
			flags = IRQF_TRIGGER_FALLING;
		else
			flags = IRQF_TRIGGER_RISING;

		flags =IRQF_TRIGGER_RISING;//|= IRQF_SHARED;

		res =devm_request_threaded_irq(&mpu_client->dev, mpuirq_dev_data.irq, NULL, mpuirq_handler, flags|IRQF_ONESHOT, interface, &mpuirq_dev_data.irq);
		//res =
		 //   request_irq(mpuirq_dev_data.irq, mpuirq_handler, flags,
		//		interface, &mpuirq_dev_data.irq);  
		if (res) {
			dev_err(&mpu_client->adapter->dev,
				"myirqtest: cannot register IRQ %d\n",
				mpuirq_dev_data.irq);
		} else {
			res = misc_register(&mpuirq_device);
			if (res < 0) {
				dev_err(&mpu_client->adapter->dev,
					"misc_register returned %d\n", res);
				free_irq(mpuirq_dev_data.irq,
					 &mpuirq_dev_data.irq);
			}
		}

	} else {
		res = 0;
	}

	return res;
}

void mpuirq_exit(void)
{
	if (mpuirq_dev_data.irq > 0)
		free_irq(mpuirq_dev_data.irq, &mpuirq_dev_data.irq);

	dev_info(mpuirq_device.this_device, "Unregistering %s\n", MPUIRQ_NAME);
	misc_deregister(&mpuirq_device);

	return;
}

module_param(interface, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(interface, "The Interface name");
