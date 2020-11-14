/*
 * drivers/input/touchscreen/gslX680.c
 *
 * Copyright (c) 2012 Shanghai Basewin
 *	Guan Yuwei<guanyuwei@basewin.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "tp_suspend.h"
#include "tp_gslx680_board.h"

#ifdef OLD_VERSION
#if defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_LEHENG_NEW)
#include "rockchip_gsl1680_7inch_800x480_LeHeng.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_SHENGHEXIANG_NEW)
#include "rockchip_gsl1680_7inch_800x480_ShengHeXiang.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_SHENGHEXIANG_QLT_NEW)
#include "rockchip_gsl1680_7inch_800x480_ShengHeXiang_QLT.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_SHENGHEXIANG_NEW)
#include "rockchip_gsl1680_7inch_1024x600_ShengHeXiang.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_YOUCHU_NEW)
#include "rockchip_gsl1680_7inch_800x480_YouChu_dc.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_GANGXINDA_NEW)
#include "rockchip_gsl1680_7inch_800x480_YouChu_NEW_GangXinDa.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_GANGXINDA_NEW)
#include "rockchip_gsl1680_7inch_1024x600_YouChu_NEW_GangXinDa.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_DEYI_NEW)
#include "rockchip_gsl1680_7inch_800x480_DeYi.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_YOUCHU_YISENG_NEW)
#include "rockchip_gsl1680_7inch_800x480_YouChu_YiSeng.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_HAOYUAN_NEW)
#include "rockchip_gsl1680_7inch_800x480_HaoYuan_NEW.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_XINAGRUIDE_NEW)
#include "rockchip_gsl1680_7inch_800x480_XiangRuiDe_NEW.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_YOUCHU_NEW)
#include "rockchip_gsl1680_7inch_1024x600_YouChu.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_HAOYUAN_NEW)
#include "rockchip_gsl1680_7inch_1024x600_HaoYuan.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_DEYI_NEW)
#include "rockchip_gsl1680_7inch_1024x600_DeYi_NEW.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_FEICHU_NEW)
#include "rockchip_gsl1680_7inch_1024x600_FeiChu_NEW.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_K71_NEW)
#include "rockchip_gsl1680_7inch_1024x600_K71_NEW.h"
#elif defined(CONFIG_GSLX680_9INCH_800X480_NEW)
#include "rockchip_gslX680_9inch_800x480.h"
#elif defined(CONFIG_GSLX680_9INCH_800X480_TENGYAO_NEW)
#include "rockchip_gslX680_9inch_800x480_TengYao.h"
#elif defined(CONFIG_GSLX680_9INCH_1024X600_NEW)
#include "rockchip_gslX680_9inch_1024x600.h"
#elif defined(CONFIG_GSLX680_8INCH_800X600_NEW)
#include "rockchip_gslX680_8inch_800x600.h"
#elif defined(CONFIG_GSLX680_8INCH_1024X600_NEW)
#include "rockchip_gslX680_8inch_1024x600.h"
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_43INCH_480x272_HAOYUAN)
#include "rockchip_gsl1680_43inch_480x272_YouChu.h"
#else
#include "rockchip_gslX680.h"
#endif
#else
#endif

#define REPORT_DATA_ANDROID_4_0
#define SLEEP_CLEAR_POINT
#ifdef FILTER_POINT
#define FILTER_MAX	9
#endif

#define GSLX680_I2C_NAME	"gslX680a"
#define GSLX680_I2C_ADDR	0x40

#ifdef OLD_VERSION
#ifdef CONFIG_MACH_RK3028A_86V
	#define IRQ_PORT			RK30_PIN3_PC7
	#if defined(CONFIG_INCAR_PNLVCOM_ADJUST)
		#define WAKE_PORT			RK30_PIN3_PC3
	#else
		#define WAKE_PORT			RK30_PIN3_PC3
	#endif
#else
	#if defined(CONFIG_MACH_RK3026_AT8931_NMC1000) || defined(CONFIG_MACH_RK3026_PMU_ACT9831)
		#define IRQ_PORT			RK30_PIN1_PA1
		#ifdef CONFIG_MACH_RK3026_YK_k71_ACT8931
			#define WAKE_PORT			INVALID_GPIO
		#else
			#define WAKE_PORT			RK30_PIN2_PB2
		#endif
	#elif defined(CONFIG_MACH_RK3026_86V_NOPMU) && defined(CONFIG_MACH_RK3026_86V_WIFI_ESP8089)
		#define IRQ_PORT			RK30_PIN1_PA1
		#define WAKE_PORT			RK30_PIN2_PB2
	#elif defined(CONFIG_PMIC_AXP192)
		#define IRQ_PORT			RK30_PIN2_PB2
		#define WAKE_PORT			INVALID_GPIO
	#elif defined(CONFIG_INCAR_PNLVCOM_ADJUST)
		#define IRQ_PORT			RK30_PIN1_PB0
		#define WAKE_PORT			RK30_PIN2_PB2
	#elif defined(CONFIG_MACH_RK3026_86V_NOPMU)
		#define IRQ_PORT			RK30_PIN1_PA1
		#define WAKE_PORT			INVALID_GPIO
	#else
		#define IRQ_PORT			RK30_PIN1_PB0
		#define WAKE_PORT			RK30_PIN0_PD3
	#endif
#endif
#else
static int g_wake_pin;
#endif
#define GSL_DATA_REG		0x80
#define GSL_STATUS_REG		0xe0
#define GSL_PAGE_REG		0xf0

#define TPD_PROC_DEBUG
#ifdef TPD_PROC_DEBUG
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#define GSL_CONFIG_PROC_FILE "gsl_config"
#define CONFIG_LEN 31
static char gsl_read[CONFIG_LEN];
static u8 gsl_data_proc[8] = {0};
static u8 gsl_proc_flag;

#if defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_1024x600_K71_NEW)
static int revert_xy = 1;
static int revert_x;
static int revert_y;
#elif defined(CONFIG_GSLX680_9INCH_800X480_NEW)
static int revert_xy = 1;
static int revert_x;
static int revert_y;
#elif defined(CONFIG_TOUCHSCREEN_GSL1680_7INCH_800x480_YOUCHU_NEW)
static int revert_xy = 1;
static int revert_x = 1;
static int revert_y = 1;
#elif defined(CONFIG_BOARD_TH71_3128_V1)
static int revert_xy = 1;
static int revert_x = 1;
static int revert_y = 1;
#else
static int revert_xy = 1;
static int revert_x;
static int revert_y = 1;
#endif
#endif

#define PRESS_MAX			255
#define MAX_FINGERS			5
#define MAX_CONTACTS		10
#define DMA_TRANS_LEN		0x20
#ifdef GSL_MONITOR
static struct delayed_work gsl_monitor_work;
static struct workqueue_struct *gsl_monitor_workqueue;
static char int_1st[4] = {0};
static char int_2nd[4] = {0};
static char dac_counter;
static char b0_counter;
static char i2c_lock_flag;
#endif

static struct i2c_client *gsl_client;

#ifdef HAVE_TOUCH_KEY
static u16 key;
static int key_state_flag;
struct key_data {
	u16 key;
	u16 x_min;
	u16 x_max;
	u16 y_min;
	u16 y_max;
};

const u16 key_array[] = {
			KEY_BACK,
			KEY_HOME,
			KEY_MENU,
			KEY_SEARCH,
};
#define MAX_KEY_NUM (sizeof(key_array) / sizeof(key_array[0]))

struct key_data gsl_key_data[MAX_KEY_NUM] = {
	{KEY_BACK, 2048, 2048, 2048, 2048},
	{KEY_HOME, 2048, 2048, 2048, 2048},
	{KEY_MENU, 2048, 2048, 2048, 2048},
	{KEY_SEARCH, 2048, 2048, 2048, 2048},
};
#endif

struct gsl_ts_data {
	u8 x_index;
	u8 y_index;
	u8 z_index;
	u8 id_index;
	u8 touch_index;
	u8 data_reg;
	u8 status_reg;
	u8 data_size;
	u8 touch_bytes;
	u8 update_data;
	u8 touch_meta_data;
	u8 finger_size;
};

static struct gsl_ts_data devices[] = {
	{
		.x_index = 6,
		.y_index = 4,
		.z_index = 5,
		.id_index = 7,
		.data_reg = GSL_DATA_REG,
		.status_reg = GSL_STATUS_REG,
		.update_data = 0x4,
		.touch_bytes = 4,
		.touch_meta_data = 4,
		.finger_size = 70,
	},
};

struct gsl_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct gsl_ts_data *dd;
	u8 *touch_data;
	u8 device_id;
	int irq;
	int irq_pin;
	int wake_pin;
	struct  tp_device  tp;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

#ifdef GSL_DEBUG
#define print_info(fmt, args...)   \
	do {                              \
		pr_info(fmt, ##args);     \
	} while (0)
#else
#define print_info(fmt, args...)
#endif

static u32 id_sign[MAX_CONTACTS + 1] = {0};
static u8 id_state_flag[MAX_CONTACTS + 1] = {0};
static u8 id_state_old_flag[MAX_CONTACTS + 1] = {0};
static u16 x_old[MAX_CONTACTS + 1] = {0};
static u16 y_old[MAX_CONTACTS + 1] = {0};
static u16 x_new;
static u16 y_new;

static int gslX680_shutdown_low(void)
{
	if (g_wake_pin != 0) {
		gpio_direction_output(g_wake_pin, 0);
		gpio_set_value(g_wake_pin, 0);
	}
	return 0;
}

static int gslX680_shutdown_high(void)
{
	if (g_wake_pin != 0) {
		gpio_direction_output(g_wake_pin, 0);
		gpio_set_value(g_wake_pin, 1);
	}
	return 0;
}

static inline u16 join_bytes(u8 a, u8 b)
{
	u16 ab = 0;

	ab = ab | a;
	ab = ab << 8 | b;
	return ab;
}

static u32 gsl_write_interface(struct i2c_client *client, const u8 reg, u8 *buf, u32 num)
{
	struct i2c_msg xfer_msg[1];

	buf[0] = reg;

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num + 1;
	xfer_msg[0].flags = client->flags & I2C_M_TEN;
	xfer_msg[0].buf = buf;

	return i2c_transfer(client->adapter, xfer_msg, 1) == 1 ? 0 : -EFAULT;
}

static int gsl_ts_write(struct i2c_client *client, u8 addr, u8 *pdata, int datalen)
{
	int ret = 0;
	u8 tmp_buf[128];
	unsigned int bytelen = 0;

	if (datalen > 125) {
		pr_info("%s too big datalen = %d!\n", __func__, datalen);
		return -1;
	}

	tmp_buf[0] = addr;
	bytelen++;

	if (datalen != 0 && pdata != NULL) {
		memcpy(&tmp_buf[bytelen], pdata, datalen);
		bytelen += datalen;
	}

	ret = i2c_master_send(client, tmp_buf, bytelen);
	return ret;
}

static int gsl_ts_read(struct i2c_client *client, u8 addr, u8 *pdata, unsigned int datalen)
{
	int ret = 0;

	if (datalen > 126) {
		pr_info("%s too big datalen = %d!\n", __func__, datalen);
		return -1;
	}

	ret = gsl_ts_write(client, addr, NULL, 0);
	if (ret < 0) {
		pr_info("%s set data address fail!\n", __func__);
		return ret;
	}

	return i2c_master_recv(client, pdata, datalen);
}

static __inline__ void fw2buf(u8 *buf, const u32 *fw)
{
	u32 *u32_buf = (int *)buf;
	*u32_buf = *fw;
}

static void gsl_load_fw(struct i2c_client *client)
{
	u8 buf[DMA_TRANS_LEN * 4 + 1] = {0};
	u8 send_flag = 1;
	u8 *cur = buf + 1;
	u32 source_line = 0;
	u32 source_len;
	struct fw_data *ptr_fw;

	pr_info("=============gsl_load_fw start==============\n");

	ptr_fw = GSLX680_FW;
	source_len = ARRAY_SIZE(GSLX680_FW);

	for (source_line = 0; source_line < source_len; source_line++) {
		/* init page trans, set the page val */
		if (ptr_fw[source_line].offset == GSL_PAGE_REG) {
			fw2buf(cur, &ptr_fw[source_line].val);
			gsl_write_interface(client, GSL_PAGE_REG, buf, 4);
			send_flag = 1;
		} else {
			if (send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20) == 1)
				buf[0] = (u8)ptr_fw[source_line].offset;

			fw2buf(cur, &ptr_fw[source_line].val);
			cur += 4;

			if (send_flag % (DMA_TRANS_LEN < 0x20 ? DMA_TRANS_LEN : 0x20) == 0) {
				gsl_write_interface(client, buf[0], buf, cur - buf - 1);
				cur = buf + 1;
			}

			send_flag++;
		}
	}

	pr_info("=============gsl_load_fw end==============\n");
}

