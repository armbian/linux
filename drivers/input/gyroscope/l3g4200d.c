/* drivers/i2c/chips/l3g4200d.c - l3g4200d compass driver
 *
 * Copyright (C) 2007-2008 HTC Corporation.
 * Author: Hou-Kun Chen <houkun.chen@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/l3g4200d.h>
#include <mach/gpio.h>
#include <mach/board.h> 
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if 0
#define DBG(x...) printk(x)
#else
#define DBG(x...)  
#endif
static int  l3g4200d_probe(struct i2c_client *client, const struct i2c_device_id *id);

#define L3G4200D_SPEED		200 * 1000
#define L3G4200D_DEVID		0xD3

#define L3G4200D_MAJOR   102
#define L3G4200D_MINOR   4

/* l3g4200d gyroscope registers */
#define WHO_AM_I    0x0F

#define CTRL_REG1       0x20    /* power control reg */
#define CTRL_REG2       0x21    /* power control reg */
#define CTRL_REG3       0x22    /* power control reg */
#define CTRL_REG4       0x23    /* interrupt control reg */
#define CTRL_REG5       0x24    /* interrupt control reg */
#define AXISDATA_REG    0x28


/* Addresses to scan -- protected by sense_data_mutex */
//static char sense_data[RBUFF_SIZE + 1];
static struct i2c_client *this_client;
static struct l3g4200d_data *this_data;
static struct miscdevice l3g4200d_device;

static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend l3g4200d_early_suspend;
#endif
static int revision = -1;
/* AKM HW info */
static ssize_t gsensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%#x\n", revision);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(vendor, 0444, gsensor_vendor_show, NULL);

static struct kobject *android_gsensor_kobj;

static int gsensor_sysfs_init(void)
{
	int ret ;

	android_gsensor_kobj = kobject_create_and_add("android_gyrosensor", NULL);
	if (android_gsensor_kobj == NULL) {
		printk(KERN_ERR
		       "L3G4200D gsensor_sysfs_init:"\
		       "subsystem_register failed\n"); 
		ret = -ENOMEM;
		goto err;
	}

	ret = sysfs_create_file(android_gsensor_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR
		       "L3G4200D gsensor_sysfs_init:"\
		       "sysfs_create_group failed\n");
		goto err4;
	}

	return 0 ;
err4:
	kobject_del(android_gsensor_kobj);
err:
	return ret ;
}

#if 0	
static int l3g4200d_rx_data(struct i2c_client *client, char *rxData, int length)
{
	int ret = 0;
	char reg = rxData[0];
	ret = i2c_master_reg8_recv(client, reg, rxData, length, L3G4200D_SPEED);
	return (ret > 0)? 0 : ret;
}
#else
static int l3g4200d_rx_data(struct i2c_client *client, char *rxData, int length)
{
	int ret = 0;
	char reg = rxData[0];
	int i = 0;
	for(i=0; i<3; i++)
	{
		ret = i2c_master_reg8_recv(client, reg, rxData, length, L3G4200D_SPEED);
		if(ret < 0)
		mdelay(1);
		else
		break;
	}
	return (ret > 0)? 0 : ret;
}
#endif
#if 0	
static int l3g4200d_tx_data(struct i2c_client *client, char *txData, int length)
{
	int ret = 0;
	char reg = txData[0];
	ret = i2c_master_reg8_send(client, reg, &txData[1], length-1, L3G4200D_SPEED);
	return (ret > 0)? 0 : ret;
}
#else
static int l3g4200d_tx_data(struct i2c_client *client, char *txData, int length)
{
	int ret = 0;
	char reg = txData[0];
	int i = 0;
	for(i=0; i<3; i++)
	{
		ret = i2c_master_reg8_send(client, reg, &txData[1], length-1, L3G4200D_SPEED);
		if(ret < 0)
		mdelay(1);
		else
		break;
	}
	return (ret > 0)? 0 : ret;
}
#endif

