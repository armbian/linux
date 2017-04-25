/*
 * drivers/media/video/tsc/tscdrv.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * csjamesdeng <csjamesdeng@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/rmap.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <asm-generic/gpio.h>
#include <asm/current.h>
#include <linux/sys_config.h>
#include <linux/platform_device.h>
#include <linux/clk/sunxi.h>

#include "dvb_drv_sun5i.h"
#include "tscdrv.h"
#include <asm/io.h>


/* add by xhw */
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>

static struct tsc_dev *tsc_devp;
static struct device_node *node;
int tsc_dev_major = TSCDEV_MAJOR;
int tsc_dev_minor = TSCDEV_MINOR;
static spinlock_t tsc_spin_lock;
static int clk_status;

static struct of_device_id sun50i_tsc_match[] = {
	{ .compatible = "allwinner,sun50i-tsc",},
	{}
};

MODULE_DEVICE_TABLE(of, sun50i_tsc_match);

static DECLARE_WAIT_QUEUE_HEAD(wait_proc);

#ifdef CONFIG_PM

int enable_tsc_hw_clk(void)
{
	unsigned long flags;
	int res = -EFAULT;

	spin_lock_irqsave(&tsc_spin_lock, flags);

	if (clk_status == 1)
		goto out;

	clk_status = 1;

	sunxi_periph_reset_deassert(tsc_devp->tsc_clk);
	if (clk_enable(tsc_devp->tsc_clk)) {
		pr_err("enable tsc_clk failed\n");
		goto out;
	} else {
		res = 0;
	}

out:
	spin_unlock_irqrestore(&tsc_spin_lock, flags);
	return res;
}

int disable_tsc_hw_clk(void)
{
	unsigned long flags;
	int res = -EFAULT;

	spin_lock_irqsave(&tsc_spin_lock, flags);

	if (clk_status == 0) {
		res = 0;
		goto out;
	}
	clk_status = 0;

	if ((NULL == tsc_devp->tsc_clk) || (IS_ERR(tsc_devp->tsc_clk))) {
		pr_err("tsc_clk is invalid, just return!\n");
	} else {
		clk_disable(tsc_devp->tsc_clk);
		sunxi_periph_reset_assert(tsc_devp->tsc_clk);
		res = 0;
	}

out:
	spin_unlock_irqrestore(&tsc_spin_lock, flags);
	return res;
}

#endif

/*
 * interrupt service routine
 * To wake up wait queue
 */
static irqreturn_t tscdriverinterrupt(int irq, void *dev_id)
{
	struct iomap_para addrs = tsc_devp->iomap_addrs;
	unsigned long tsc_int_status_reg;
	unsigned long tsc_int_ctrl_reg;
	unsigned int  tsc_status;
	unsigned int  tsc_interrupt_enable;


	tsc_int_ctrl_reg   = (unsigned long)(addrs.regs_macc + 0x80 + 0x08);
	tsc_int_status_reg = (unsigned long)(addrs.regs_macc + 0x80 + 0x18);
	tsc_interrupt_enable = ioread32((void *)(tsc_int_ctrl_reg));
	tsc_status = ioread32((void *)(tsc_int_status_reg));

	iowrite32(tsc_interrupt_enable, (void *)(tsc_int_ctrl_reg));
	iowrite32(tsc_status, (void *)(tsc_int_status_reg));

	tsc_devp->irq_flag = 1;
	wake_up_interruptible(&wait_proc);

	return IRQ_HANDLED;
}

static void close_all_fillters(struct tsc_dev *devp)
{
	int i;
	unsigned int value = 0;
	struct iomap_para addrs = tsc_devp->iomap_addrs;

	/*close tsf0*/
	iowrite32(0, (void *)(addrs.regs_macc + 0x80 + 0x10));
	iowrite32(0, (void *)(addrs.regs_macc + 0x80 + 0x30));

	for (i = 0; i < 32; i++) {
		iowrite32(i, (void *)(addrs.regs_macc + 0x80 + 0x3c));
		value = (0<<16 | 0x1fff);
		iowrite32(value, (void *)(addrs.regs_macc + 0x80 + 0x4c));
	}
}

