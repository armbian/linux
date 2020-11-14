/* drivers/regulator/act8891.c
 *
 * Copyright (C) 2011 ROCKCHIP, Inc.
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
/*******************************************************************/
/*	  COPYRIGHT (C)  ROCK-CHIPS FUZHOU . ALL RIGHTS RESERVED.			  */
/*******************************************************************
FILE		:	  act8891.c
DESC		:	  act8891 PMIC driver
AUTHOR		:	  zq  
DATE		:	  2011-09-05
NOTES		:
$LOG: GPIO.C,V $
REVISION 0.01
********************************************************************/


#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/act8891.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <mach/iomux.h>

#if 0
#define DBG(x...)	printk(KERN_INFO x)
#else
#define DBG(x...)
#endif
#if 1
#define DBG_INFO(x...)	printk(KERN_INFO x)
#else
#define DBG_INFO(x...)
#endif
#define PM_CONTROL

struct act8891 {
	struct device *dev;
	struct mutex io_lock;
	struct i2c_client *i2c;
	int num_regulators;
	struct regulator_dev **rdev;
};

static u8 act8891_reg_read(struct act8891 *act8891, u8 reg);
static int act8891_set_bits(struct act8891 *act8891, u8 reg, u16 mask, u16 val);


#define act8891_BUCK1_SET_VOL_BASE 0x20
#define act8891_BUCK2_SET_VOL_BASE 0x30
#define act8891_BUCK3_SET_VOL_BASE 0x40
#define act8891_LDO1_SET_VOL_BASE 0x50
#define act8891_LDO2_SET_VOL_BASE 0x54
#define act8891_LDO3_SET_VOL_BASE 0x60
#define act8891_LDO4_SET_VOL_BASE 0x64

#define act8891_BUCK1_CONTR_BASE 0x22
#define act8891_BUCK2_CONTR_BASE 0x32
#define act8891_BUCK3_CONTR_BASE 0x42
#define act8891_LDO1_CONTR_BASE 0x51
#define act8891_LDO2_CONTR_BASE 0x55
#define act8891_LDO3_CONTR_BASE 0x61
#define act8891_LDO4_CONTR_BASE 0x65

#define BUCK_VOL_MASK 0x3f
#define LDO_VOL_MASK 0x3f

#define VOL_MIN_IDX 0x00
#define VOL_MAX_IDX 0x3f

const static int buck_set_vol_base_addr[] = {
	act8891_BUCK1_SET_VOL_BASE,
	act8891_BUCK2_SET_VOL_BASE,
	act8891_BUCK3_SET_VOL_BASE,
};
const static int buck_contr_base_addr[] = {
	act8891_BUCK1_CONTR_BASE,
 	act8891_BUCK2_CONTR_BASE,
 	act8891_BUCK3_CONTR_BASE,
};
#define act8891_BUCK_SET_VOL_REG(x) (buck_set_vol_base_addr[x])
#define act8891_BUCK_CONTR_REG(x) (buck_contr_base_addr[x])


const static int ldo_set_vol_base_addr[] = {
	act8891_LDO1_SET_VOL_BASE,
	act8891_LDO2_SET_VOL_BASE,
	act8891_LDO3_SET_VOL_BASE,
	act8891_LDO4_SET_VOL_BASE, 
};
const static int ldo_contr_base_addr[] = {
	act8891_LDO1_CONTR_BASE,
	act8891_LDO2_CONTR_BASE,
	act8891_LDO3_CONTR_BASE,
	act8891_LDO4_CONTR_BASE,
};
#define act8891_LDO_SET_VOL_REG(x) (ldo_set_vol_base_addr[x])
#define act8891_LDO_CONTR_REG(x) (ldo_contr_base_addr[x])

const static int buck_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

const static int ldo_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