static int test_i2c(struct i2c_client *client)
{
	u8 read_buf = 0;
	u8 write_buf = 0x12;
	int ret, rc = 1;

	ret = gsl_ts_read(client, 0xf0, &read_buf, sizeof(read_buf));
	if (ret  < 0)
		rc--;
	else
		pr_info("I read reg 0xf0 is %x\n", read_buf);

	msleep(2);
	ret = gsl_ts_write(client, 0xf0, &write_buf, sizeof(write_buf));
	if (ret  >=  0)
		pr_info("I write reg 0xf0 0x12\n");
	msleep(2);
	ret = gsl_ts_read(client, 0xf0, &read_buf, sizeof(read_buf));
	if (ret <  0)
		rc--;
	else
		pr_info("I read reg 0xf0 is 0x%x\n", read_buf);

	return rc;
}

static void startup_chip(struct i2c_client *client)
{
	u8 tmp = 0x00;

#ifdef GSL_NOID_VERSION
	gsl_DataInit(gsl_config_data_id);
#endif
	gsl_ts_write(client, 0xe0, &tmp, 1);
	msleep(10);
}

static void reset_chip(struct i2c_client *client)
{
	u8 tmp = 0x88;
	u8 buf[4] = {0x00};

	gsl_ts_write(client, 0xe0, &tmp, sizeof(tmp));
	msleep(20);
	tmp = 0x04;
	gsl_ts_write(client, 0xe4, &tmp, sizeof(tmp));
	msleep(10);
	gsl_ts_write(client, 0xbc, buf, sizeof(buf));
	msleep(10);
}

