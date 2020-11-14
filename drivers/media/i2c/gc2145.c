// SPDX-License-Identifier: GPL-2.0
/*
 * GC2145 CMOS Image Sensor driver
 *
 * Copyright (C) 2015 Texas Instruments, Inc.
 *
 * Benoit Parrot <bparrot@ti.com>
 * Lad, Prabhakar <prabhakar.csengg@gmail.com>
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>

#define DRIVER_NAME "gc2145"
#define GC2145_PIXEL_RATE		(120 * 1000 * 1000)

/*
 * GC2145 register definitions
 */
#define REG_SOFTWARE_STANDBY		0xf2

#define REG_SC_CHIP_ID_H		0xf0
#define REG_SC_CHIP_ID_L		0xf1

#define REG_NULL			0xFFFF	/* Array end token */

#define SENSOR_ID(_msb, _lsb)		((_msb) << 8 | (_lsb))
#define GC2145_ID			0x2145

struct sensor_register {
	u16 addr;
	u8 value;
};

struct gc2145_framesize {
	u16 width;
	u16 height;
	u16 fps;
	u16 max_exp_lines;
	const struct sensor_register *regs;
};

struct gc2145_pll_ctrl {
	u8 ctrl1;
	u8 ctrl2;
	u8 ctrl3;
};

struct gc2145_pixfmt {
	u32 code;
	/* Output format Register Value (REG_FORMAT_CTRL00) */
	struct sensor_register *format_ctrl_regs;
};

struct pll_ctrl_reg {
	unsigned int div;
	unsigned char reg;
};

static const char * const gc2145_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
	"dvdd",		/* Digital core power */
};

#define GC2145_NUM_SUPPLIES ARRAY_SIZE(gc2145_supply_names)

struct gc2145 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	unsigned int fps;
	unsigned int xvclk_frequency;
	struct clk *xvclk;
	struct gpio_desc *pwdn_gpio;
	struct regulator_bulk_data supplies[GC2145_NUM_SUPPLIES];
	struct mutex lock;
	struct i2c_client *client;
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_frequency;
	const struct gc2145_framesize *frame_size;
	int streaming;
};