/*
 * poll operateion for wait for TS irq
 */
unsigned int tscdev_poll(struct file *filp, struct poll_table_struct *wait)
{
	int mask = 0;

	poll_wait(filp, &tsc_devp->wq, wait);

	if (tsc_devp->irq_flag == 1)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/*
 * ioctl function
 */
long tscdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;
	/*struct clk_para temp_clk;*/
	/*struct clk *clk_hdle;*/
	struct intrstatus statusdata;

	int arg_rate = (int)arg;
	unsigned long tsc_parent_clk_rate;

	ret = 0;

	if (_IOC_TYPE(cmd) != TSCDEV_IOC_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) > TSCDEV_IOC_MAXNR)
		return -EINVAL;

	switch (cmd) {
	case TSCDEV_WAIT_INT:
		ret = wait_event_interruptible_timeout(wait_proc, tsc_devp->irq_flag, HZ * 1);
		if (!ret && !tsc_devp->irq_flag) {
			/*case: wait timeout.*/
			pr_err("%s: wait timeout\n", __func__);
			memset(&statusdata, 0, sizeof(statusdata));
		} else {
		/*case: interrupt occured.*/
			tsc_devp->irq_flag = 0;
			statusdata.port0chan = tsc_devp->intstatus.port0chan;
			statusdata.port0pcr = tsc_devp->intstatus.port0pcr;
		}

		/*copy status data to user*/
		if (copy_to_user((struct intrstatus *)arg, &(tsc_devp->intstatus),
					sizeof(struct intrstatus))) {
			return -EFAULT;
		}

		break;

	case TSCDEV_GET_PHYSICS:
		return 0;

	case TSCDEV_ENABLE_INT:
		enable_irq(tsc_devp->irq);
		break;

	case TSCDEV_DISABLE_INT:
		tsc_devp->irq_flag = 1;
		wake_up_interruptible(&wait_proc);
		disable_irq(tsc_devp->irq);
		break;

	case TSCDEV_RELEASE_SEM:
		tsc_devp->irq_flag = 1;
		wake_up_interruptible(&wait_proc);
		break;

	case TSCDEV_GET_CLK:

		tsc_devp->tsc_clk = of_clk_get(node, 1);
		if (!tsc_devp->tsc_clk || IS_ERR(tsc_devp->tsc_clk)) {
			pr_err("%s: get tsc clk failed\n", __func__);
			ret = -EINVAL;
		}

		break;

	case TSCDEV_PUT_CLK:

		clk_put(tsc_devp->tsc_clk);

		break;

	case TSCDEV_ENABLE_CLK:

		clk_prepare_enable(tsc_devp->tsc_clk);

		break;

	case TSCDEV_DISABLE_CLK:

		clk_disable_unprepare(tsc_devp->tsc_clk);

		break;

	case TSCDEV_GET_CLK_FREQ:

		ret = clk_get_rate(tsc_devp->tsc_clk);

		break;

	case TSCDEV_SET_SRC_CLK_FREQ:

		pr_info("tsc_devp->iomap_addrs.regs_macc: %p\n",
			tsc_devp->iomap_addrs.regs_macc);
		pr_info("0-tsc_top_00:%x\n",
			readl(tsc_devp->iomap_addrs.regs_macc));

		writel(0x1, tsc_devp->iomap_addrs.regs_macc);

		pr_info("1-tsc_top_00:%x\n",
			readl(tsc_devp->iomap_addrs.regs_macc));
		break;

	case TSCDEV_SET_CLK_FREQ:

		if (clk_get_rate(tsc_devp->tsc_clk)/1000000 != arg_rate) {
			if (!clk_set_rate(tsc_devp->tsc_parent_pll_clk,
					arg_rate*1000000)) {
				tsc_parent_clk_rate = clk_get_rate
					(tsc_devp->tsc_parent_pll_clk);
				if (clk_set_rate(tsc_devp->tsc_clk,
						tsc_parent_clk_rate))
					pr_err("set ve clock failed\n");

			} else {
				pr_err("set pll4 clock failed\n");
			}
		}
		ret = clk_get_rate(tsc_devp->tsc_clk);

		break;

	default:
		pr_err("%s: invalid cmd\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tscdev_open(struct inode *inode, struct file *filp)
{
	/*unsigned long clk_rate;*/

	if (down_interruptible(&tsc_devp->sem)) {
		pr_err("%s: down_interruptible failed\n", __func__);
		return -ERESTARTSYS;
	}

#if 0
	/*open ts clock*/
	tsc_devp->tsc_clk = of_clk_get(node, 1);
	if (!tsc_devp->tsc_clk || IS_ERR(tsc_devp->tsc_clk)) {
		pr_err("%s: get tsc clk failed\n", __func__);
		return -EINVAL;
	}

	if (clk_prepare_enable(tsc_devp->tsc_clk) < 0) {
		pr_err("%s: enable tsc clk error\n", __func__);
		return -EINVAL;
	}

	tsc_devp->tsc_parent_pll_clk = of_clk_get(node, 0);
	if (!tsc_devp->tsc_parent_pll_clk ||
		IS_ERR(tsc_devp->tsc_parent_pll_clk)) {
		pr_err("%s: get pll5 clk failed\n", __func__);
		return -EINVAL;
	}

	/* no reset tsc module*/
	sunxi_periph_reset_assert(tsc_devp->tsc_clk);

	clk_set_parent(tsc_devp->tsc_clk, tsc_devp->tsc_parent_pll_clk);

	/*set ts clock rate*/
	clk_rate = clk_get_rate(tsc_devp->tsc_parent_pll_clk);
	pr_info("%s: parent clock rate %ld\n", __func__, clk_rate);

	clk_rate /= 2;
	pr_info("%s: clock rate %ld\n", __func__, clk_rate);

	if (clk_set_rate(tsc_devp->tsc_clk, clk_rate) < 0) {
		pr_err("%s: set clk rate error\n", __func__);
		return -EINVAL;
	}

	clk_rate = clk_get_rate(tsc_devp->tsc_clk);
	pr_info("%s: clock rate %ld", __func__, clk_rate);
#endif

#if 0
	if (clk_prepare_enable(tsc_devp->tsc_clk) < 0) {
		pr_err("%s: enable tsc clk error\n", __func__);
		return -EINVAL;
	}
#endif

	/* init other resource here */
	tsc_devp->irq_flag = 0;

	up(&tsc_devp->sem);
	nonseekable_open(inode, filp);

	return 0;
#if 0
err:
	if (tsc_devp->tsc_clk) {
		clk_disable_unprepare(tsc_devp->tsc_clk);
		clk_put(tsc_devp->tsc_clk);
		tsc_devp->tsc_clk = NULL;
	}

	if (tsc_devp->tsc_parent_pll_clk) {
		clk_put(tsc_devp->tsc_parent_pll_clk);
		tsc_devp->tsc_parent_pll_clk = NULL;
	}

	return ret;
#endif
}

static int tscdev_release(struct inode *inode, struct file *filp)
{
	pr_info("tscdev_release");
	if (down_interruptible(&tsc_devp->sem))
		return -ERESTARTSYS;
	close_all_fillters(tsc_devp);

#if 0
	if (tsc_devp->tsc_clk) {
		clk_disable_unprepare(tsc_devp->tsc_clk);
		clk_put(tsc_devp->tsc_clk);
		tsc_devp->tsc_clk = NULL;
	}

	if (tsc_devp->tsc_parent_pll_clk) {
		clk_put(tsc_devp->tsc_parent_pll_clk);
		tsc_devp->tsc_parent_pll_clk = NULL;
	}
#endif

	/* release other resource here */
	tsc_devp->irq_flag = 1;
	up(&tsc_devp->sem);

	return 0;
}

void tscdev_vma_open(struct vm_area_struct *vma)
{
	pr_info("%s\n", __func__);
}

void tscdev_vma_close(struct vm_area_struct *vma)
{
	pr_info("%s\n", __func__);
}

static struct vm_operations_struct tscdev_remap_vm_ops = {
	.open  = tscdev_vma_open,
	.close = tscdev_vma_close,
};

static int tscdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long temp_pfn;

	if (vma->vm_end - vma->vm_start == 0) {
		pr_err("vma->vm_end is equal vma->vm_start : %lx\n",
			vma->vm_start);
		return 0;
	}
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) {
		pr_err("vma->vm_pgoff is %lx,largest page number\n",
			vma->vm_pgoff);
		return -EINVAL;
	}

	temp_pfn = REGS_BASE >> 12;
	/* Set reserved and I/O flag for the area. */
	vma->vm_flags |= /*VM_RESERVED | */VM_IO;
	/* Select uncached access. */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, temp_pfn,
			(vma->vm_end - vma->vm_start), vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &tscdev_remap_vm_ops;
	tscdev_vma_open(vma);

	return 0;

}