#if 0
static int gyro_rx_data(struct i2c_client *client, char *rxData, int length)
{
	int ret = 0;
	int i = 0;
	char reg = rxData[0];
	for(i=0; i<3; i++)
	{
		ret = i2c_master_reg8_recv(client, reg, rxData, length, L3G4200D_SPEED);
		if(ret < 0)
		mdelay(1);
		else
		break;
	}
	return (ret > 0)? 0 : ret;
}
#endif

static int gyro_tx_data(struct i2c_client *client, char *txData, int length)
{
	int ret = 0;
	char reg = txData[0];
	int i = 0;
	
	for(i=0; i<3; i++)
	{
		ret = i2c_master_reg8_send(client, reg, &txData[1], length-1, L3G4200D_SPEED);
		if(ret < 0)
		mdelay(1);
		else
		break;
	}
	return (ret > 0)? 0 : ret;
}

#if 0
/*  i2c read routine for l3g4200d digital gyroscope */
static char l3g4200d_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len)
{
	char tmp;
	int ret = 0;
	if (this_client == NULL)  /*  No global client pointer? */
		return -1;

	data[0]=reg_addr;
	ret = gyro_rx_data(this_client, data, len);
	return tmp;
}
#endif
/*  i2c write routine for l3g4200d digital gyroscope */
static char l3g4200d_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len)
{

	char buffer[3];
	int ret = 0;
	int i = 0;
	if (this_client == NULL)  /*  No global client pointer? */
		return -1;
	for (i = 0; i < len; i++)
		{
		buffer[0] = reg_addr+i;
		buffer[1] = data[i];
		ret = gyro_tx_data(this_client, &buffer[0], 2);
		}
	return ret;

}

static char l3g4200d_read_reg(struct i2c_client *client,int addr)
{
	char tmp;
	int ret = 0;

	tmp = addr;
//	ret = l3g4200d_tx_data(client, &tmp, 1);
	ret = l3g4200d_rx_data(client, &tmp, 1);
	return tmp;
}

static int l3g4200d_write_reg(struct i2c_client *client,int addr,int value)
{
	char buffer[3];
	int ret = 0;

	buffer[0] = addr;
	buffer[1] = value;
	ret = l3g4200d_tx_data(client, &buffer[0], 2);
	return ret;
}


static char l3g4200d_get_devid(struct i2c_client *client)
{
	unsigned int devid = 0;
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	
	devid = l3g4200d_read_reg(client, WHO_AM_I)&0xff;
	if (devid == GYRO_DEVID_L3G4200D) {
		l3g4200d->devid = devid;
		printk(KERN_INFO "gyro is L3G4200D and devid=0x%x\n",devid);
		return 0;
	} else if (devid == GYRO_DEVID_L3G20D)
	{
		l3g4200d->devid = devid;
		printk(KERN_INFO "gyro is L3G20D and devid=0x%x\n",devid);
		return 0;
	}
	else
	{
		printk(KERN_ERR "%s:gyro device id is error,devid=%d\n",__func__,devid);
		return -1;
	}
}

static int l3g4200d_active(struct i2c_client *client,int enable)
{
	int tmp;
	int ret = 0;
	
	tmp = l3g4200d_read_reg(client,CTRL_REG1);
	if(enable)
		tmp |=ACTIVE_MASK;
	else
		tmp &=~ACTIVE_MASK;
	DBG("l3g4200d_active %s (0x%x)\n",enable?"active":"standby",tmp);	
	ret = l3g4200d_write_reg(client,CTRL_REG1,tmp);
	return ret;
}



static int device_init(void)
{	
	int res;	
	unsigned char buf[5];	
	buf[0] = 0x07;	//27
	buf[1] = 0x00;	
	buf[2] = 0x00;	
	buf[3] = 0x20;	//0x00
	buf[4] = 0x00;	
	res = l3g4200d_i2c_write(CTRL_REG1, &buf[0], 5);	
	return res;
}