static const struct sensor_register gc2145_init_regs[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0xfe},
	{0xf2, 0x00},
	/*ISP reg*/
	{0xfe, 0x00},
	{0x03, 0x04},
	{0x04, 0xe2},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x52},
	{0x12, 0x2e},
	{0x17, 0x14},
	{0x18, 0x22},
	{0x19, 0x0e},
	{0x1a, 0x01},
	{0x1b, 0x4b},
	{0x1c, 0x07},
	{0x1d, 0x10},
	{0x1e, 0x88},
	{0x1f, 0x78},
	{0x20, 0x03},
	{0x21, 0x40},
	{0x22, 0xa0},
	{0x24, 0x16},
	{0x25, 0x01},
	{0x26, 0x10},
	{0x2d, 0x60},
	{0x30, 0x01},
	{0x31, 0x90},
	{0x33, 0x06},
	{0x34, 0x01},
	{0xfe, 0x00},
	{0x80, 0x7f},
	{0x81, 0x26},
	{0x82, 0xfa},
	{0x83, 0x00},
	{0x84, 0x00},
	{0x86, 0x02},
	{0x88, 0x03},
	{0x89, 0x03},
	{0x85, 0x08},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0xb0, 0x55},
	{0xc3, 0x00},
	{0xc4, 0x80},
	{0xc5, 0x90},
	{0xc6, 0x3b},
	{0xc7, 0x46},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xb6, 0x01},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	/*BLK*/
	{0xfe, 0x00},
	{0x40, 0x42},
	{0x41, 0x00},
	{0x43, 0x5b},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},
	{0x76, 0x00},
	{0x6a, 0x08},
	{0x6b, 0x08},
	{0x6c, 0x08},
	{0x6d, 0x08},
	{0x6e, 0x08},
	{0x6f, 0x08},
	{0x70, 0x08},
	{0x71, 0x08},
	{0x76, 0x00},
	{0x72, 0xf0},
	{0x7e, 0x3c},
	{0x7f, 0x00},
	{0xfe, 0x02},
	{0x48, 0x15},
	{0x49, 0x00},
	{0x4b, 0x0b},
	{0xfe, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x09, 0x00},
	{0x0a, 0x82},
	{0x0b, 0x11},
	{0x0c, 0x10},
	{0x11, 0x10},
	{0x13, 0x7b},
	{0x17, 0x00},
	{0x1c, 0x11},
	{0x1e, 0x61},
	{0x1f, 0x35},
	{0x20, 0x40},
	{0x22, 0x40},
	{0x23, 0x20},
	{0xfe, 0x02},
	{0x0f, 0x04},
	{0xfe, 0x01},
	{0x12, 0x35},
	{0x15, 0xb0},
	{0x10, 0x31},
	{0x3e, 0x28},
	{0x3f, 0xb0},
	{0x40, 0x90},
	{0x41, 0x0f},

	/*INTPEE*/
	{0xfe, 0x02},
	{0x90, 0x6c},
	{0x91, 0x03},
	{0x92, 0xcb},
	{0x94, 0x33},
	{0x95, 0x84},
	{0x97, 0x45},
	{0xa2, 0x11},
	{0xfe, 0x00},
	/*DNDD*/
	{0xfe, 0x02},
	{0x80, 0xc1},
	{0x81, 0x08},
	{0x82, 0x1f},
	{0x83, 0x10},
	{0x84, 0x0a},
	{0x86, 0xf0},
	{0x87, 0x50},
	{0x88, 0x15},
	{0x89, 0xb0},
	{0x8a, 0x30},
	{0x8b, 0x10},
	/*ASDE*/
	{0xfe, 0x01},
	{0x21, 0x04},
	{0xfe, 0x02},
	{0xa3, 0x50},
	{0xa4, 0x20},
	{0xa5, 0x40},
	{0xa6, 0x80},
	{0xab, 0x40},
	{0xae, 0x0c},
	{0xb3, 0x46},
	{0xb4, 0x64},
	{0xb6, 0x38},
	{0xb7, 0x01},
	{0xb9, 0x2b},
	{0x3c, 0x04},
	{0x3d, 0x15},
	{0x4b, 0x06},
	{0x4c, 0x20},
	{0xfe, 0x00},
	/*GAMMA*/
	/*gamma1*/
#if 1
	{0xfe, 0x02},
	{0x10, 0x09},
	{0x11, 0x0d},
	{0x12, 0x13},
	{0x13, 0x19},
	{0x14, 0x27},
	{0x15, 0x37},
	{0x16, 0x45},
	{0x17, 0x53},
	{0x18, 0x69},
	{0x19, 0x7d},
	{0x1a, 0x8f},
	{0x1b, 0x9d},
	{0x1c, 0xa9},
	{0x1d, 0xbd},
	{0x1e, 0xcd},
	{0x1f, 0xd9},
	{0x20, 0xe3},
	{0x21, 0xea},
	{0x22, 0xef},
	{0x23, 0xf5},
	{0x24, 0xf9},
	{0x25, 0xff},
#else
	{0xfe, 0x02},
	{0x10, 0x0a},
	{0x11, 0x12},
	{0x12, 0x19},
	{0x13, 0x1f},
	{0x14, 0x2c},
	{0x15, 0x38},
	{0x16, 0x42},
	{0x17, 0x4e},
	{0x18, 0x63},
	{0x19, 0x76},
	{0x1a, 0x87},
	{0x1b, 0x96},
	{0x1c, 0xa2},
	{0x1d, 0xb8},
	{0x1e, 0xcb},
	{0x1f, 0xd8},
	{0x20, 0xe2},
	{0x21, 0xe9},
	{0x22, 0xf0},
	{0x23, 0xf8},
	{0x24, 0xfd},
	{0x25, 0xff},
	{0xfe, 0x00},
#endif
	{0xfe, 0x00},
	{0xc6, 0x20},
	{0xc7, 0x2b},
	/*gamma2*/
