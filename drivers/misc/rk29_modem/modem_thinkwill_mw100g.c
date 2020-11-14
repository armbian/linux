/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/stat.h>	 /* permission constants */
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <mach/iomux.h>
#include <mach/gpio.h>
#include <linux/delay.h>

#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <mach/board.h>

#include <linux/platform_device.h>

#include "rk29_modem.h"

// ȷ���������ظ�����wakeup
static int do_wakeup_handle = 0;
static irqreturn_t mw100g_irq_handler(int irq, void *dev_id);

static int __devinit mw100g_suspend(struct platform_device *pdev, pm_message_t state)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
    do_wakeup_handle = 0;
    return rk29_modem_suspend(pdev, state);
}

static int __devinit mw100g_resume(struct platform_device *pdev)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
    return rk29_modem_resume(pdev);
}

static struct rk29_io_t mw100g_io_power = {
    .io_addr    = RK29_PIN6_PB1,
    .enable     = GPIO_HIGH,
    .disable    = GPIO_LOW,
};

static struct rk29_irq_t mw100g_irq_bp_wakeup_ap= {
    .irq_addr   = RK29_PIN3_PC4,
    .irq_trigger = IRQF_TRIGGER_FALLING, // �½��ش���
};

static struct platform_driver mw100g_platform_driver = {
	.driver		= {
		.name		= "thinkwill_mw100g",
	},
	.suspend    = mw100g_suspend,
	.resume     = mw100g_resume,
};

static struct rk29_modem_t mw100g_driver = {
    .driver         = &mw100g_platform_driver,
    .modem_power    = &mw100g_io_power,
    .ap_ready       = NULL,
    .bp_wakeup_ap   = &mw100g_irq_bp_wakeup_ap,
    .status         = MODEM_ENABLE,
    .dev_init       = NULL,
    .dev_uninit     = NULL,
    .irq_handler    = mw100g_irq_handler,
    
    .enable         = NULL,
    .disable        = NULL,
    .sleep          = NULL,
    .wakeup         = NULL,
};

static void do_wakeup(struct work_struct *work)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
//    rk28_send_wakeup_key();
}

static DECLARE_DELAYED_WORK(wakeup_work, do_wakeup);

/*
    mw100g ģ��� IRQ ���������ú�����rk29_modem�е�IRQ����������
 */
static irqreturn_t mw100g_irq_handler(int irq, void *dev_id)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);

    if( irq == gpio_to_irq(mw100g_driver.bp_wakeup_ap->irq_addr) )
    {
        if( !do_wakeup_handle )
        {
            do_wakeup_handle = 1;
            // �����յ� bb wakeup ap ��IRQ������һ��8���suspend����ʱ�䵽���Զ��ͷ�
            // �ͷ�ʱ���û�������������ͽ��ٴι���.
            wake_lock_timeout(&mw100g_driver.wakelock_bbwakeupap, 20 * HZ);
            schedule_delayed_work(&wakeup_work, HZ / 10);
        } else
            printk("%s: already wakeup\n", __FUNCTION__);
        return IRQ_HANDLED;
    }
    
    return IRQ_NONE;
}

static int __init mw100g_init(void)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);

    return rk29_modem_init(&mw100g_driver);
}

static void __exit mw100g_exit(void)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
    rk29_modem_exit();
}

module_init(mw100g_init);
module_exit(mw100g_exit);

MODULE_AUTHOR("lintao lintao@rock-chips.com");
MODULE_DESCRIPTION("ROCKCHIP modem driver");
MODULE_LICENSE("GPL");