int l3g4200d_set_bandwidth(char bw)
{
	int res = 0;
	unsigned char data;

	res = l3g4200d_read_reg(this_client, CTRL_REG1);
	if (res >= 0)
		data = res & 0x000F;

	data = data + bw;
	res = l3g4200d_i2c_write(CTRL_REG1, &data, 1);
	return res;
}

static int l3g4200d_start_dev(struct i2c_client *client, char rate)
{
	int ret = 0;
	//int tmp;
	//struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);

	/* standby */
	l3g4200d_active(client,0);
	device_init();
	l3g4200d_set_bandwidth(rate);
	l3g4200d_active(client,1);
	
	enable_irq(client->irq);
	return ret;

}

static int l3g4200d_start(struct i2c_client *client, char rate)
{ 
    struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
    
    DBG("%s::enter\n",__FUNCTION__); 
    if (l3g4200d->status == L3G4200D_OPEN) {
        return 0;      
    }
    l3g4200d->status = L3G4200D_OPEN;
    return l3g4200d_start_dev(client, rate);
}

static int l3g4200d_close_dev(struct i2c_client *client)
{    	
	DBG("%s :enter\n",__FUNCTION__);	
	disable_irq_nosync(client->irq);
	return l3g4200d_active(client,0);
}

static int l3g4200d_close(struct i2c_client *client)
{
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	DBG("%s::enter\n",__FUNCTION__); 
	l3g4200d->status = L3G4200D_CLOSE;

	return l3g4200d_close_dev(client);
}

static int l3g4200d_reset_rate(struct i2c_client *client, char rate)
{
	int ret = 0;
	
	DBG("%s\n",__func__);
	
 //   ret = l3g4200d_close_dev(client);
    ret = l3g4200d_start_dev(client, rate);
  
	return ret ;
}

static inline int l3g4200d_convert_to_int(char value)
{
    int result;

    if (value < 1) {
       result = value * 1;
    } else {
       result = ~(((~value & 0x7f) + 1)* 1) + 1;
    }

    return result;
}

static void l3g4200d_report_value(struct i2c_client *client, struct l3g4200d_axis *axis)
{
	struct l3g4200d_data *l3g4200d = i2c_get_clientdata(client);
	//struct l3g4200d_axis *axis = (struct l3g4200d_axis *)rbuf;

	/* Report acceleration sensor information */
	input_report_rel(l3g4200d->input_dev, ABS_RX, axis->x);
	input_report_rel(l3g4200d->input_dev, ABS_RY, axis->y);
	input_report_rel(l3g4200d->input_dev, ABS_RZ, axis->z);
	input_sync(l3g4200d->input_dev);
	DBG("%s:x==%d  y==%d z==%d\n",__func__,axis->x,axis->y,axis->z);
}


static int l3g4200d_get_data(struct i2c_client *client)
{
	//char buffer[6];
	int ret,i;
	struct l3g4200d_axis axis;
	struct l3g4200d_platform_data *pdata = client->dev.platform_data;	
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	//int res;
	unsigned char gyro_data[6];
	/* x,y,z hardware data */
	int x = 0, y = 0, z = 0;

	for(i=0;i<6;i++)
	{
		gyro_data[i] = AXISDATA_REG+i;
		//ret = l3g4200d_tx_data(client, &buffer[0], 1);
		ret = l3g4200d_rx_data(client, &gyro_data[i], 1);
	}

	x = (short) (((gyro_data[1]) << 8) | gyro_data[0]);
	y = (short) (((gyro_data[3]) << 8) | gyro_data[2]);
	z = (short) (((gyro_data[5]) << 8) | gyro_data[4]);

	DBG("%s: x=%d  y=%d z=%d \n",__func__, x,y,z);
	if(pdata && pdata->orientation)
	{
		axis.x = (pdata->orientation[0])*x + (pdata->orientation[1])*y + (pdata->orientation[2])*z;
		axis.y = (pdata->orientation[3])*x + (pdata->orientation[4])*y + (pdata->orientation[5])*z;	
		axis.z = (pdata->orientation[6])*x + (pdata->orientation[7])*y + (pdata->orientation[8])*z;
	}
	else
	{
		axis.x = x;	
		axis.y = y;
		axis.z = z;	
	}

	//filter gyro data
	if((abs(l3g4200d->axis.x - axis.x) > pdata->x_min)||(abs(l3g4200d->axis.y - axis.y) > pdata->y_min)||(abs(l3g4200d->axis.z - axis.z) > pdata->z_min))
	{
		l3g4200d->axis.x = axis.x;
		l3g4200d->axis.y = axis.y;
		l3g4200d->axis.z = axis.z;	
		if(abs(l3g4200d->axis.x) <= pdata->x_min) l3g4200d->axis.x = 0;	
		if(abs(l3g4200d->axis.y) <= pdata->y_min) l3g4200d->axis.y = 0;
		if(abs(l3g4200d->axis.z) <= pdata->z_min) l3g4200d->axis.z = 0;
		
		l3g4200d_report_value(client, &l3g4200d->axis);
	}
	
	return 0;
}