static void clr_reg(struct i2c_client *client)
{
	u8 write_buf[4] = {0};

	write_buf[0] = 0x88;
	gsl_ts_write(client, 0xe0, &write_buf[0], 1);
	msleep(20);
	write_buf[0] = 0x03;
	gsl_ts_write(client, 0x80, &write_buf[0], 1);
	msleep(5);
	write_buf[0] = 0x04;
	gsl_ts_write(client, 0xe4, &write_buf[0], 1);
	msleep(5);
	write_buf[0] = 0x00;
	gsl_ts_write(client, 0xe0, &write_buf[0], 1);
	msleep(20);
}

static void init_chip(struct i2c_client *client)
{
	int rc;
	int i;

	gslX680_shutdown_low();
	msleep(20);
	gslX680_shutdown_high();
	msleep(20);
	for (i = 0; i < 10; i++) {
	rc = test_i2c(client);
		if (rc >= 0)
			break;
	}
	if (rc < 0) {
		pr_info("------gslX680 test_i2c error------\n");
		return;
	}
	clr_reg(client);
	reset_chip(client);
	gsl_load_fw(client);
	startup_chip(client);
	reset_chip(client);
	startup_chip(client);
}

static void check_mem_data(struct i2c_client *client)
{
	u8 read_buf[4]  = {0};

	msleep(30);
	gsl_ts_read(client, 0xb0, read_buf, sizeof(read_buf));

	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a) {
		pr_info("#########check mem read 0xb0 = %x %x %x %x #########\n", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip(client);
	}
}

#ifdef TPD_PROC_DEBUG
static int char_to_int(char ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');
	else
		return (ch - 'a' + 10);
}