static int act8891_ldo_list_voltage(struct regulator_dev *dev, unsigned index)
{
	return 1000 * ldo_voltage_map[index];
}
static int act8891_ldo_is_enabled(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8891_LDO1;
	u16 val;
	u16 mask=0x80;
	 val = act8891_reg_read(act8891, act8891_LDO_CONTR_REG(ldo));	 
	 if (val < 0)
		return val;
	 val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8891_ldo_enable(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8891_LDO1;
	u16 mask=0x80;	
	int ret;
	return act8891_set_bits(act8891, act8891_LDO_CONTR_REG(ldo), mask, 0x80);
	
}
static int act8891_ldo_disable(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8891_LDO1;
	u16 mask=0x80;
	int ret;
	return act8891_set_bits(act8891, act8891_LDO_CONTR_REG(ldo), mask, 0);

}
static int act8891_ldo_get_voltage(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8891_LDO1;
	u16 reg = 0;
	int val;
	reg = act8891_reg_read(act8891,act8891_LDO_SET_VOL_REG(ldo));
	reg &= LDO_VOL_MASK;
	val = 1000 * ldo_voltage_map[reg];	
	return val;
}
static int act8891_ldo_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) -ACT8891_LDO1;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map =ldo_voltage_map;
	u16 val;
	int ret = 0;
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX;
	     val++){
		if (vol_map[val] >= min_vol)
			break;	}
		
	if (vol_map[val] > max_vol)
		return -EINVAL;

	ret = act8891_set_bits(act8891, act8891_LDO_SET_VOL_REG(ldo),
	       	LDO_VOL_MASK, val);
	if (ret)
		return ret;

}
static int act8891_ldo_get_mode(struct regulator_dev *dev, unsigned index)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8891_LDO1 ;
	u16 mask = 0xcf;
	u16 val;
	val = act8891_reg_read(act8891, act8891_LDO_CONTR_REG(ldo));
	val=val|mask;
	if (val== mask)
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_STANDBY;

}
static int act8891_ldo_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) -ACT8891_LDO1 ;
	u16 mask = 0x20;
	switch(mode)
	{
	case REGULATOR_MODE_NORMAL:
		return act8891_set_bits(act8891, act8891_LDO_CONTR_REG(ldo), mask, 0);		
	case REGULATOR_MODE_STANDBY:
		return act8891_set_bits(act8891, act8891_LDO_CONTR_REG(ldo), mask, mask);
	default:
		printk("error:pmu_act8891 only lowpower and nomal mode\n");
		return -EINVAL;
	}


}
static struct regulator_ops act8891_ldo_ops = {
	.set_voltage = act8891_ldo_set_voltage,
	.get_voltage = act8891_ldo_get_voltage,
	.list_voltage = act8891_ldo_list_voltage,
	.is_enabled = act8891_ldo_is_enabled,
	.enable = act8891_ldo_enable,
	.disable = act8891_ldo_disable,
	.get_mode = act8891_ldo_get_mode,
	.set_mode = act8891_ldo_set_mode,
	
};

