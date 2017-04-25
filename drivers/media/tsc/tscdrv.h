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

#ifndef __TSC_DRV_H__
#define __TSC_DRV_H__

/* version */
#define DRV_VERSION                 "0.01alpha"
/* interrupt number, */
#define SUNXI_IRQ_TS                 (32 + 81)
/* interrupt number */
#define TS_IRQ_NO                   (SUNXI_IRQ_TS)

#ifndef TSCDEV_MAJOR
#define TSCDEV_MAJOR                (225)
#endif

#ifndef TSCDEV_MINOR
#define TSCDEV_MINOR                (0)
#endif

#if 0
struct tsc_dev {
	struct cdev cdev;               /* char device struct */
	struct device *dev;             /* ptr to class device struct */
	struct class *class;            /* class for auto create device node */
	struct semaphore sem;           /* mutual exclusion semaphore */
	spinlock_t lock;                /* spinlock to protect ioclt access */
	wait_queue_head_t  wq;          /* wait queue for poll ops */

	char   name[16];

	struct resource *regs;          /* registers resource */
	char *regsaddr;                /* registers address */

	unsigned int irq;               /* tsc driver irq number */
	unsigned int irq_flag;          /* flag of tsc driver irq generated */

	int     ts_dev_major;
	int     ts_dev_minor;

	struct clk *parent;
	struct clk *tsc_clk;            /* ts  clock */

	struct intrstatus intstatus;    /* save interrupt status */
	int    is_opened;
	struct pinctrl *pinctrl;
};
#endif

struct iomap_para {
	volatile char *regs_macc;
};

struct tsc_dev {
	struct cdev cdev;           /* char device struct */
	struct device *dev;         /* ptr to class device struct      */
	struct class  *class;       /* class for auto create device node */
	struct semaphore sem;       /* mutual exclusion semaphore        */
	spinlock_t lock;            /* spinlock to protect ioclt access */
	wait_queue_head_t wq;       /* wait queue for poll ops        */
	char   name[16];

	struct iomap_para iomap_addrs;   /* io remap addrs   */
	/*struct timer_list cedar_engine_timer;*/
	/*struct timer_list cedar_engine_timer_rel;*/

	unsigned int irq;               /* tsc driver irq number */
	unsigned int irq_flag;          /* flag of tsc driver irq generated */
	unsigned int ref_count;
	struct clk *tsc_parent_pll_clk;
	struct clk *tsc_clk;            /* ts  clock */

	#if 0
	u32 irq;                /* cedar video engine irq number      */
	u32 de_irq_flag;        /* flag of video decoder engine irq generated */
	u32 de_irq_value;       /* value of video decoder engine irq          */
	u32 en_irq_flag;        /* flag of video encoder engine irq generated */
	u32 en_irq_value;       /* value of video encoder engine irq          */
	u32 irq_has_enable;
	u32 ref_count;

	unsigned int *sram_bass_vir;
	unsigned int *clk_bass_vir;
	#endif
	struct intrstatus intstatus;    /* save interrupt status */
	struct pinctrl *pinctrl;
};

#endif
