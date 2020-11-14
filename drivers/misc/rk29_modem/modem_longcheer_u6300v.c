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
static irqreturn_t u6300v_irq_handler(int irq, void *dev_id);
static int __devinit u6300v_resume(struct platform_device *pdev);

static struct rk29_io_t u6300v_io_ap_ready = {
    .io_addr    = RK29_PIN3_PC2,
    .enable     = GPIO_LOW,
    .disable    = GPIO_HIGH,
};

static struct rk29_io_t u6300v_io_power = {
    .io_addr    = RK29_PIN6_PB1,
    .enable     = GPIO_HIGH,
    .disable    = GPIO_LOW,
};

static struct rk29_irq_t u6300v_irq_bp_wakeup_ap= {
    .irq_addr   = RK29_PIN3_PD7,
    .irq_trigger = IRQF_TRIGGER_FALLING, // �½��ش���
};

static struct platform_driver u6300v_platform_driver = {
	.driver		= {
		.name		= "longcheer_u6300v",
	},
	.suspend    = rk29_modem_suspend,
	.resume     = rk29_modem_resume,
};

static struct rk29_modem_t u6300v_driver = {
    .driver         = &u6300v_platform_driver,
    .modem_power    = &u6300v_io_power,
    .ap_ready       = &u6300v_io_ap_ready,
    .bp_wakeup_ap   = &u6300v_irq_bp_wakeup_ap,
    .status         = MODEM_ENABLE,
    .dev_init       = NULL,
    .dev_uninit     = NULL,
    .irq_handler    = u6300v_irq_handler,
    .suspend        = NULL,
    .resume         = u6300v_resume,
    
    .enable         = NULL,
    .disable        = NULL,
    .sleep          = NULL,
    .wakeup         = NULL,
};

static void do_test1(struct work_struct *work)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
    // ��־AP�Ѿ�����BB�����ϱ����ݸ�AP
    gpio_direction_output(u6300v_driver.ap_ready->io_addr, u6300v_driver.ap_ready->enable);
}

static DECLARE_DELAYED_WORK(test1, do_test1);

static int __devinit u6300v_resume(struct platform_device *pdev)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);

/* cmy: Ŀǰ��ϵͳ�����Ѻ����������AP_RDY��������ͨ���������������豸����(����USB���ߴ���)
        ��Ҫ��ʱ����AP_RDY�ź�
        ���õ��������������������豸������(����)��������AP_RDY����������:
            1 ������AP_RDY�ĺ���ע�뵽Ŀ���豸��resume������
            2 ��rk29_modem_resume�У��ȴ�Ŀ���豸resume֮��������AP_RDY
 */
    schedule_delayed_work(&test1, 2*HZ);

    return 0;
}

/*
    u6300v ģ��� IRQ ���������ú�����rk29_modem�е�IRQ����������
 */
static irqreturn_t u6300v_irq_handler(int irq, void *dev_id)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);

    if( irq == gpio_to_irq(u6300v_driver.bp_wakeup_ap->irq_addr) )
    {
        if( !do_wakeup_handle )
        {
            do_wakeup_handle = 1;
            // �����յ� bb wakeup ap ��IRQ������һ��8���suspend����ʱ�䵽���Զ��ͷ�
            // �ͷ�ʱ���û�������������ͽ��ٴι���.
            wake_lock_timeout(&u6300v_driver.wakelock_bbwakeupap, 8 * HZ);
        } else
            printk("%s: already wakeup\n", __FUNCTION__);
        return IRQ_HANDLED;
    }
    
    return IRQ_NONE;
}

static int __init u6300v_init(void)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);

    return rk29_modem_init(&u6300v_driver);
}

static void __exit u6300v_exit(void)
{
    printk("%s[%d]: %s\n", __FILE__, __LINE__, __FUNCTION__);
    rk29_modem_exit();
}

module_init(u6300v_init);
module_exit(u6300v_exit);

MODULE_AUTHOR("lintao lintao@rock-chips.com");
MODULE_DESCRIPTION("ROCKCHIP modem driver");
MODULE_LICENSE("GPL");

#if 0
int test(void)
{
    printk(">>>>>> test \n ");
    int ret = gpio_request(IRQ_BB_WAKEUP_AP, NULL);
    if(ret != 0)
    {
        printk(">>>>>> gpio_request failed! \n ");
        gpio_free(IRQ_BB_WAKEUP_AP);
        return ret;
    }

//    printk(">>>>>> set GPIOPullUp \n ");
//    gpio_pull_updown(IRQ_BB_WAKEUP_AP, GPIOPullUp);
//    printk(">>>>>> set GPIO_HIGH \n ");
//    gpio_direction_output(IRQ_BB_WAKEUP_AP, GPIO_HIGH);

//    printk(">>>>>> set GPIO_LOW \n ");
//    gpio_direction_output(IRQ_BB_WAKEUP_AP, GPIO_LOW);
//    msleep(1000);
    
    gpio_free(IRQ_BB_WAKEUP_AP);

    printk(">>>>>> END \n ");
}
#endif