static int tsc_select_gpio_state(struct pinctrl *pctrl, char *name)
{
	int ret = 0;
	struct pinctrl_state *pctrl_state = NULL;

	pctrl_state = pinctrl_lookup_state(pctrl, name);
	if (IS_ERR(pctrl_state)) {
		pr_err("TSC pinctrl_lookup_state(%s) failed! return %p\n",
			name, pctrl_state);
		return -1;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret < 0)
		pr_err("TSC pinctrl_select_state(%s) failed! return %d\n",
			name, ret);

	return ret;
}

static unsigned int request_tsc_pio(struct platform_device *pdev)
{
	/* request device pinctrl, set as default state */
#if 0
	tsc_devp->pinctrl = devm_pinctrl_get_select_default(pdev->dev);
	if (IS_ERR_OR_NULL(tsc_devp->pinctrl)) {
		pr_err("request pinctrl handle for device [%s] failed, pinctrl: %p\n",
				dev_name(pdev->dev), tsc_devp->pinctrl);
		return -EINVAL;
	}

	return 0;
#else
	tsc_devp->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(tsc_devp->pinctrl)) {
		pr_err("devm get pinctrl handle for device failed\n");
		return -1;
	}

	return tsc_select_gpio_state(tsc_devp->pinctrl, "default");