static int gsl_config_read_proc(struct seq_file *m, void *v)
{
	char temp_data[5] = {0};
	unsigned int tmp = 0;

	if ('v' == gsl_read[0] && 's' == gsl_read[1]) {
#ifdef GSL_NOID_VERSION
		tmp = gsl_version_id();
#else
		tmp = 0x20121215;
#endif
		seq_printf(m, "version:%x\n", tmp);
	} else if ('r' == gsl_read[0] && 'e' == gsl_read[1]) {
		if ('i' == gsl_read[3]) {
#ifdef GSL_NOID_VERSION
			tmp = (gsl_data_proc[5] << 8) | gsl_data_proc[4];
			seq_printf(m, "gsl_config_data_id[%d] = ", tmp);
			if (tmp >= 0 && tmp < ARRAY_SIZE(gsl_config_data_id))
					seq_printf(m, "%d\n", gsl_config_data_id[tmp]);
#endif
		} else {
			gsl_ts_write(gsl_client, 0Xf0, &gsl_data_proc[4], 4);
			if (gsl_data_proc[0] < 0x80)
				gsl_ts_read(gsl_client, gsl_data_proc[0], temp_data, 4);
			gsl_ts_read(gsl_client, gsl_data_proc[0], temp_data, 4);

			seq_printf(m, "offset : {0x%02x,0x", gsl_data_proc[0]);
			seq_printf(m, "%02x", temp_data[3]);
			seq_printf(m, "%02x", temp_data[2]);
			seq_printf(m, "%02x", temp_data[1]);
			seq_printf(m, "%02x};\n", temp_data[0]);
		}
	}

	return 0;
}

static ssize_t gsl_config_write_proc(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	u8 buf[8] = {0};
	char temp_buf[CONFIG_LEN];
	char *path_buf;
	int tmp = 0;
	int tmp1 = 0;

	print_info("[tp-gsl][%s] \n", __func__);
	if (count > 512) {
		print_info("size not match [%d:%ld]\n", CONFIG_LEN, count);
		return -EFAULT;
	}
	path_buf = kzalloc(count, GFP_KERNEL);
	if (!path_buf) {
		pr_info("alloc path_buf memory error \n");
		return -ENOMEM;
	}
	if (copy_from_user(path_buf, buffer, count)) {
		print_info("copy from user fail\n");
		goto exit_write_proc_out;
	}
	memcpy(temp_buf, path_buf, (count < CONFIG_LEN ? count : CONFIG_LEN));
	print_info("[tp-gsl][%s][%s]\n", __func__, temp_buf);

	buf[3] = char_to_int(temp_buf[14]) << 4 | char_to_int(temp_buf[15]);
	buf[2] = char_to_int(temp_buf[16]) << 4 | char_to_int(temp_buf[17]);
	buf[1] = char_to_int(temp_buf[18]) << 4 | char_to_int(temp_buf[19]);
	buf[0] = char_to_int(temp_buf[20]) << 4 | char_to_int(temp_buf[21]);

	buf[7] = char_to_int(temp_buf[5]) << 4 | char_to_int(temp_buf[6]);
	buf[6] = char_to_int(temp_buf[7]) << 4 | char_to_int(temp_buf[8]);
	buf[5] = char_to_int(temp_buf[9]) << 4 | char_to_int(temp_buf[10]);
	buf[4] = char_to_int(temp_buf[11]) << 4 | char_to_int(temp_buf[12]);
	if ('v' == temp_buf[0] && 's' == temp_buf[1]) {
		memcpy(gsl_read, temp_buf, 4);
		pr_info("gsl version\n");
	} else if ('s' == temp_buf[0] && 't' == temp_buf[1]) {
	#ifdef GSL_MONITOR
		cancel_delayed_work_sync(&gsl_monitor_work);
	#endif
		gsl_proc_flag = 1;
		reset_chip(gsl_client);
	} else if ('e' == temp_buf[0] && 'n' == temp_buf[1])	{
		msleep(20);
		reset_chip(gsl_client);
		startup_chip(gsl_client);
		gsl_proc_flag = 0;
	} else if ('r' == temp_buf[0] && 'e' == temp_buf[1]) {
		memcpy(gsl_read, temp_buf, 4);
		memcpy(gsl_data_proc, buf, 8);
	} else if ('w' == temp_buf[0] && 'r' == temp_buf[1]) {
		gsl_ts_write(gsl_client, buf[4], buf, 4);
	}
#ifdef GSL_NOID_VERSION
	else if ('i' == temp_buf[0] && 'd' == temp_buf[1]) {
		tmp1 = (buf[7] << 24) | (buf[6] << 16) | (buf[5] << 8) | buf[4];
		tmp = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
		if (tmp1 >= 0 && tmp1 < ARRAY_SIZE(gsl_config_data_id))
			gsl_config_data_id[tmp1] = tmp;
	}
#endif
exit_write_proc_out:
	kfree(path_buf);
	return count;
}

static int gsl_server_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsl_config_read_proc, NULL);
}

static const struct file_operations gsl_seq_fops = {
	.open = gsl_server_list_open,
	.read = seq_read,
	.release = single_release,
	.write = gsl_config_write_proc,
	.owner = THIS_MODULE,
};
#endif