#if 1
	{0xfe, 0x02},
	{0x26, 0x0f},
	{0x27, 0x14},
	{0x28, 0x19},
	{0x29, 0x1e},
	{0x2a, 0x27},
	{0x2b, 0x33},
	{0x2c, 0x3b},
	{0x2d, 0x45},
	{0x2e, 0x59},
	{0x2f, 0x69},
	{0x30, 0x7c},
	{0x31, 0x89},
	{0x32, 0x98},
	{0x33, 0xae},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe2},
	{0x38, 0xe9},
	{0x39, 0xf3},
	{0x3a, 0xf9},
	{0x3b, 0xff},
#else
	/*Gamma outdoor*/
	{0xfe, 0x02},
	{0x26, 0x17},
	{0x27, 0x18},
	{0x28, 0x1c},
	{0x29, 0x20},
	{0x2a, 0x28},
	{0x2b, 0x34},
	{0x2c, 0x40},
	{0x2d, 0x49},
	{0x2e, 0x5b},
	{0x2f, 0x6d},
	{0x30, 0x7d},
	{0x31, 0x89},
	{0x32, 0x97},
	{0x33, 0xac},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe5},
	{0x38, 0xec},
	{0x39, 0xf8},
	{0x3a, 0xfd},
	{0x3b, 0xff},
#endif
	/*YCP*/
	{0xfe, 0x02},
	{0xd1, 0x40},
	{0xd2, 0x40},
	{0xd3, 0x48},
	{0xd6, 0xf0},
	{0xd7, 0x10},
	{0xd8, 0xda},
	{0xdd, 0x14},
	{0xde, 0x86},
	{0xed, 0x80},
	{0xee, 0x00},
	{0xef, 0x3f},
	{0xd8, 0xd8},
	/*abs*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	/*LSC*/
	{0xfe, 0x01},
	{0xc2, 0x14},
	{0xc3, 0x0d},
	{0xc4, 0x0c},
	{0xc8, 0x15},
	{0xc9, 0x0d},
	{0xca, 0x0a},
	{0xbc, 0x24},
	{0xbd, 0x10},
	{0xbe, 0x0b},
	{0xb6, 0x25},
	{0xb7, 0x16},
	{0xb8, 0x15},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x00},
	{0xbf, 0x07},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xaa, 0x01},
	{0xab, 0x01},
	{0xac, 0x00},
	{0xad, 0x05},
	{0xae, 0x06},
	{0xaf, 0x0e},
	{0xb0, 0x0b},
	{0xb1, 0x07},
	{0xb2, 0x06},
	{0xb3, 0x17},
	{0xb4, 0x0e},
	{0xb5, 0x0e},
	{0xd0, 0x09},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd6, 0x08},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xd3, 0x0a},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x77},
	{0xa7, 0x77},
	{0xa8, 0x77},
	{0xa9, 0x77},
	{0xa1, 0x80},
	{0xa2, 0x80},

	{0xfe, 0x01},
	{0xdf, 0x0d},
	{0xdc, 0x25},
	{0xdd, 0x30},
	{0xe0, 0x77},
	{0xe1, 0x80},
	{0xe2, 0x77},
	{0xe3, 0x90},
	{0xe6, 0x90},
	{0xe7, 0xa0},
	{0xe8, 0x90},
	{0xe9, 0xa0},
	{0xfe, 0x00},
	/*AWB*/
	{0xfe, 0x01},
	{0x4f, 0x00},
	{0x4f, 0x00},
	{0x4b, 0x01},
	{0x4f, 0x00},

	{0x4c, 0x01},
	{0x4d, 0x71},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x91},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x70},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x90},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xb0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x8f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xaf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xd0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xf0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xcf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xef},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xae},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xce},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xad},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcd},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xac},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcc},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcb},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xab},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xaa},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xc9},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x89},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xa9},
	{0x4e, 0x04},
	{0x4c, 0x02},
	{0x4d, 0x0b},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0a},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xeb},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xea},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x09},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x29},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x2a},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x4a},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x8a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x49},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x89},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xa9},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x48},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x68},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xca},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc9},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe9},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x09},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xa7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe7},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x07},
	{0x4e, 0x07},

	{0x4f, 0x01},
	{0x50, 0x80},
	{0x51, 0xa8},
	{0x52, 0x47},
	{0x53, 0x38},
	{0x54, 0xc7},
	{0x56, 0x0e},
	{0x58, 0x08},
	{0x5b, 0x00},
	{0x5c, 0x74},
	{0x5d, 0x8b},
	{0x61, 0xdb},
	{0x62, 0xb8},
	{0x63, 0x86},
	{0x64, 0xc0},
	{0x65, 0x04},
	{0x67, 0xa8},
	{0x68, 0xb0},
	{0x69, 0x00},
	{0x6a, 0xa8},
	{0x6b, 0xb0},
	{0x6c, 0xaf},
	{0x6d, 0x8b},
	{0x6e, 0x50},
	{0x6f, 0x18},
	{0x73, 0xf0},
	{0x70, 0x0d},
	{0x71, 0x60},
	{0x72, 0x80},
	{0x74, 0x01},
	{0x75, 0x01},
	{0x7f, 0x0c},
	{0x76, 0x70},
	{0x77, 0x58},
	{0x78, 0xa0},
	{0x79, 0x5e},
	{0x7a, 0x54},
	{0x7b, 0x58},
	{0xfe, 0x00},
	/*CC*/
	{0xfe, 0x02},
	{0xc0, 0x01},
	{0xc1, 0x44},
	{0xc2, 0xfd},
	{0xc3, 0x04},
	{0xc4, 0xF0},
	{0xc5, 0x48},
	{0xc6, 0xfd},
	{0xc7, 0x46},
	{0xc8, 0xfd},
	{0xc9, 0x02},
	{0xca, 0xe0},
	{0xcb, 0x45},
	{0xcc, 0xec},
	{0xcd, 0x48},
	{0xce, 0xf0},
	{0xcf, 0xf0},
	{0xe3, 0x0c},
	{0xe4, 0x4b},
	{0xe5, 0xe0},
	/*ABS*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	{0xfe, 0x00},
	/*OUTPUT*/
	{0xfe, 0x00},
	{0xf2, 0x0f},
	/*dark sun*/
	{0xfe, 0x02},
	{0x40, 0xbf},
	{0x46, 0xcf},
	{0xfe, 0x00},

	/*frame rate 50Hz*/
	{0xfe, 0x00},
	{0x05, 0x01},
	{0x06, 0x56},
	{0x07, 0x00},
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},

	{0x27, 0x04},
	{0x28, 0xe2},
	{0x29, 0x04},
	{0x2a, 0xe2},
	{0x2b, 0x04},
	{0x2c, 0xe2},
	{0x2d, 0x04},
	{0x2e, 0xe2},
	{0xfe, 0x00},

	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