#endif
}

static void release_tsc_pio(void)
{
	if (tsc_devp->pinctrl != NULL) {
		devm_pinctrl_put(tsc_devp->pinctrl);
	}
}

static struct file_operations tscdev_fops = {
	.owner          = THIS_MODULE,
	.mmap           = tscdev_mmap,
	.poll           = tscdev_poll,
	.open           = tscdev_open,
	.release        = tscdev_release,
	.llseek         = no_llseek,
	.unlocked_ioctl = tscdev_ioctl,
};

static int  tscdev_init(struct platform_device *pdev)

{
#if 0
	int ret;
	int devno;
	dev_t dev = 0;

	script_item_value_type_e type;
	script_item_u val;

	/* add by xhw*/
	/*struct device_node *node;*/
	node = pdev->dev.of_node;
	type = script_get_item("ts0", "ts0_used", &val);
	if (type != SCIRPT_ITEM_VALUE_TYPE_INT) {
		pr_err("%s: get csi0_used failed\n", __func__);
		return 0;
	}

	if (val.val != 1) {
		pr_err("%s: tsc driver is disabled\n", __func__);
		return 0;
	}
	tsc_used = val.val;

	tsc_devp = kmalloc(sizeof(struct tsc_dev), GFP_KERNEL);
	if (tsc_devp == NULL) {
		pr_err("%s: malloc memory for tsc device error\n", __func__);
		return -ENOMEM;
	}
	memset(tsc_devp, 0, sizeof(struct tsc_dev));

	tsc_devp->ts_dev_major  = TSCDEV_MAJOR;
	tsc_devp->ts_dev_minor  = TSCDEV_MINOR;

	/* register char device */
	dev = MKDEV(tsc_devp->ts_dev_major, tsc_devp->ts_dev_minor);
	if (tsc_devp->ts_dev_major) {
		ret = register_chrdev_region(dev, 1, "ts0");
	} else {
		ret = alloc_chrdev_region(&dev,
			tsc_devp->ts_dev_minor, 1, "ts0");
		tsc_devp->ts_dev_major = MAJOR(dev);
		tsc_devp->ts_dev_minor = MINOR(dev);
	}
	if (ret < 0) {
		pr_err("%s: can't get major %d",
			__func__, tsc_devp->ts_dev_major);
		return -EFAULT;
	}

	sema_init(&tsc_devp->sem, 1);
	init_waitqueue_head(&tsc_devp->wq);

	/* request TS irq */
	ret = request_irq(TS_IRQ_NO, tscdriverinterrupt, 0, "ts0", NULL);
	if (ret < 0) {
		pr_err("%s: request irq error\n", __func__);
		ret = -EINVAL;
		goto err2;
	}
	/*tsc_devp->irq = TS_IRQ_NO;*/
	/* add by xhw */

	tsc_devp->irq = irq_of_parse_and_map(node, 0);
	pr_info("The irq is %d\n", tsc_devp->irq);
	if (tsc_devp->irq <= 0)
		pr_err("Can't parse IRQ");

	/* create char device */
	devno = MKDEV(tsc_devp->ts_dev_major, tsc_devp->ts_dev_minor);
	cdev_init(&tsc_devp->cdev, &tscdev_fops);
	tsc_devp->cdev.owner = THIS_MODULE;
	tsc_devp->cdev.ops = &tscdev_fops;
	ret = cdev_add(&tsc_devp->cdev, devno, 1);
	if (ret) {
		pr_err("%s: add tsc char device error\n", __func__);
		ret = -EINVAL;
		goto err3;
	}

	tsc_devp->class = class_create(THIS_MODULE, "ts0");
	if (IS_ERR(tsc_devp->class)) {
		pr_err("%s: create tsc_dev class failed\n", __func__);
		ret = -EINVAL;
		goto err4;
	}

#if 0
	tsc_devp->dev = device_create(tsc_devp->class,
		NULL, devno, NULL, "ts0");
	if (IS_ERR(tsc_devp->dev)) {
		pr_err("%s: create tsc_dev device failed\n", __func__);
		ret = -EINVAL;
		goto err5;
	}
#endif
	snprintf(tsc_devp->name, sizeof(tsc_devp->name), "ts0");
	pdev->dev.init_name = tsc_devp->name;

	/* request TS pio */
#if 1
	if (request_tsc_pio(pdev)) {
		pr_err("%s: request tsc pio failed\n", __func__);
		ret = -EINVAL;
		goto err6;
	}
#else
	ret = request_tsc_pio();
	if (ret < 0) {
		pr_err("%s: request tsc pio failed\n", __func__);
		ret = -1;
		goto err6;
	}
#endif
	if (register_tsiomem(tsc_devp)) {
		pr_err("%s: register ts io memory error\n", __func__);
		ret = -EINVAL;
		goto err7;
	}

	return 0;

err7:
	release_tsc_pio();
err6:
	device_destroy(tsc_devp->class, dev);
err5:
	class_destroy(tsc_devp->class);
err4:
	cdev_del(&tsc_devp->cdev);
err3:
	free_irq(TS_IRQ_NO, NULL);
err2:
	unregister_chrdev_region(dev, 1);
	kfree(tsc_devp);

	return ret;
#endif

	int ret = 0;
	int devno;
	unsigned long clk_rate;
	struct device_node *node;
	dev_t dev;

	dev = 0;

	pr_info("[tsc]: install start!\n");

	node = pdev->dev.of_node;

	/*register or alloc the device number.*/
	if (tsc_dev_major) {
		dev = MKDEV(tsc_dev_major, tsc_dev_minor);
		ret = register_chrdev_region(dev, 1, "ts0");
	} else {
		ret = alloc_chrdev_region(&dev, tsc_dev_minor, 1, "ts0");
		tsc_dev_major = MAJOR(dev);
		tsc_dev_minor = MINOR(dev);
	}

	if (ret < 0) {
		pr_err(KERN_WARNING "ts0: can't get major %d\n", tsc_dev_major);
		return ret;
	}
	spin_lock_init(&tsc_spin_lock);
	tsc_devp = kmalloc(sizeof(struct tsc_dev), GFP_KERNEL);
	if (tsc_devp == NULL) {
		pr_err("malloc mem for tsc device err\n");
		return -ENOMEM;
	}
	memset(tsc_devp, 0, sizeof(struct tsc_dev));


	tsc_devp->irq = irq_of_parse_and_map(node, 0);
	pr_info("irq_of_parse_and_map get irq is %d\n", tsc_devp->irq);
	if (tsc_devp->irq <= 0)
		pr_err("Can't parse IRQ");

	sema_init(&tsc_devp->sem, 1);
	init_waitqueue_head(&tsc_devp->wq);

	memset(&tsc_devp->iomap_addrs, 0, sizeof(struct iomap_para));

	ret = request_irq(SUNXI_IRQ_TS, tscdriverinterrupt, 0, "ts0", NULL);
	if (ret < 0) {
		pr_err("request irq err\n");
		return -EINVAL;
	}

	/* map for macc io space */
	tsc_devp->iomap_addrs.regs_macc = of_iomap(node, 0);
	if (!tsc_devp->iomap_addrs.regs_macc)
		pr_err("ve Can't map registers");
	pr_info("of_iomap get tsc regs_macc is %p\n",
			tsc_devp->iomap_addrs.regs_macc);

	tsc_devp->tsc_parent_pll_clk = of_clk_get(node, 0);

	if ((!tsc_devp->tsc_parent_pll_clk) ||
			IS_ERR(tsc_devp->tsc_parent_pll_clk)) {
		pr_err("try to get tsc_parent_pll_clk fail\n");
		return -EINVAL;
	}

	tsc_devp->tsc_clk = of_clk_get(node, 1);

	if (!tsc_devp->tsc_clk || IS_ERR(tsc_devp->tsc_clk))
		pr_err("get tsc_clk failed;\n");

	/* no reset tsc module */
	sunxi_periph_reset_assert(tsc_devp->tsc_clk);

	clk_set_parent(tsc_devp->tsc_clk, tsc_devp->tsc_parent_pll_clk);
	/*set ts clock rate*/
	clk_rate = clk_get_rate(tsc_devp->tsc_parent_pll_clk);
	pr_info("%s: parent clock rate %ld\n", __func__, clk_rate);

	clk_rate /= 2;
	pr_info("%s: clock rate %ld\n", __func__, clk_rate);

	if (clk_set_rate(tsc_devp->tsc_clk, clk_rate) < 0) {
		pr_err("%s: set clk rate error\n", __func__);
		return -EINVAL;
	}

	clk_rate = clk_get_rate(tsc_devp->tsc_clk);
	pr_info("%s: clock rate %ld\n", __func__, clk_rate);

	clk_prepare_enable(tsc_devp->tsc_clk);

	/* Create char device */
	devno = MKDEV(tsc_dev_major, tsc_dev_minor);
	cdev_init(&tsc_devp->cdev, &tscdev_fops);
	tsc_devp->cdev.owner = THIS_MODULE;

	ret = cdev_add(&tsc_devp->cdev, devno, 1);
	if (ret) {
		pr_err(KERN_NOTICE "Err:%d add tscdev", ret);
	}
	tsc_devp->class = class_create(THIS_MODULE, "ts0");
	tsc_devp->dev = device_create(tsc_devp->class,
			NULL, devno, NULL, "ts0");

	if (request_tsc_pio(pdev)) {
		pr_err("%s: request tsc pio failed\n", __func__);
		return -EINVAL;
	}

	pr_info("[tsc]: install end!\n");
	return 0;


}

