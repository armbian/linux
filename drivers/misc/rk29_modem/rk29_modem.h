/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/platform_device.h>
#include <mach/board.h>

/* Modem states */
#define MODEM_DISABLE       0
#define MODEM_ENABLE        1
#define MODEM_SLEEP         2
#define MODEM_WAKEUP        3
#define MODEM_MAX_STATUS    4

struct rk29_irq_t {
    unsigned long irq_addr;
    unsigned long irq_trigger;
};

struct rk29_modem_t {
    struct platform_driver *driver;
    // ����modem��Դ��IO
    struct rk29_io_t *modem_power;
    // ��AP��������δ����ʱ��ͨ�� ap_ready ���IO��֪ͨBP��
    struct rk29_io_t *ap_ready;
    // ��BP���յ����Ż�������ʱ��ͨ�� bp_wakeup_ap ���IRQ������AP
    struct rk29_irq_t *bp_wakeup_ap;
    // ��ǰmodem״̬��Ŀǰֻ�õ�MODEM_ENABLE(�ϵ�)��MODEM_DISABLE(�µ�)
    // ͬʱ��status�ĳ�ʼֵҲ��������ʱ��modem�Ƿ��ϵ�
    int status;
    struct wake_lock wakelock_bbwakeupap;

    // �豸��ʼ������, ��Ҫ���ø���GPIO�Լ�IRQ�������
    int (*dev_init)(struct rk29_modem_t *driver);
    int (*dev_uninit)(struct rk29_modem_t *driver);
    irqreturn_t (*irq_handler)(int irq, void *dev_id);
    int (*suspend)(struct platform_device *pdev, pm_message_t state);
    int (*resume)(struct platform_device *pdev);

    int (*enable)(struct rk29_modem_t *driver);
    int (*disable)(struct rk29_modem_t *driver);
    int (*sleep)(struct rk29_modem_t *driver);
    int (*wakeup)(struct rk29_modem_t *driver);
};

void rk29_modem_exit(void);
int rk29_modem_init(struct rk29_modem_t *rk29_modem);
int rk29_modem_suspend(struct platform_device *pdev, pm_message_t state);
int rk29_modem_resume(struct platform_device *pdev);