/*
static int l3g4200d_trans_buff(char *rbuf, int size)
{
	//wait_event_interruptible_timeout(data_ready_wq,
	//				 atomic_read(&data_ready), 1000);
	wait_event_interruptible(data_ready_wq,
					 atomic_read(&data_ready));

	atomic_set(&data_ready, 0);
	memcpy(rbuf, &sense_data[0], size);

	return 0;
}
*/

static int l3g4200d_open(struct inode *inode, struct file *file)
{
	DBG("%s :enter\n",__FUNCTION__);	
	return 0;//nonseekable_open(inode, file);
}

static int l3g4200d_release(struct inode *inode, struct file *file)
{
	DBG("%s :enter\n",__FUNCTION__);	
	return 0;
}
#define RBUFF_SIZE		12	/* Rx buffer size */

static long l3g4200d_ioctl( struct file *file, unsigned int cmd,unsigned long arg)
{

	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(this_client);
	void __user *argp = (void __user *)arg;
	//char msg[RBUFF_SIZE + 1];
	int ret = -1;
	char rate;
	struct i2c_client *client = container_of(l3g4200d_device.parent, struct i2c_client, dev);
	
	switch (cmd) {
	case L3G4200D_IOCTL_GET_ENABLE:	
		DBG("%s :L3G4200D_IOCTL_GET_ENABLE\n",__FUNCTION__);	
		ret=!l3g4200d->status;
		if (copy_to_user(argp, &ret, sizeof(ret)))
		{
            printk("%s:failed to copy status to user space.\n",__FUNCTION__);
			return -EFAULT;
		}
		break;
	case L3G4200D_IOCTL_SET_ENABLE:			
		DBG("%s :L3G4200D_IOCTL_SET_ENABLE,flag=%d\n",__FUNCTION__,*(unsigned int *)argp);
		if(*(unsigned int *)argp)
		{
			ret = l3g4200d_start(client, ODR100_BW12_5);
			if (ret < 0)
			return ret;
		}
		else
		{
			ret = l3g4200d_close(client);
			if (ret < 0)
			return ret;
		}
		ret=l3g4200d->status;
		if (copy_to_user(argp, &ret, sizeof(ret)))
		{
            printk("%s:failed to copy sense data to user space.\n",__FUNCTION__);
			return -EFAULT;
		}
		break;
	case L3G4200D_IOCTL_SET_DELAY:		
		DBG("%s :L3G4200D_IOCTL_SET_DELAY,rate=%d\n",__FUNCTION__,rate);
		if (copy_from_user(&rate, argp, sizeof(rate)))
		return -EFAULT;
		ret = l3g4200d_reset_rate(client, 0x00);//rate<<4);//0x20		
		if (ret < 0)
			return ret;
		break;
/*
	case ECS_IOCTL_GETDATA:
		ret = l3g4200d_trans_buff(msg, RBUFF_SIZE);
		if (ret < 0)
			return ret;
		if (copy_to_user(argp, &msg, sizeof(msg)))
			return -EFAULT;
		break;
*/	
	default:
		printk("%s:error,cmd=0x%x\n",__func__,cmd);
		return -ENOTTY;
	}
	
	DBG("%s:line=%d,cmd=0x%x\n",__func__,__LINE__,cmd);
	return 0;
}