static void  tscdev_exit(void)
{
#if 0
	dev_t dev;

	if (tsc_used != 1) {
		pr_err("%s: tsc driver is disabled\n", __func__);
		return;
	}

	if (tsc_devp == NULL) {
		pr_err("%s: invalid tsc_devp\n", __func__);
		return;
	}

	/* unregister iomem and iounmap */
	tsiomem_unregister(tsc_devp);

	dev = MKDEV(tsc_devp->ts_dev_major, tsc_devp->ts_dev_minor);
	device_destroy(tsc_devp->class, dev);
	class_destroy(tsc_devp->class);
	cdev_del(&tsc_devp->cdev);

	/* release ts irq */
	free_irq(TS_IRQ_NO, NULL);

	/* release ts pin */
	release_tsc_pio();

	unregister_chrdev_region(dev, 1);
	pr_info("tscdev_exit\n");
	kfree(tsc_devp);
#endif
	dev_t dev;
	dev = MKDEV(tsc_dev_major, tsc_dev_minor);

	free_irq(SUNXI_IRQ_TS, NULL);
	iounmap(tsc_devp->iomap_addrs.regs_macc);

	/* Destroy char device */
	if (tsc_devp) {
		cdev_del(&tsc_devp->cdev);
		device_destroy(tsc_devp->class, dev);
		class_destroy(tsc_devp->class);
	}

	if (NULL == tsc_devp->tsc_clk || IS_ERR(tsc_devp->tsc_clk)) {
		pr_err("tsc_clk handle is invalid, just return!\n");
	} else {
		clk_disable_unprepare(tsc_devp->tsc_clk);
		clk_put(tsc_devp->tsc_clk);
		tsc_devp->tsc_clk = NULL;
	}


	if (NULL == tsc_devp->tsc_parent_pll_clk ||
			IS_ERR(tsc_devp->tsc_parent_pll_clk)) {
		pr_err("tsc_parent_pll_clk handle is invalid, just return!\n");
	} else {
		clk_put(tsc_devp->tsc_parent_pll_clk);
	}

	/* release ts pin */
	release_tsc_pio();
	unregister_chrdev_region(dev, 1);

	if (tsc_devp) {
		kfree(tsc_devp);
	}
}