#ifdef FILTER_POINT
static void filter_point(u16 x, u16 y, u8 id)
{
	u16 x_err = 0;
	u16 y_err = 0;
	u16 filter_step_x = 0, filter_step_y = 0;

	id_sign[id] = id_sign[id] + 1;
	if (id_sign[id] == 1) {
		x_old[id] = x;
		y_old[id] = y;
	}

	x_err = x > x_old[id] ? (x - x_old[id]) : (x_old[id] - x);
	y_err = y > y_old[id] ? (y - y_old[id]) : (y_old[id] - y);

	if ((x_err > FILTER_MAX && y_err > FILTER_MAX / 3) || (x_err > FILTER_MAX / 3 && y_err > FILTER_MAX)) {
		filter_step_x = x_err;
		filter_step_y = y_err;
	} else {
		if (x_err > FILTER_MAX)
			filter_step_x = x_err;
		if (y_err > FILTER_MAX)
			filter_step_y = y_err;
	}

	if (x_err <= 2 * FILTER_MAX && y_err <= 2 * FILTER_MAX) {
		filter_step_x >>= 2;
		filter_step_y >>= 2;
	} else if (x_err <= 3 * FILTER_MAX && y_err <= 3 * FILTER_MAX) {
		filter_step_x >>= 1;
		filter_step_y >>= 1;
	} else if (x_err <= 4 * FILTER_MAX && y_err <= 4 * FILTER_MAX) {
		filter_step_x = filter_step_x * 3 / 4;
		filter_step_y = filter_step_y * 3 / 4;
	}

	x_new = x > x_old[id] ? (x_old[id] + filter_step_x) : (x_old[id] - filter_step_x);
	y_new = y > y_old[id] ? (y_old[id] + filter_step_y) : (y_old[id] - filter_step_y);

	x_old[id] = x_new;
	y_old[id] = y_new;
}
#else
static void record_point(u16 x, u16 y, u8 id)
{
	u16 x_err = 0;
	u16 y_err = 0;

	id_sign[id] = id_sign[id] + 1;

	if (id_sign[id] == 1) {
		x_old[id] = x;
		y_old[id] = y;
	}

	x = (x_old[id] + x) / 2;
	y = (y_old[id] + y) / 2;

	if (x > x_old[id])
		x_err = x - x_old[id];
	else
		x_err = x_old[id] - x;

	if (y > y_old[id])
		y_err = y - y_old[id];
	else
		y_err = y_old[id] - y;

	if ((x_err > 3 && y_err > 1) || (x_err > 1 && y_err > 3)) {
		x_new = x;
		x_old[id] = x;
		y_new = y;
		y_old[id] = y;
	} else {
		if (x_err > 3) {
			x_new = x;
			x_old[id] = x;
		} else {
			x_new = x_old[id];
		}
		if (y_err > 3) {
			y_new = y;
			y_old[id] = y;
		} else {
			y_new = y_old[id];
		}
	}

	if (id_sign[id] == 1) {
		x_new = x_old[id];
		y_new = y_old[id];
	}
}
#endif

#ifdef HAVE_TOUCH_KEY
static void report_key(struct gsl_ts *ts, u16 x, u16 y)
{
	u16 i = 0;

	for (i = 0; i < MAX_KEY_NUM; i++) {
		if ((gsl_key_data[i].x_min < x) && (x < gsl_key_data[i].x_max) && (gsl_key_data[i].y_min < y) && (y < gsl_key_data[i].y_max)) {
			key = gsl_key_data[i].key;
			input_report_key(ts->input, key, 1);
			input_sync(ts->input);
			key_state_flag = 1;
			break;
		}
	}
}
#endif

static void report_data(struct gsl_ts *ts, u16 x, u16 y, u8 pressure, u8 id)
{
	if (revert_xy)
		swap(x, y);

	print_info("#####id=%d,x=%d,y=%d######\n", id, x, y);

	if (x > SCREEN_MAX_X || y > SCREEN_MAX_Y) {
	#ifdef HAVE_TOUCH_KEY
		report_key(ts, x, y);
	#endif
		return;
	}

	if (revert_x)
		x = SCREEN_MAX_X - x - 1;
	if (revert_y)
		y = SCREEN_MAX_Y - y - 1;
#ifdef REPORT_DATA_ANDROID_4_0
	input_mt_slot(ts->input, id);
	input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, pressure);
	input_report_abs(ts->input, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 1);
#else
	input_report_abs(ts->input, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, pressure);
	input_report_abs(ts->input, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, 1);
	input_mt_sync(ts->input);
#endif
}