/* Senor full resolution setting */
static const struct sensor_register gc2145_full_regs[] = {
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x01},
	{0x74, 0x01},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82},
	{0xfe, 0x01},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x15},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

/* Preview resolution setting*/
static const struct sensor_register gc2145_svga_regs_30fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0xb8},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0xac},
	{0x27, 0x05},
	{0x28, 0x04},
	{0x29, 0x05},
	{0x2a, 0x04},
	{0x2b, 0x05},
	{0x2c, 0x04},
	{0x2d, 0x05},
	{0x2e, 0x04},
	{0x3c, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfa, 0x00},
	{0x18, 0x42},
	{0xfd, 0x03},
	{0xb6, 0x01},
	/* crop window */
	{0xfe, 0x00},
	{0x99, 0x11},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	/* AWB */
	{0xfe, 0x00},
	{0xec, 0x01},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x01},
	{0x74, 0x00},
	/* AEC */
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x4c},
	{0x07, 0x14},
	{0x08, 0x36},
	{0x0a, 0xc0},
	{0x21, 0x14},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

/* Preview resolution setting*/
static const struct sensor_register gc2145_svga_regs_20fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x03},
	{0x08, 0x80},
	{0xb6, 0x01},
	{0xfd, 0x03},
	{0xfa, 0x00},
	{0x18, 0x42},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

static const struct gc2145_framesize gc2145_framesizes[] = {
	{ /* SVGA */
		.width		= 800,
		.height		= 600,
		.fps		= 20,
		.regs		= gc2145_svga_regs_20fps,
	}, { /* SVGA */
		.width		= 800,
		.height		= 600,
		.fps		= 30,
		.regs		= gc2145_svga_regs_30fps,
	}, { /* FULL */
		.width		= 1600,
		.height		= 1200,
		.fps		= 20,
		.regs		= gc2145_full_regs,
	}
};