static void l3g4200d_work_func(struct work_struct *work)
{
	struct l3g4200d_data *l3g4200d = container_of(work, struct l3g4200d_data, work);
	struct i2c_client *client = l3g4200d->client;
	
	if (l3g4200d_get_data(client) < 0) 
		DBG(KERN_ERR "L3G4200D mma_work_func: Get data failed\n");
		
	enable_irq(client->irq);		
}

static void  l3g4200d_delaywork_func(struct work_struct *work)
{
	struct delayed_work *delaywork = container_of(work, struct delayed_work, work);
	struct l3g4200d_data *l3g4200d = container_of(delaywork, struct l3g4200d_data, delaywork);
	struct i2c_client *client = l3g4200d->client;

	if (l3g4200d_get_data(client) < 0) 
		DBG(KERN_ERR "L3G4200D mma_work_func: Get data failed\n");
	enable_irq(client->irq);		
}

static irqreturn_t l3g4200d_interrupt(int irq, void *dev_id)
{
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)dev_id;
	
	disable_irq_nosync(irq);
	schedule_delayed_work(&l3g4200d->delaywork, msecs_to_jiffies(10));
	DBG("%s :enter\n",__FUNCTION__);	
	return IRQ_HANDLED;
}

static struct file_operations l3g4200d_fops = {
	.owner = THIS_MODULE,
	.open = l3g4200d_open,
	.release = l3g4200d_release,
	.unlocked_ioctl = l3g4200d_ioctl,
};

static struct miscdevice l3g4200d_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyrosensor",//"l3g4200d_daemon",
	.fops = &l3g4200d_fops,
};

static int l3g4200d_remove(struct i2c_client *client)
{
	struct l3g4200d_data *l3g4200d = i2c_get_clientdata(client);
	
    misc_deregister(&l3g4200d_device);
    input_unregister_device(l3g4200d->input_dev);
    input_free_device(l3g4200d->input_dev);
    free_irq(client->irq, l3g4200d);
    kfree(l3g4200d); 
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&l3g4200d_early_suspend);
#endif      
    this_client = NULL;
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void l3g4200d_suspend(struct early_suspend *h)
{
	struct i2c_client *client = container_of(l3g4200d_device.parent, struct i2c_client, dev);
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	if(l3g4200d->status == L3G4200D_OPEN)
	{
		l3g4200d_close_dev(client);
	}
	
	DBG("%s:%d\n",__func__,l3g4200d->status);
}

static void l3g4200d_resume(struct early_suspend *h)
{
	struct i2c_client *client = container_of(l3g4200d_device.parent, struct i2c_client, dev);
	struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	if (l3g4200d->status == L3G4200D_OPEN)
		l3g4200d_start_dev(client,l3g4200d->curr_tate);
	
	DBG("%s:%d\n",__func__,l3g4200d->status);
}
#else
static int l3g4200d_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret=0;
	//DBG("Gsensor mma7760 enter 2 level  suspend l3g4200d->status %d\n",l3g4200d->status);
	//struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	//if(l3g4200d->status == L3G4200D_OPEN)
	//{
	//l3g4200d->status = L3G4200D_SUSPEND;
	//ret = l3g4200d_close_dev(client);
	//}
	return ret;
}
static int l3g4200d_resume(struct i2c_client *client)
{
	int ret=0;
	//struct l3g4200d_data *l3g4200d = (struct l3g4200d_data *)i2c_get_clientdata(client);
	//DBG("Gsensor mma7760 2 level resume!! l3g4200d->status %d\n",l3g4200d->status);
	//if((l3g4200d->status == L3G4200D_SUSPEND) && (l3g4200d->status != L3G4200D_OPEN))
	//if (l3g4200d->status == L3G4200D_OPEN)
	//ret = l3g4200d_start_dev(client, l3g4200d->curr_tate);
	return ret;
}
#endif