static void gslX680_ts_worker(struct work_struct *work)
{
	int rc, i;
	u8 id, touches;
	u16 x, y;
#ifdef GSL_NOID_VERSION
	unsigned int tmp1;
	u8 buf[4] = {0};
	struct gsl_touch_info cinfo;
#endif
	struct gsl_ts *ts = container_of(work, struct gsl_ts, work);

	print_info("=====gslX680_ts_worker=====\n");

#ifdef TPD_PROC_DEBUG
	if (gsl_proc_flag == 1)
		goto schedule;
#endif

#ifdef GSL_MONITOR
	if (i2c_lock_flag != 0)
		goto i2c_lock_schedule;
	else
		i2c_lock_flag = 1;
#endif

	rc = gsl_ts_read(ts->client, 0x80, ts->touch_data, ts->dd->data_size);
	if (rc < 0)  {
		dev_err(&ts->client->dev, "read failed\n");
		reset_chip(ts->client);
		startup_chip(ts->client);
		goto schedule;
	}

	touches = ts->touch_data[ts->dd->touch_index];
	print_info("-----touches: %d -----\n", touches);
#ifdef GSL_NOID_VERSION
	cinfo.finger_num = touches;
	print_info("tp-gsl  finger_num = %d\n", cinfo.finger_num);
	for (i = 0; i < (touches < MAX_CONTACTS ? touches : MAX_CONTACTS); i++) {
		cinfo.x[i] = join_bytes((ts->touch_data[ts->dd->x_index  + 4 * i + 1] & 0xf),
				ts->touch_data[ts->dd->x_index + 4 * i]);
		cinfo.y[i] = join_bytes(ts->touch_data[ts->dd->y_index + 4 * i + 1],
							ts->touch_data[ts->dd->y_index + 4 * i]);
		print_info("tp-gsl  x = %d y = %d \n", cinfo.x[i], cinfo.y[i]);
	}
	cinfo.finger_num = (ts->touch_data[3] << 24) | (ts->touch_data[2] << 16)
		| (ts->touch_data[1] << 8) | (ts->touch_data[0]);
	gsl_alg_id_main(&cinfo);
	tmp1 = gsl_mask_tiaoping();
	print_info("[tp-gsl] tmp1=%x\n", tmp1);
	if (tmp1 > 0 && tmp1 < 0xffffffff) {
		buf[0] = 0xa;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0;
		gsl_ts_write(ts->client, 0xf0, buf, 4);
		buf[0] = (u8)(tmp1 & 0xff);
		buf[1] = (u8)((tmp1 >> 8) & 0xff);
		buf[2] = (u8)((tmp1 >> 16) & 0xff);
		buf[3] = (u8)((tmp1 >> 24) & 0xff);
		print_info("tmp1=%08x,buf[0]=%02x,buf[1]=%02x,buf[2]=%02x,buf[3]=%02x\n",
					tmp1, buf[0], buf[1], buf[2], buf[3]);
		gsl_ts_write(ts->client, 0x8, buf, 4);
	}
	touches = cinfo.finger_num;
#endif

	for (i = 1; i <= MAX_CONTACTS; i++) {
		if (touches == 0)
			id_sign[i] = 0;
		id_state_flag[i] = 0;
	}
	for (i = 0; i < (touches > MAX_FINGERS ? MAX_FINGERS : touches); i++) {
	#ifdef GSL_NOID_VERSION
		id = cinfo.id[i];
		x =  cinfo.x[i];
		y =  cinfo.y[i];
	#else
		x = join_bytes((ts->touch_data[ts->dd->x_index  + 4 * i + 1] & 0xf),
					ts->touch_data[ts->dd->x_index + 4 * i]);
		y = join_bytes(ts->touch_data[ts->dd->y_index + 4 * i + 1],
					ts->touch_data[ts->dd->y_index + 4 * i]);
		id = ts->touch_data[ts->dd->id_index + 4 * i] >> 4;
	#endif

		if (id >= 1 && id <= MAX_CONTACTS) {
		#ifdef FILTER_POINT
			filter_point(x, y, id);
		#else
			record_point(x, y, id);
		#endif
			report_data(ts, x_new, y_new, 10, id);
			id_state_flag[id] = 1;
		}
	}
	for (i = 1; i <= MAX_CONTACTS; i++) {
		if ((touches == 0) || ((id_state_old_flag[i] != 0) && (id_state_flag[i] == 0)))	{
		#ifdef REPORT_DATA_ANDROID_4_0
			input_mt_slot(ts->input, i);
			input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
			input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		#endif
			id_sign[i] = 0;
		}
		id_state_old_flag[i] = id_state_flag[i];
	}
#ifndef REPORT_DATA_ANDROID_4_0
	if (touches == 0) {
		input_mt_sync(ts->input);
	#ifdef HAVE_TOUCH_KEY
		if (key_state_flag) {
			input_report_key(ts->input, key, 0);
			input_sync(ts->input);
			key_state_flag = 0;
		}
	#endif
	}
#endif
	input_sync(ts->input);

schedule:
#ifdef GSL_MONITOR
	i2c_lock_flag = 0;
i2c_lock_schedule:
#endif
	enable_irq(ts->irq);
}