static const struct gc2145_pixfmt gc2145_formats[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_2X8,
	}
};

static inline struct gc2145 *to_gc2145(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2145, sd);
}

/* sensor register write */
static int gc2145_write(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xFF;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"gc2145 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

/* sensor register read */
static int gc2145_read(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	dev_err(&client->dev,
		"gc2145 read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc2145_write_array(struct i2c_client *client,
			      const struct sensor_register *regs)
{
	int i, ret = 0;

	i = 0;
	while (regs[i].addr != REG_NULL) {
		ret = gc2145_write(client, regs[i].addr, regs[i].value);
		if (ret) {
			dev_err(&client->dev, "%s failed !\n", __func__);
			break;
		}

		i++;
	}

	return ret;
}

static void gc2145_get_default_format(struct v4l2_mbus_framefmt *format)
{
	format->width = gc2145_framesizes[0].width;
	format->height = gc2145_framesizes[0].height;
	format->colorspace = V4L2_COLORSPACE_SRGB;
	format->code = gc2145_formats[0].code;
	format->field = V4L2_FIELD_NONE;
}

static void gc2145_set_streaming(struct gc2145 *gc2145, int on)
{
	struct i2c_client *client = gc2145->client;
	int ret;

	dev_dbg(&client->dev, "%s: on: %d\n", __func__, on);

	ret = gc2145_write(client, REG_SOFTWARE_STANDBY, on);
	if (ret)
		dev_err(&client->dev, "gc2145 soft standby failed\n");
}

/*
 * V4L2 subdev video and pad level operations
 */

static int gc2145_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (code->index >= ARRAY_SIZE(gc2145_formats))
		return -EINVAL;

	code->code = gc2145_formats[code->index].code;

	return 0;
}

static int gc2145_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = ARRAY_SIZE(gc2145_formats);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (fse->index >= ARRAY_SIZE(gc2145_framesizes))
		return -EINVAL;

	while (--i)
		if (fse->code == gc2145_formats[i].code)
			break;

	fse->code = gc2145_formats[i].code;

	fse->min_width  = gc2145_framesizes[fse->index].width;
	fse->max_width  = fse->min_width;
	fse->max_height = gc2145_framesizes[fse->index].height;
	fse->min_height = fse->max_height;

	return 0;
}

static int gc2145_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);

	dev_dbg(&client->dev, "%s enter\n", __func__);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		struct v4l2_mbus_framefmt *mf;

		mf = v4l2_subdev_get_try_format(sd, cfg, 0);
		mutex_lock(&gc2145->lock);
		fmt->format = *mf;
		mutex_unlock(&gc2145->lock);
		return 0;
#else
	return -ENOTTY;
#endif
	}

	mutex_lock(&gc2145->lock);
	fmt->format = gc2145->format;
	mutex_unlock(&gc2145->lock);

	dev_dbg(&client->dev, "%s: %x %dx%d\n", __func__,
		gc2145->format.code, gc2145->format.width,
		gc2145->format.height);

	return 0;
}

static void __gc2145_try_frame_size_fps(struct v4l2_mbus_framefmt *mf,
					const struct gc2145_framesize **size,
					unsigned int fps)
{
	const struct gc2145_framesize *fsize = &gc2145_framesizes[0];
	const struct gc2145_framesize *match = NULL;
	unsigned int i = ARRAY_SIZE(gc2145_framesizes);
	unsigned int min_err = UINT_MAX;

	while (i--) {
		unsigned int err = abs(fsize->width - mf->width)
				+ abs(fsize->height - mf->height);
		if (err < min_err && fsize->regs[0].addr) {
			min_err = err;
			match = fsize;
		}
		fsize++;
	}

	if (!match) {
		match = &gc2145_framesizes[0];
	} else {
		fsize = &gc2145_framesizes[0];
		for (i = 0; i < ARRAY_SIZE(gc2145_framesizes); i++) {
			if (fsize->width == match->width &&
			    fsize->height == match->height &&
			    fps >= fsize->fps)
				match = fsize;

			fsize++;
		}
	}

	mf->width  = match->width;
	mf->height = match->height;

	if (size)
		*size = match;
}