static int act8891_dcdc_list_voltage(struct regulator_dev *dev, unsigned index)
{
	return 1000 * buck_voltage_map[index];
}
static int act8891_dcdc_is_enabled(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1;
	u16 val;
	u16 mask=0x80;	
	val = act8891_reg_read(act8891, act8891_BUCK_CONTR_REG(buck));
	if (val < 0)
		return val;
	 val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8891_dcdc_enable(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	u16 mask=0x80;	
	return act8891_set_bits(act8891, act8891_BUCK_CONTR_REG(buck), mask, 0x80);

}
static int act8891_dcdc_disable(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	u16 mask=0x80;
	 return act8891_set_bits(act8891, act8891_BUCK_CONTR_REG(buck), mask, 0);
}
static int act8891_dcdc_get_voltage(struct regulator_dev *dev)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	u16 reg = 0;
	int val;
	reg = act8891_reg_read(act8891,act8891_BUCK_SET_VOL_REG(buck));
	reg &= BUCK_VOL_MASK;
	val = 1000 * buck_voltage_map[reg];	
	return val;
}
static int act8891_dcdc_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map = buck_voltage_map;
	u16 val;
	int ret = 0;

	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX;
	     val++){
		if (vol_map[val] >= min_vol)
			break;}

	if (vol_map[val] > max_vol)
		return -EINVAL;
	ret = act8891_set_bits(act8891, act8891_BUCK_SET_VOL_REG(buck),
	       	BUCK_VOL_MASK, val);
	if (ret)
		return ret;
}
static int act8891_dcdc_get_mode(struct regulator_dev *dev, unsigned index)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	u16 mask = 0xcf;
	u16 val;
	val = act8891_reg_read(act8891, act8891_BUCK_CONTR_REG(buck));
	val=val|mask;
	if (val== mask)
		return REGULATOR_MODE_STANDBY;
	else
		return REGULATOR_MODE_NORMAL;

}
static int act8891_dcdc_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8891 *act8891 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) -ACT8891_DCDC1 ;
	u16 mask = 0x20;
	switch(mode)
	{
	case REGULATOR_MODE_STANDBY:
		return act8891_set_bits(act8891, act8891_BUCK_CONTR_REG(buck), mask, 0);
	case REGULATOR_MODE_NORMAL:
		return act8891_set_bits(act8891, act8891_BUCK_CONTR_REG(buck), mask, mask);
	default:
		printk("error:pmu_act8891 only powersave and pwm mode\n");
		return -EINVAL;
	}


}
static struct regulator_ops act8891_dcdc_ops = { 
	.set_voltage = act8891_dcdc_set_voltage,
	.get_voltage = act8891_dcdc_get_voltage,
	.list_voltage= act8891_dcdc_list_voltage,
	.is_enabled = act8891_dcdc_is_enabled,
	.enable = act8891_dcdc_enable,
	.disable = act8891_dcdc_disable,
	.get_mode = act8891_dcdc_get_mode,
	.set_mode = act8891_dcdc_set_mode,
};
static struct regulator_desc regulators[] = {
	{
		.name = "ACT_LDO1",
		.id =0,
		.ops = &act8891_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO2",
		.id = 1,
		.ops = &act8891_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO3",
		.id = 2,
		.ops = &act8891_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_LDO4",
		.id = 3,
		.ops = &act8891_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},

	{
		.name = "ACT_DCDC1",
		.id = 4,
		.ops = &act8891_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_DCDC2",
		.id = 5,
		.ops = &act8891_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "ACT_DCDC3",
		.id = 6,
		.ops = &act8891_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	
};

static int act8891_i2c_read(struct i2c_client *i2c, char reg, int count,	u16 *dest)
{
      int ret;
    struct i2c_adapter *adap;
    struct i2c_msg msgs[2];

    if(!i2c)
		return ret;

	if (count != 1)
		return -EIO;  
  
    adap = i2c->adapter;		
    
    msgs[0].addr = i2c->addr;
    msgs[0].buf = &reg;
    msgs[0].flags = i2c->flags;
    msgs[0].len = 1;
    msgs[0].scl_rate = 200*1000;
    
    msgs[1].buf = dest;
    msgs[1].addr = i2c->addr;
    msgs[1].flags = i2c->flags | I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].scl_rate = 200*1000;
    ret = i2c_transfer(adap, msgs, 2);

	DBG("***run in %s %d msgs[1].buf = %d\n",__FUNCTION__,__LINE__,*(msgs[1].buf));

	return 0;   
}

static int act8891_i2c_write(struct i2c_client *i2c, char reg, int count, const u16 src)
{
	int ret=-1;
	
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	char tx_buf[2];

	if(!i2c)
		return ret;
	if (count != 1)
		return -EIO;
    
	adap = i2c->adapter;		
	tx_buf[0] = reg;
	tx_buf[1] = src;
	
	msg.addr = i2c->addr;
	msg.buf = &tx_buf[0];
	msg.len = 1 +1;
	msg.flags = i2c->flags;   
	msg.scl_rate = 200*1000;	

	ret = i2c_transfer(adap, &msg, 1);
	return ret;	
}