#ifdef GSL_MONITOR
static void gsl_monitor_worker(void)
{
	char write_buf[4] = {0};
	char read_buf[4]  = {0};

	print_info("----------------gsl_monitor_worker-----------------\n");

	if (i2c_lock_flag != 0)
		goto queue_monitor_work;
	else
		i2c_lock_flag = 1;

	gsl_ts_read(gsl_client, 0xb0, read_buf, 4);
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
		b0_counter++;
	else
		b0_counter = 0;

	if (b0_counter > 1) {
		pr_info("======read 0xb0: %x %x %x %x ======\n", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip(gsl_client);
		b0_counter = 0;
	}

	gsl_ts_read(gsl_client, 0xb4, read_buf, 4);
	int_2nd[3] = int_1st[3];
	int_2nd[2] = int_1st[2];
	int_2nd[1] = int_1st[1];
	int_2nd[0] = int_1st[0];
	int_1st[3] = read_buf[3];
	int_1st[2] = read_buf[2];
	int_1st[1] = read_buf[1];
	int_1st[0] = read_buf[0];

	if (int_1st[3] == int_2nd[3] && int_1st[2] == int_2nd[2] && int_1st[1] == int_2nd[1] && int_1st[0] == int_2nd[0]) {
		pr_info("======int_1st: %x %x %x %x , int_2nd: %x %x %x %x ======\n", int_1st[3], int_1st[2], int_1st[1], int_1st[0], int_2nd[3], int_2nd[2], int_2nd[1], int_2nd[0]);
		init_chip(gsl_client);
	}

	write_buf[3] = 0x01;
	write_buf[2] = 0xfe;
	write_buf[1] = 0x10;
	write_buf[0] = 0x00;
	gsl_ts_write(gsl_client, 0xf0, write_buf, 4);
	gsl_ts_read(gsl_client, 0x10, read_buf, 4);
	gsl_ts_read(gsl_client, 0x10, read_buf, 4);

	if (read_buf[3] < 10 && read_buf[2] < 10 && read_buf[1] < 10 && read_buf[0] < 10)
		dac_counter++;
	else
		dac_counter = 0;

	if (dac_counter > 1) {
		pr_info("======read DAC1_0: %x %x %x %x ======\n", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		init_chip(gsl_client);
		dac_counter = 0;
	}

	i2c_lock_flag = 0;

queue_monitor_work:
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 100);
}
#endif

static irqreturn_t gsl_ts_irq(int irq, void *dev_id)
{
	struct gsl_ts *ts = dev_id;

	print_info("========gslX680 Interrupt=========\n");

	disable_irq_nosync(ts->irq);

	if (!work_pending(&ts->work))
		queue_work(ts->wq, &ts->work);

	return IRQ_HANDLED;
}

static int gslX680_ts_init(struct i2c_client *client, struct gsl_ts *ts)
{
	struct input_dev *input_device;
	int rc = 0;

	pr_info("[GSLX680] Enter %s\n", __func__);

	ts->dd = &devices[ts->device_id];

	if (ts->device_id == 0) {
		ts->dd->data_size = MAX_FINGERS * ts->dd->touch_bytes + ts->dd->touch_meta_data;
		ts->dd->touch_index = 0;
	}

	ts->touch_data = devm_kzalloc(&client->dev, ts->dd->data_size, GFP_KERNEL);
	if (!ts->touch_data) {
		pr_err("%s: Unable to allocate memory\n", __func__);
		return -ENOMEM;
	}

	input_device = devm_input_allocate_device(&client->dev);
	if (!input_device) {
		rc = -ENOMEM;
		goto error_alloc_dev;
	}

	ts->input = input_device;
	input_device->name = GSLX680_I2C_NAME;
	input_device->id.bustype = BUS_I2C;
	input_device->dev.parent = &client->dev;
	input_set_drvdata(input_device, ts);

#ifdef REPORT_DATA_ANDROID_4_0
	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(EV_KEY, input_device->evbit);
	__set_bit(EV_REP, input_device->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_device->propbit);
	input_mt_init_slots(input_device, (MAX_CONTACTS + 1), 0);
#else
	input_set_abs_params(input_device, ABS_MT_TRACKING_ID, 0, (MAX_CONTACTS + 1), 0, 0);
	set_bit(EV_ABS, input_device->evbit);
	set_bit(EV_KEY, input_device->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_device->propbit);
	input_device->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif

#ifdef HAVE_TOUCH_KEY
	input_device->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 0; i < MAX_KEY_NUM; i++)
		set_bit(key_array[i], input_device->keybit);
#endif

	set_bit(ABS_MT_POSITION_X, input_device->absbit);
	set_bit(ABS_MT_POSITION_Y, input_device->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_device->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_device->absbit);

	input_set_abs_params(input_device, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_device, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);

	ts->wq = create_singlethread_workqueue("kworkqueue_ts");
	if (!ts->wq) {
		dev_err(&client->dev, "Could not create workqueue\n");
		goto error_wq_create;
	}
	flush_workqueue(ts->wq);

	INIT_WORK(&ts->work, gslX680_ts_worker);

	rc = input_register_device(input_device);
	if (rc)
		goto error_unreg_device;

	return 0;

error_unreg_device:
	destroy_workqueue(ts->wq);
error_wq_create:
error_alloc_dev:
	return rc;
}

static int gsl_ts_suspend(struct device *dev)
{
	struct gsl_ts *ts = dev_get_drvdata(dev);
	int i;

	pr_info("I'am in gsl_ts_suspend() start\n");

#ifdef GSL_MONITOR
	pr_info("gsl_ts_suspend () : cancel gsl_monitor_work\n");
	cancel_delayed_work_sync(&gsl_monitor_work);
#endif

	disable_irq_nosync(ts->irq);

	gslX680_shutdown_low();

#ifdef SLEEP_CLEAR_POINT
	msleep(10);
	#ifdef REPORT_DATA_ANDROID_4_0
	for (i = 1; i <= MAX_CONTACTS; i++) {
		input_mt_slot(ts->input, i);
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	}
	#else
	input_mt_sync(ts->input);
	#endif
	input_sync(ts->input);
	msleep(10);
	report_data(ts, 1, 1, 10, 1);
	input_sync(ts->input);
#endif

	return 0;
}

static int gsl_ts_resume(struct device *dev)
{
	struct gsl_ts *ts = dev_get_drvdata(dev);
	int i;

	pr_info("I'am in gsl_ts_resume() start\n");

	gslX680_shutdown_high();
	msleep(20);
	reset_chip(ts->client);
	startup_chip(ts->client);
	check_mem_data(ts->client);

#ifdef SLEEP_CLEAR_POINT
	#ifdef REPORT_DATA_ANDROID_4_0
	for (i = 1; i <= MAX_CONTACTS; i++) {
		input_mt_slot(ts->input, i);
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	}
	#else
	input_mt_sync(ts->input);
	#endif
	input_sync(ts->input);
#endif
#ifdef GSL_MONITOR
	pr_info("gsl_ts_resume () : queue gsl_monitor_work\n");
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 300);
#endif
	pr_info("gsl_ts_resume () end\n");
	enable_irq(ts->irq);
	return 0;
}