static const struct i2c_device_id l3g4200d_id[] = {
		{"l3g4200d_gryo", 0},
		{ }
};

static struct i2c_driver l3g4200d_driver = {
	.driver = {
		.name = "l3g4200d_gryo",
	    },
	.id_table 	= l3g4200d_id,
	.probe		= l3g4200d_probe,
	.remove		= __devexit_p(l3g4200d_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND	
	.suspend = &l3g4200d_suspend,
	.resume = &l3g4200d_resume,
#endif	
};


static int l3g4200d_init_irq(struct i2c_client *client)
{
	struct l3g4200d_data *l3g4200d;
	int ret;
	l3g4200d = i2c_get_clientdata(client);
	DBG("gpio_to_irq(%d) is %d\n",client->irq,gpio_to_irq(client->irq));
	if ( !gpio_is_valid(client->irq)) {
		DBG("+++++++++++gpio_is_invalid\n");
		return -EINVAL;
	}
	ret = gpio_request(client->irq, "l3g4200d_int");
	if (ret) {
		DBG( "failed to request mma7990_trig GPIO%d\n",gpio_to_irq(client->irq));
		return ret;
	}
	ret = gpio_direction_input(client->irq);
	if (ret) {
	DBG("failed to set mma7990_trig GPIO gpio input\n");
		return ret;
	}
	gpio_pull_updown(client->irq, GPIOPullUp);
	client->irq = gpio_to_irq(client->irq);
	ret = request_irq(client->irq, l3g4200d_interrupt, IRQF_TRIGGER_LOW, client->dev.driver->name, l3g4200d);
	DBG("request irq is %d,ret is  0x%x\n",client->irq,ret);
	if (ret ) {
		DBG(KERN_ERR "l3g4200d_init_irq: request irq failed,ret is %d\n",ret);
        return ret;
	}
	disable_irq(client->irq);
	init_waitqueue_head(&data_ready_wq);
 
	return 0;
}


static int l3g4200d_validate_pdata(struct l3g4200d_data *gyro)
{
	if (gyro->pdata->axis_map_x > 2 ||
	    gyro->pdata->axis_map_y > 2 ||
	    gyro->pdata->axis_map_z > 2) {
		dev_err(&gyro->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			gyro->pdata->axis_map_x, gyro->pdata->axis_map_y,
			gyro->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (gyro->pdata->negate_x > 1 ||
	    gyro->pdata->negate_y > 1 ||
	    gyro->pdata->negate_z > 1) {
		dev_err(&gyro->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			gyro->pdata->negate_x, gyro->pdata->negate_y,
			gyro->pdata->negate_z);
		return -EINVAL;
	}

	return 0;
}




static int  l3g4200d_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct l3g4200d_data *l3g4200d;
	struct l3g4200d_platform_data *pdata = pdata = client->dev.platform_data;
	int i,err;
	
	if(pdata && pdata->init)
		pdata->init();
	
	l3g4200d = kzalloc(sizeof(struct l3g4200d_data), GFP_KERNEL);
	if (!l3g4200d) {
		DBG("[l3g4200d]:alloc data failed.\n");
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}
    
	INIT_WORK(&l3g4200d->work, l3g4200d_work_func);
	INIT_DELAYED_WORK(&l3g4200d->delaywork, l3g4200d_delaywork_func);

	l3g4200d->client = client;
	i2c_set_clientdata(client, l3g4200d);

	this_client = client;

	l3g4200d->pdata = kmalloc(sizeof(*l3g4200d->pdata), GFP_KERNEL);

	if (l3g4200d->pdata == NULL)
		goto exit_kfree;

	memcpy(l3g4200d->pdata, client->dev.platform_data, sizeof(*l3g4200d->pdata));

	err = l3g4200d_validate_pdata(l3g4200d);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}
	this_data=l3g4200d;
	
	//try three times
	for(i=0; i<3; i++)
	{
		err = l3g4200d_get_devid(client);
		if (!err)
		break;
	}
	if(err)
	{
		printk("%s:fail\n",__func__);
		goto exit_kfree_pdata;
	}

	
	err = l3g4200d_init_irq(client);
	if (err < 0) {
		DBG(KERN_ERR
		       "l3g4200d_probe: l3g4200d_init_irq failed\n");
		goto exit_request_gpio_irq_failed;
	}
		
	l3g4200d->input_dev = input_allocate_device();
	if (!l3g4200d->input_dev) {
		err = -ENOMEM;
		DBG(KERN_ERR
		       "l3g4200d_probe: Failed to allocate input device\n");
		goto exit_input_allocate_device_failed;
	}

	//set_bit(EV_ABS, l3g4200d->input_dev->evbit);

	/* x-axis acceleration */
	input_set_capability(l3g4200d->input_dev, EV_REL, REL_RX);
	input_set_abs_params(l3g4200d->input_dev, ABS_RX, -32768, 32768, 0, 0); //2g full scale range
	/* y-axis acceleration */	
	input_set_capability(l3g4200d->input_dev, EV_REL, REL_RY);
	input_set_abs_params(l3g4200d->input_dev, ABS_RY, -32768, 32768, 0, 0); //2g full scale range
	/* z-axis acceleration */
	input_set_capability(l3g4200d->input_dev, EV_REL, REL_RZ);
	input_set_abs_params(l3g4200d->input_dev, ABS_RZ, -32768, 32768, 0, 0); //2g full scale range

	l3g4200d->input_dev->name = "gyro";
	l3g4200d->input_dev->dev.parent = &client->dev;
	l3g4200d->axis.x = 0;	
	l3g4200d->axis.y = 0;	
	l3g4200d->axis.z = 0;
	
	err = input_register_device(l3g4200d->input_dev);
	if (err < 0) {
		DBG(KERN_ERR
		       "l3g4200d_probe: Unable to register input device: %s\n",
		       l3g4200d->input_dev->name);
		goto exit_input_register_device_failed;
	}

	l3g4200d_device.parent = &client->dev;
	err = misc_register(&l3g4200d_device);
	if (err < 0) {
		DBG(KERN_ERR
		       "l3g4200d_probe: mmad_device register failed\n");
		goto exit_misc_device_register_l3g4200d_device_failed;
	}

	err = gsensor_sysfs_init();
	if (err < 0) {
		DBG(KERN_ERR
            "l3g4200d_probe: gsensor sysfs init failed\n");
		goto exit_gsensor_sysfs_init_failed;
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	l3g4200d_early_suspend.suspend = l3g4200d_suspend;
	l3g4200d_early_suspend.resume = l3g4200d_resume;
	l3g4200d_early_suspend.level = 0x2;
	register_early_suspend(&l3g4200d_early_suspend);
#endif

	l3g4200d->status = L3G4200D_CLOSE;
#if  0	
//	l3g4200d_start_test(this_client);
	l3g4200d_start(client, L3G4200D_RATE_12P5);
#endif
	
	return 0;

exit_gsensor_sysfs_init_failed:
    misc_deregister(&l3g4200d_device);
exit_misc_device_register_l3g4200d_device_failed:
    input_unregister_device(l3g4200d->input_dev);
exit_input_register_device_failed:
	input_free_device(l3g4200d->input_dev);
exit_input_allocate_device_failed:
    free_irq(client->irq, l3g4200d);
exit_kfree_pdata:
exit_request_gpio_irq_failed:
	kfree(l3g4200d->pdata);
exit_kfree:
	kfree(l3g4200d);	
exit_alloc_data_failed:
	DBG("%s error\n",__FUNCTION__);
	return err;
}


static int __init l3g4200d_i2c_init(void)
{
	return i2c_add_driver(&l3g4200d_driver);
}

static void __exit l3g4200d_i2c_exit(void)
{
	i2c_del_driver(&l3g4200d_driver);
}

module_init(l3g4200d_i2c_init);
module_exit(l3g4200d_i2c_exit);