static u8 act8891_reg_read(struct act8891 *act8891, u8 reg)
{
	u16 val = 0;

	mutex_lock(&act8891->io_lock);

	act8891_i2c_read(act8891->i2c, reg, 1, &val);

	DBG("reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);

	mutex_unlock(&act8891->io_lock);

	return val & 0xff;	
}

static int act8891_set_bits(struct act8891 *act8891, u8 reg, u16 mask, u16 val)
{
	u16 tmp;
	int ret;

	mutex_lock(&act8891->io_lock);

	ret = act8891_i2c_read(act8891->i2c, reg, 1, &tmp);
	tmp = (tmp & ~mask) | val;
	if (ret == 0) {
		ret = act8891_i2c_write(act8891->i2c, reg, 1, tmp);
		DBG("reg write 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);
	}
	mutex_unlock(&act8891->io_lock);

	return ret;	
}
static int __devinit setup_regulators(struct act8891 *act8891, struct act8891_platform_data *pdata)
{	
	int i, err;

	act8891->num_regulators = pdata->num_regulators;
	act8891->rdev = kcalloc(pdata->num_regulators,
			       sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!act8891->rdev) {
		return -ENOMEM;
	}
	/* Instantiate the regulators */
	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		act8891->rdev[i] = regulator_register(&regulators[id],
			act8891->dev, pdata->regulators[i].initdata, act8891);
/*
		if (IS_ERR(act8891->rdev[i])) {
			err = PTR_ERR(act8891->rdev[i]);
			dev_err(act8891->dev, "regulator init failed: %d\n",
				err);
			goto error;
		}*/
	}

	return 0;
error:
	while (--i >= 0)
		regulator_unregister(act8891->rdev[i]);
	kfree(act8891->rdev);
	act8891->rdev = NULL;
	return err;
}

static int __devinit act8891_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct act8891 *act8891;	
	struct act8891_platform_data *pdata = i2c->dev.platform_data;
	int ret;
	act8891 = kzalloc(sizeof(struct act8891), GFP_KERNEL);
	if (act8891 == NULL) {
		ret = -ENOMEM;		
		goto err;
	}
	act8891->i2c = i2c;
	act8891->dev = &i2c->dev;
	i2c_set_clientdata(i2c, act8891);
	mutex_init(&act8891->io_lock);	
	
	
	if (pdata) {
		ret = setup_regulators(act8891, pdata);
		if (ret < 0)		
			goto err;
	} else
		dev_warn(act8891->dev, "No platform init data supplied\n");
	
	pdata->set_init(act8891);

	return 0;

err:
	return ret;	

}

static int __devexit act8891_i2c_remove(struct i2c_client *i2c)
{
	struct act8891 *act8891 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < act8891->num_regulators; i++)
		if (act8891->rdev[i])
			regulator_unregister(act8891->rdev[i]);
	kfree(act8891->rdev);
	i2c_set_clientdata(i2c, NULL);
	kfree(act8891);

	return 0;
}

static const struct i2c_device_id act8891_i2c_id[] = {
       { "act8891", 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, act8891_i2c_id);

static struct i2c_driver act8891_i2c_driver = {
	.driver = {
		.name = "act8891",
		.owner = THIS_MODULE,
	},
	.probe    = act8891_i2c_probe,
	.remove   = __devexit_p(act8891_i2c_remove),
	.id_table = act8891_i2c_id,
};

static int __init act8891_module_init(void)
{
	int ret;
	ret = i2c_add_driver(&act8891_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);
	return ret;
}
module_init(act8891_module_init);

static void __exit act8891_module_exit(void)
{
	i2c_del_driver(&act8891_i2c_driver);
}
module_exit(act8891_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhangqing <zhangqing@rock-chips.com>");
MODULE_DESCRIPTION("act8891 PMIC driver");