#ifdef CONFIG_PM
static int snd_sw_tsc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;

	pr_info("[tsc] standby suspend\n");
	ret = disable_tsc_hw_clk();

	if (ret < 0) {
		pr_err("Warring: tsc clk disable somewhere error!\n");
		return -EFAULT;
	}

	return 0;

}

static int snd_sw_tsc_resume(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[tsc] standby resume\n");

	if (tsc_devp->ref_count == 0)
		return 0;

	ret = enable_tsc_hw_clk();
	if (ret < 0) {
		pr_err("Warring: tsc clk enable somewhere error!\n");
		return -EFAULT;
	}
	return 0;
}

#endif

/*static int __devexit sunxi_tsc_remove(struct platform_device *pdev)*/
static int sunxi_tsc_remove(struct platform_device *pdev)

{
	tscdev_exit();
	return 0;
}

/*static int __devinit sunxi_tsc_probe(struct platform_device *pdev)*/
static int  sunxi_tsc_probe(struct platform_device *pdev)

{
	tscdev_init(pdev);
	return 0;
}


static struct platform_driver sunxi_tsc_driver = {
	.probe		= sunxi_tsc_probe,
	.remove		= sunxi_tsc_remove,
#ifdef CONFIG_PM
	.suspend	= snd_sw_tsc_suspend,
	.resume		= snd_sw_tsc_resume,
#endif
	.driver		= {
		.name	= "ts0",
		.owner	= THIS_MODULE,
		.of_match_table = sun50i_tsc_match,
	},
};

static int __init sunxi_tsc_init(void)

{
	pr_info("sunxi tsc init begin\n");
	tsc_devp = NULL;
	node = NULL;
	clk_status = 0;
	return platform_driver_register(&sunxi_tsc_driver);
}

static void __exit sunxi_tsc_exit(void)
{
	pr_info("sunxi tsc exit\n");
	platform_driver_unregister(&sunxi_tsc_driver);
}

module_init(sunxi_tsc_init);
module_exit(sunxi_tsc_exit);


MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("User mode tsc device interface");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:tsc-sunxi");