static int gc2145_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int index = ARRAY_SIZE(gc2145_formats);
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	const struct gc2145_framesize *size = NULL;
	struct gc2145 *gc2145 = to_gc2145(sd);
	int ret = 0;

	dev_dbg(&client->dev, "%s enter\n", __func__);

	__gc2145_try_frame_size_fps(mf, &size, gc2145->fps);

	while (--index >= 0)
		if (gc2145_formats[index].code == mf->code)
			break;

	if (index < 0)
		return -EINVAL;

	mf->colorspace = V4L2_COLORSPACE_SRGB;
	mf->code = gc2145_formats[index].code;
	mf->field = V4L2_FIELD_NONE;

	mutex_lock(&gc2145->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		mf = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*mf = fmt->format;
#else
		return -ENOTTY;
#endif
	} else {
		if (gc2145->streaming) {
			mutex_unlock(&gc2145->lock);
			return -EBUSY;
		}

		gc2145->frame_size = size;
		gc2145->format = fmt->format;

	}

	mutex_unlock(&gc2145->lock);
	return ret;
}

static int gc2145_s_stream(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);
	int ret = 0;

	dev_dbg(&client->dev, "%s: on: %d\n", __func__, on);

	mutex_lock(&gc2145->lock);

	on = !!on;

	if (gc2145->streaming == on)
		goto unlock;

	if (!on) {
		/* Stop Streaming Sequence */
		gc2145_set_streaming(gc2145, 0x00);
		gc2145->streaming = on;
		if (!IS_ERR(gc2145->pwdn_gpio)) {
			gpiod_set_value_cansleep(gc2145->pwdn_gpio, 1);
			usleep_range(2000, 5000);
		}
		goto unlock;
	}
	if (!IS_ERR(gc2145->pwdn_gpio)) {
		gpiod_set_value_cansleep(gc2145->pwdn_gpio, 0);
		usleep_range(2000, 5000);
	}

	ret = gc2145_write_array(client, gc2145_init_regs);
	if (ret)
		goto unlock;

	ret = gc2145_write_array(client, gc2145->frame_size->regs);
	if (ret)
		goto unlock;

	gc2145_set_streaming(gc2145, 0x0f);
	gc2145->streaming = on;

unlock:
	mutex_unlock(&gc2145->lock);
	return ret;
}

static int gc2145_set_test_pattern(struct gc2145 *gc2145, int value)
{
	return 0;
}

static int gc2145_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc2145 *gc2145 =
			container_of(ctrl->handler, struct gc2145, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		return gc2145_set_test_pattern(gc2145, ctrl->val);
	}

	return 0;
}

static const struct v4l2_ctrl_ops gc2145_ctrl_ops = {
	.s_ctrl = gc2145_s_ctrl,
};

static const char * const gc2145_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bars",
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev internal operations
 */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc2145_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);

	dev_dbg(&client->dev, "%s:\n", __func__);

	gc2145_get_default_format(format);

	return 0;
}
#endif

static int gc2145_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_PARALLEL;
	config->flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
			V4L2_MBUS_VSYNC_ACTIVE_LOW |
			V4L2_MBUS_PCLK_SAMPLE_RISING;

	return 0;
}

static int gc2145_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc2145 *gc2145 = to_gc2145(sd);

	mutex_lock(&gc2145->lock);
	fi->interval.numerator = 10000;
	fi->interval.denominator = gc2145->fps * 10000;
	mutex_unlock(&gc2145->lock);

	return 0;
}

static int gc2145_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);
	const struct gc2145_framesize *size = NULL;
	struct v4l2_mbus_framefmt mf;
	unsigned int fps;
	int ret = 0;

	dev_dbg(&client->dev, "Setting %d/%d frame interval\n",
		 fi->interval.numerator, fi->interval.denominator);

	mutex_lock(&gc2145->lock);
	if (gc2145->format.width == 1600)
		goto unlock;
	fps = DIV_ROUND_CLOSEST(fi->interval.denominator,
				fi->interval.numerator);
	mf = gc2145->format;
	__gc2145_try_frame_size_fps(&mf, &size, fps);
	if (gc2145->frame_size != size) {
		ret = gc2145_write_array(client, size->regs);
		if (ret)
			goto unlock;
		gc2145->frame_size = size;
		gc2145->fps = fps;
	}