#if 1
static int gsl_ts_early_suspend(struct tp_device *tp_d)
{
	struct gsl_ts *ts = container_of(tp_d, struct gsl_ts, tp);

	pr_info("[GSLX680] Enter %s\n", __func__);
	return gsl_ts_suspend(&ts->client->dev);
}

static int gsl_ts_late_resume(struct tp_device *tp_d)
{
	struct gsl_ts *ts = container_of(tp_d, struct gsl_ts, tp);

	pr_info("[GSLX680] Enter %s\n", __func__);
	return gsl_ts_resume(&ts->client->dev);
}
#endif

static int  gsl_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct gsl_ts *ts;
	int rc;
	struct device_node *np = client->dev.of_node;
	enum of_gpio_flags wake_flags;
	unsigned long irq_flags;

	pr_info("GSLX680 Enter %s\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C functionality not supported\n");
		return -ENODEV;
	}
	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	pr_info("==kzalloc success=\n");

	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->device_id = id->driver_data;

	ts->irq_pin = of_get_named_gpio_flags(np, "irq-gpio", 0, (enum of_gpio_flags *)&irq_flags);
	ts->wake_pin = of_get_named_gpio_flags(np, "wake-gpio", 0, &wake_flags);

	if (gpio_is_valid(ts->wake_pin)) {
		rc = devm_gpio_request_one(&client->dev, ts->wake_pin, (wake_flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH, "gslX680 wake pin");
		if (rc != 0) {
			dev_err(&client->dev, "gslX680 wake pin error\n");
			return -EIO;
		}
		g_wake_pin = ts->wake_pin;
	} else {
		dev_info(&client->dev, "wake pin invalid\n");
	}
	if (gpio_is_valid(ts->irq_pin)) {
		rc = devm_gpio_request_one(&client->dev, ts->irq_pin, (irq_flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH, "gslX680 irq pin");
		if (rc != 0) {
			dev_err(&client->dev, "gslX680 irq pin error\n");
			return -EIO;
		}
	} else {
		dev_info(&client->dev, "irq pin invalid\n");
	}

	rc = gslX680_ts_init(client, ts);
	if (rc < 0) {
		dev_err(&client->dev, "GSLX680 init failed\n");
		goto error_mutex_destroy;
	}

	gsl_client = client;

	init_chip(ts->client);
	check_mem_data(ts->client);

	ts->irq = gpio_to_irq(ts->irq_pin);
	if (ts->irq) {
		rc = devm_request_irq(&client->dev, ts->irq, gsl_ts_irq, IRQF_TRIGGER_RISING, client->name, ts);
		if (rc != 0) {
			pr_info("Cannot allocate ts INT!ERRNO:%d\n", rc);
			goto error_req_irq_fail;
		}
		disable_irq(ts->irq);
	} else {
		pr_info("gsl x680 irq req fail\n");
		goto error_req_irq_fail;
	}
	/* create debug attribute */
	ts->tp.tp_resume = gsl_ts_late_resume;
	ts->tp.tp_suspend = gsl_ts_early_suspend;
	tp_register_fb(&ts->tp);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = gsl_ts_early_suspend;
	ts->early_suspend.resume = gsl_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef GSL_MONITOR
	pr_info("gsl_ts_probe () : queue gsl_monitor_workqueue\n");

	INIT_DELAYED_WORK(&gsl_monitor_work, gsl_monitor_worker);
	gsl_monitor_workqueue = create_singlethread_workqueue("gsl_monitor_workqueue");
	queue_delayed_work(gsl_monitor_workqueue, &gsl_monitor_work, 1000);
#endif

#ifdef TPD_PROC_DEBUG
	proc_create(GSL_CONFIG_PROC_FILE, 0644, NULL, &gsl_seq_fops);
	gsl_proc_flag = 0;
#endif

	pr_info("[GSLX680] End %s\n", __func__);

	return 0;

error_req_irq_fail:

error_mutex_destroy:
	return rc;
}

static int gsl_ts_remove(struct i2c_client *client)
{
	struct gsl_ts *ts = i2c_get_clientdata(client);

	pr_info("==gsl_ts_remove=\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

#ifdef GSL_MONITOR
	cancel_delayed_work_sync(&gsl_monitor_work);
	destroy_workqueue(gsl_monitor_workqueue);
#endif

	device_init_wakeup(&client->dev, 0);
	cancel_work_sync(&ts->work);
	destroy_workqueue(ts->wq);


	return 0;
}

static const struct i2c_device_id gsl_ts_id[] = {
	{GSLX680_I2C_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, gsl_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id gslx680a_of_match[] = {
	{.compatible = "gsl,gslx680a"},
	{}
};
MODULE_DEVICE_TABLE(of, gslx680a_of_match);
#endif

static struct i2c_driver gsl_ts_driver = {
	.driver = {
		.of_match_table = of_match_ptr(gslx680a_of_match),
		.name = GSLX680_I2C_NAME,
	},
	.probe		= gsl_ts_probe,
	.remove		= gsl_ts_remove,
	.id_table	= gsl_ts_id,
};

module_i2c_driver(gsl_ts_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GSLX680 touchscreen controller driver");
MODULE_AUTHOR("Guan Yuwei, guanyuwei@basewin.com");
MODULE_ALIAS("platform:gsl_ts");
