/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip CIF Driver
 *
 * Copyright (C) 2018 Rockchip Electronics Co., Ltd.
 */

#ifndef _RKCIF_REGS_H
#define _RKCIF_REGS_H

/* CIF Reg Offset */
#define CIF_CTRL			0x00
#define CIF_INTEN			0x04
#define CIF_INTSTAT			0x08
#define CIF_FOR				0x0c
#define CIF_LINE_NUM_ADDR		0x10
#define CIF_FRM0_ADDR_Y			0x14
#define CIF_FRM0_ADDR_UV		0x18
#define CIF_FRM1_ADDR_Y			0x1c
#define CIF_FRM1_ADDR_UV		0x20
#define CIF_VIR_LINE_WIDTH		0x24
#define CIF_SET_SIZE			0x28
#define CIF_SCM_ADDR_Y			0x2c
#define CIF_SCM_ADDR_U			0x30
#define CIF_SCM_ADDR_V			0x34
#define CIF_WB_UP_FILTER		0x38
#define CIF_WB_LOW_FILTER		0x3c
#define CIF_WBC_CNT			0x40
#define CIF_CROP			0x44
#define CIF_SCL_CTRL			0x48
#define CIF_SCL_DST			0x4c
#define CIF_SCL_FCT			0x50
#define CIF_SCL_VALID_NUM		0x54
#define CIF_LINE_LOOP_CTR		0x58
#define CIF_FRAME_STATUS		0x60
#define CIF_CUR_DST			0x64
#define CIF_LAST_LINE			0x68
#define CIF_LAST_PIX			0x6c

/* The key register bit description */

/* CIF_CTRL Reg */
#define DISABLE_CAPTURE			(0x0 << 0)
#define ENABLE_CAPTURE			(0x1 << 0)
#define MODE_ONEFRAME			(0x0 << 1)
#define MODE_PINGPONG			(0x1 << 1)
#define MODE_LINELOOP			(0x2 << 1)
#define AXI_BURST_16			(0xF << 12)

/* CIF_INTEN */
#define INTEN_DISABLE			(0x0 << 0)
#define FRAME_END_EN			(0x1 << 0)
#define BUS_ERR_EN			(0x1 << 6)
#define SCL_ERR_EN			(0x1 << 7)
#define PST_INF_FRAME_END_EN		(0x1 << 9)

/* CIF INTSTAT */
#define INTSTAT_CLS			(0x3FF)
#define FRAME_END			(0x01 << 0)
#define PST_INF_FRAME_END		(0x01 << 9)
#define FRAME_END_CLR			(0x01 << 0)
#define PST_INF_FRAME_END_CLR		(0x01 << 9)
#define INTSTAT_ERR			(0xFC)

/* FRAME STATUS */
#define FRAME_STAT_CLS			0x00
#define FRM0_STAT_CLS			0x20	/* write 0 to clear frame 0 */

/* CIF FORMAT */
#define VSY_HIGH_ACTIVE			(0x01 << 0)
#define VSY_LOW_ACTIVE			(0x00 << 0)
#define HSY_LOW_ACTIVE			(0x01 << 1)
#define HSY_HIGH_ACTIVE			(0x00 << 1)
#define INPUT_MODE_YUV			(0x00 << 2)
#define INPUT_MODE_PAL			(0x02 << 2)
#define INPUT_MODE_NTSC			(0x03 << 2)
#define INPUT_MODE_RAW			(0x04 << 2)
#define INPUT_MODE_JPEG			(0x05 << 2)
#define INPUT_MODE_MIPI			(0x06 << 2)
#define YUV_INPUT_ORDER_UYVY		(0x00 << 5)
#define YUV_INPUT_ORDER_YVYU		(0x01 << 5)
#define YUV_INPUT_ORDER_VYUY		(0x10 << 5)
#define YUV_INPUT_ORDER_YUYV		(0x03 << 5)
#define YUV_INPUT_422			(0x00 << 7)
#define YUV_INPUT_420			(0x01 << 7)
#define INPUT_420_ORDER_EVEN		(0x00 << 8)
#define INPUT_420_ORDER_ODD		(0x01 << 8)
#define CCIR_INPUT_ORDER_ODD		(0x00 << 9)
#define CCIR_INPUT_ORDER_EVEN		(0x01 << 9)
#define RAW_DATA_WIDTH_8		(0x00 << 11)
#define RAW_DATA_WIDTH_10		(0x01 << 11)
#define RAW_DATA_WIDTH_12		(0x02 << 11)
#define YUV_OUTPUT_422			(0x00 << 16)
#define YUV_OUTPUT_420			(0x01 << 16)
#define OUTPUT_420_ORDER_EVEN		(0x00 << 17)
#define OUTPUT_420_ORDER_ODD		(0x01 << 17)
#define RAWD_DATA_LITTLE_ENDIAN		(0x00 << 18)
#define RAWD_DATA_BIG_ENDIAN		(0x01 << 18)
#define UV_STORAGE_ORDER_UVUV		(0x00 << 19)
#define UV_STORAGE_ORDER_VUVU		(0x01 << 19)

/* CIF_SCL_CTRL */
#define ENABLE_SCL_DOWN			(0x01 << 0)
#define DISABLE_SCL_DOWN		(0x00 << 0)
#define ENABLE_SCL_UP			(0x01 << 1)
#define DISABLE_SCL_UP			(0x00 << 1)
#define ENABLE_YUV_16BIT_BYPASS		(0x01 << 4)
#define DISABLE_YUV_16BIT_BYPASS	(0x00 << 4)
#define ENABLE_RAW_16BIT_BYPASS		(0x01 << 5)
#define DISABLE_RAW_16BIT_BYPASS	(0x00 << 5)
#define ENABLE_32BIT_BYPASS		(0x01 << 6)
#define DISABLE_32BIT_BYPASS		(0x00 << 6)

/* CIF_INTSTAT */
#define CIF_F0_READY			(0x01 << 0)
#define CIF_F1_READY			(0x01 << 1)

/* CIF CROP */
#define CIF_CROP_Y_SHIFT		16
#define CIF_CROP_X_SHIFT		0

#endif