unlock:
	mutex_unlock(&gc2145->lock);

	return ret;
}

static const struct v4l2_subdev_core_ops gc2145_subdev_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops gc2145_subdev_video_ops = {
	.s_stream = gc2145_s_stream,
	.g_mbus_config = gc2145_g_mbus_config,
	.g_frame_interval = gc2145_g_frame_interval,
	.s_frame_interval = gc2145_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc2145_subdev_pad_ops = {
	.enum_mbus_code = gc2145_enum_mbus_code,
	.enum_frame_size = gc2145_enum_frame_sizes,
	.get_fmt = gc2145_get_fmt,
	.set_fmt = gc2145_set_fmt,
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_ops gc2145_subdev_ops = {
	.core  = &gc2145_subdev_core_ops,
	.video = &gc2145_subdev_video_ops,
	.pad   = &gc2145_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops gc2145_subdev_internal_ops = {
	.open = gc2145_open,
};
#endif

static int gc2145_detect(struct gc2145 *gc2145)
{
	struct i2c_client *client = gc2145->client;
	u8 pid, ver;
	int ret;

	dev_dbg(&client->dev, "%s:\n", __func__);

	/* Check sensor revision */
	ret = gc2145_read(client, REG_SC_CHIP_ID_H, &pid);
	if (!ret)
		ret = gc2145_read(client, REG_SC_CHIP_ID_L, &ver);

	if (!ret) {
		unsigned short id;

		id = SENSOR_ID(pid, ver);
		if (id != GC2145_ID) {
			ret = -1;
			dev_err(&client->dev,
				"Sensor detection failed (%04X, %d)\n",
				id, ret);
		} else {
			dev_info(&client->dev, "Found GC%04X sensor\n", id);
			if (!IS_ERR(gc2145->pwdn_gpio))
				gpiod_set_value_cansleep(gc2145->pwdn_gpio, 1);
		}
	}

	return ret;
}

static int __gc2145_power_on(struct gc2145 *gc2145)
{
	int ret;
	struct device *dev = &gc2145->client->dev;

	if (!IS_ERR(gc2145->xvclk)) {
		ret = clk_set_rate(gc2145->xvclk, 24000000);
		if (ret < 0)
			dev_info(dev, "Failed to set xvclk rate (24MHz)\n");
	}

	if (!IS_ERR(gc2145->pwdn_gpio)) {
		gpiod_set_value_cansleep(gc2145->pwdn_gpio, 1);
		usleep_range(2000, 5000);
	}

	if (!IS_ERR(gc2145->supplies)) {
		ret = regulator_bulk_enable(GC2145_NUM_SUPPLIES,
			gc2145->supplies);
		if (ret < 0)
			dev_info(dev, "Failed to enable regulators\n");

		usleep_range(20000, 50000);
	}

	if (!IS_ERR(gc2145->pwdn_gpio)) {
		gpiod_set_value_cansleep(gc2145->pwdn_gpio, 0);
		usleep_range(2000, 5000);
	}

	if (!IS_ERR(gc2145->xvclk)) {
		ret = clk_prepare_enable(gc2145->xvclk);
		if (ret < 0)
			dev_info(dev, "Failed to enable xvclk\n");
	}
	usleep_range(7000, 10000);

	return 0;
}

static void __gc2145_power_off(struct gc2145 *gc2145)
{
	if (!IS_ERR(gc2145->xvclk))
		clk_disable_unprepare(gc2145->xvclk);
	if (!IS_ERR(gc2145->supplies))
		regulator_bulk_disable(GC2145_NUM_SUPPLIES, gc2145->supplies);
	if (!IS_ERR(gc2145->pwdn_gpio))
		gpiod_set_value_cansleep(gc2145->pwdn_gpio, 1);
}

static int gc2145_configure_regulators(struct gc2145 *gc2145)
{
	unsigned int i;

	for (i = 0; i < GC2145_NUM_SUPPLIES; i++)
		gc2145->supplies[i].supply = gc2145_supply_names[i];

	return devm_regulator_bulk_get(&gc2145->client->dev,
				       GC2145_NUM_SUPPLIES,
				       gc2145->supplies);
}

static int gc2145_parse_of(struct gc2145 *gc2145)
{
	struct device *dev = &gc2145->client->dev;
	int ret;

	gc2145->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc2145->pwdn_gpio))
		dev_info(dev, "Failed to get pwdn-gpios, maybe no use\n");

	ret = gc2145_configure_regulators(gc2145);
	if (ret)
		dev_info(dev, "Failed to get power regulators\n");

	return __gc2145_power_on(gc2145);
}

static int gc2145_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2145 *gc2145;
	int ret;

	gc2145 = devm_kzalloc(&client->dev, sizeof(*gc2145), GFP_KERNEL);
	if (!gc2145)
		return -ENOMEM;

	gc2145->client = client;
	gc2145->xvclk = devm_clk_get(&client->dev, "xvclk");
	if (IS_ERR(gc2145->xvclk)) {
		dev_err(&client->dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc2145_parse_of(gc2145);

	gc2145->xvclk_frequency = clk_get_rate(gc2145->xvclk);
	if (gc2145->xvclk_frequency < 6000000 ||
	    gc2145->xvclk_frequency > 27000000)
		return -EINVAL;

	v4l2_ctrl_handler_init(&gc2145->ctrls, 2);
	gc2145->link_frequency =
			v4l2_ctrl_new_std(&gc2145->ctrls, &gc2145_ctrl_ops,
					  V4L2_CID_PIXEL_RATE, 0,
					  GC2145_PIXEL_RATE, 1,
					  GC2145_PIXEL_RATE);

	v4l2_ctrl_new_std_menu_items(&gc2145->ctrls, &gc2145_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(gc2145_test_pattern_menu) - 1,
				     0, 0, gc2145_test_pattern_menu);
	gc2145->sd.ctrl_handler = &gc2145->ctrls;

	if (gc2145->ctrls.error) {
		dev_err(&client->dev, "%s: control initialization error %d\n",
			__func__, gc2145->ctrls.error);
		return  gc2145->ctrls.error;
	}

	sd = &gc2145->sd;
	client->flags |= I2C_CLIENT_SCCB;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	v4l2_i2c_subdev_init(sd, client, &gc2145_subdev_ops);

	sd->internal_ops = &gc2145_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	gc2145->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	ret = media_entity_init(&sd->entity, 1, &gc2145->pad, 0);
	if (ret < 0) {
		v4l2_ctrl_handler_free(&gc2145->ctrls);
		return ret;
	}
#endif

	mutex_init(&gc2145->lock);

	gc2145_get_default_format(&gc2145->format);
	gc2145->frame_size = &gc2145_framesizes[0];
	gc2145->format.width = gc2145_framesizes[0].width;
	gc2145->format.height = gc2145_framesizes[0].height;
	gc2145->fps = gc2145_framesizes[0].fps;

	ret = gc2145_detect(gc2145);
	if (ret < 0)
		goto error;

	ret = v4l2_async_register_subdev(&gc2145->sd);
	if (ret)
		goto error;

	dev_info(&client->dev, "%s sensor driver registered !!\n", sd->name);

	return 0;

error:
	v4l2_ctrl_handler_free(&gc2145->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	mutex_destroy(&gc2145->lock);
	__gc2145_power_off(gc2145);
	return ret;
}

static int gc2145_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145 *gc2145 = to_gc2145(sd);

	v4l2_ctrl_handler_free(&gc2145->ctrls);
	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	mutex_destroy(&gc2145->lock);

	__gc2145_power_off(gc2145);

	return 0;
}

static const struct i2c_device_id gc2145_id[] = {
	{ "gc2145", 0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(i2c, gc2145_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc2145_of_match[] = {
	{ .compatible = "galaxycore,gc2145", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gc2145_of_match);
#endif

static struct i2c_driver gc2145_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(gc2145_of_match),
	},
	.probe		= gc2145_probe,
	.remove		= gc2145_remove,
	.id_table	= gc2145_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc2145_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc2145_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_AUTHOR("Benoit Parrot <bparrot@ti.com>");
MODULE_DESCRIPTION("GC2145 CMOS Image Sensor driver");
MODULE_LICENSE("GPL v2");
