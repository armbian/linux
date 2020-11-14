/* drivers/video/rockchip/chips/rk29_fb.h
 *
 * Copyright (C) 2010 ROCKCHIP, Inc.
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

#ifndef __RK3066B_LCDC_H
#define __RK3066B_LCDC_H

#include<linux/rk_fb.h>

#define LcdReadBit(inf, addr, msk)      ((inf->regbak.addr=inf->preg->addr)&(msk))
#define LcdWrReg(inf, addr, val)        inf->preg->addr=inf->regbak.addr=(val)
#define LcdRdReg(inf, addr)             (inf->preg->addr)
#define LcdSetBit(inf, addr, msk)       inf->preg->addr=((inf->regbak.addr) |= (msk))
#define LcdClrBit(inf, addr, msk)       inf->preg->addr=((inf->regbak.addr) &= ~(msk))
#define LcdSetRegBit(inf, addr, msk)    inf->preg->addr=((inf->preg->addr) |= (msk))
#define LcdMskReg(inf, addr, msk, val)  (inf->regbak.addr)&=~(msk);   inf->preg->addr=(inf->regbak.addr|=(val))
#define LCDC_REG_CFG_DONE()		LcdWrReg(lcdc_dev, REG_CFG_DONE, 0x01); dsb()


/********************************************************************
**                            �궨��                                *
********************************************************************/
/* SYS_CONFIG */
#define m_W2_FORMAT          (3<<0)
#define m_W1_FORMAT          (1<<2)
#define m_W0_FORMAT          (7<<3)
#define m_W0_CBR_DEFLICK_EN  (1<<6)
#define m_W0_YRGB_DEFLICK_EN  (1<<7)
#define m_INTERIACE_EN       (1<<8)
#define m_W2_EN              (1<<9)
#define m_W1_EN              (1<<10)
#define m_W0_EN              (1<<11)
#define m_HWC_EN              (1<<12)
#define m_HWC_RELOAD_EN         (1<<13)
#define m_W2_INTERLACE_READ    (1<<14)
#define m_W1_INTERLACE_READ    (1<<15)
#define m_W0_INTERLACE_READ    (1<<16)
#define m_LCDC_STANDBY            (1<<17)
#define m_HWC_BURST          (3<<18)
#define m_W2_BURST           (3<<20)
#define m_W1_BURST           (3<<22)
#define m_W0_BURST           (3<<24)
#define m_W2_LUT_CTL         (1<<26)
#define m_DSIP_LUT_CTL       (1<<27)
#define m_HWC_REVERSED_COLOR   (1<<28)
#define m_W1_AXI_OUTSTANDING2     (1<<29)
#define m_W0_AXI_OUTSTANDING2     (1<<30)
#define m_LCDC_AXICLK_AUTO_ENABLE (1<<31)

#define v_W2_FORMAT(x)         (((x)&3)<<0)
#define v_W1_FORMAT(x)          (((x)&1)<<2)
#define v_W0_FORMAT(x)          (((x)&7)<<3)
#define v_W0_CBR_DEFLICK_EN(x)  (((x)&1)<<6)
#define v_W0_YRGB_DEFLICK_EN(x)  (((x)&1)<<7)
#define v_INTERIACE_EN(x)       (((x)&1)<<8)
#define v_W2_EN(x)          (((x)&)1<<9)
#define v_W1_EN(x)          (((x)&1)<<10)
#define v_W0_EN(x)          (((x)&1)<<11)
#define v_HWC_EN(x)         (((x)&1)<<12)
#define v_HWC_RELOAD_EN(x)         (((x)&1)<<13)
#define v_W2_INTERLACE_READ(x)    (((x)&1)<<14)
#define v_W1_INTERLACE_READ(x)    (((x)&1)<<15)
#define v_W0_INTERLACE_READ(x)    (((x)&1)<<16)
#define v_LCDC_STANDBY(x)            (((x)&1)<<17)
#define v_HWC_BURST(x)          (((x)&3)<<18)
#define v_W2_BURST(x)           (((x)&3)<<20)
#define v_W1_BURST(x)           (((x)&3)<<22)
#define v_W0_BURST(x)           (((x)&3)<<24)
#define v_W2_LUT_CTL(x)         (((x)&1)<<26)
#define v_DSIP_LUT_CTL(x)       (((x)&1)<<27)
#define v_HWC_REVERSED_COLOR(x)   (((x)&1)<<28)
#define v_W1_AXI_OUTSTANDING2(x)     (((x)&1)<<29)
#define v_W0_AXI_OUTSTANDING2(x)     (((x)&1)<<30)
#define v_LCDC_AXICLK_AUTO_ENABLE(x)    (((x)&1)<<31)

//LCDC_SWAP_CTRL
#define m_W1_565_RB_SWAP        (1<<0)
#define m_W0_565_RB_SWAP        (1<<1)
#define m_W0_YRGB_M8_SWAP       (1<<2)
#define m_W0_YRGB_R_SHIFT_SWAP  (1<<3)
#define m_W0_CBR_R_SHIFT_SWAP   (1<<4)
#define m_W0_YRGB_16_SWAP       (1<<5)
#define m_W0_YRGB_8_SWAP        (1<<6)
#define m_W0_CBR_16_SWAP        (1<<7)
#define m_W0_CBR_8_SWAP         (1<<8)
#define m_W1_16_SWAP            (1<<9)
#define m_W1_8_SWAP             (1<<10)
#define m_W1_R_SHIFT_SWAP       (1<<11)
#define m_OUTPUT_BG_SWAP        (1<<12)
#define m_OUTPUT_RB_SWAP        (1<<13)
#define m_OUTPUT_RG_SWAP        (1<<14)
#define m_DELTA_SWAP            (1<<15)
#define m_DUMMY_SWAP            (1<<16)
#define m_W2_BYTE_SWAP          (1<<17)
#define v_W1_565_RB_SWAP(x)        (((x)&1)<<0)
#define v_W0_565_RB_SWAP(x)        (((x)&1)<<1)
#define v_W0_YRGB_M8_SWAP(x)       (((x)&1)<<2)
#define v_W0_YRGB_R_SHIFT_SWAP(x)  (((x)&1)<<3)
#define v_W0_CBR_R_SHIFT_SWAP(x)   (((x)&1)<<4)
#define v_W0_YRGB_16_SWAP(x)       (((x)&1)<<5)
#define v_W0_YRGB_8_SWAP(x)        (((x)&1)<<6)
#define v_W0_CBR_16_SWAP(x)        (((x)&1)<<7)
#define v_W0_CBR_8_SWAP(x)         (((x)&1)<<8)
#define v_W1_16_SWAP(x)            (((x)&1)<<9)
#define v_W1_8_SWAP(x)             (((x)&1)<<10)
#define v_W1_R_SHIFT_SWAP(x)       (((x)&1)<<11)
#define v_OUTPUT_BG_SWAP(x)        (((x)&1)<<12)
#define v_OUTPUT_RB_SWAP(x)        (((x)&1)<<13)
#define v_OUTPUT_RG_SWAP(x)        (((x)&1)<<14)
#define v_DELTA_SWAP(x)            (((x)&1)<<15)
#define v_DUMMY_SWAP(x)            (((x)&1)<<16)
#define v_W2_BYTE_SWAP(x)      (((x)&1)<<17)

//LCDC_MCU_TIMING_CTRL
#define m_MCU_WRITE_PERIOD      (31<<0)
#define m_MCU_CS_ST             (31<<5)
#define m_MCU_CS_END            (31<<10)
#define m_MCU_RW_ST             (31<<15)
#define m_MCU_RW_END            (31<<20)
#define m_MCU_HOLDMODE_SELECT     (1<<27)
#define m_MCU_HOLDMODE_FRAME_ST   (1<<28)
#define m_MCU_RS_SELECT            (1<<29)
#define m_MCU_BYPASSMODE_SELECT   (1<<30)
#define m_MCU_OUTPUT_SELECT        (1<<31)
#define v_MCU_WRITE_PERIOD(x)      (((x)&31)<<0)
#define v_MCU_CS_ST(x)          (((x)&31)<<5)
#define v_MCU_CS_END(x)         (((x)&31)<<10)
#define v_MCU_RW_ST(x)          (((x)&31)<<15)
#define v_MCU_RW_END(x)         (((x)&31)<<20)
#define v_MCU_HOLD_STATUS(x)          (((x)&1)<<26)
#define v_MCU_HOLDMODE_SELECT(x)     (((x)&1)<<27)
#define v_MCU_HOLDMODE_FRAME_ST(x)   (((x)&1)<<28)
#define v_MCU_RS_SELECT(x)            (((x)&1)<<29)
#define v_MCU_BYPASSMODE_SELECT(x)   (((x)&1)<<30)
#define v_MCU_OUTPUT_SELECT(x)        (((x)&1)<<31)

//LCDC_ BLEND_CTRL
#define m_HWC_BLEND_EN         (1<<0)
#define m_W2_BLEND_EN          (1<<1)
#define m_W1_BLEND_EN          (1<<2)
#define m_W0_BLEND_EN          (1<<3)
#define m_HWC_BLEND_FACTOR     (15<<4)
#define m_W2_BLEND_FACTOR     (0xff<<8)
#define m_W1_BLEND_FACTOR     (0xff<<16)
#define m_W0_BLEND_FACTOR     (0xff<<24)

#define v_HWC_BLEND_EN(x)         (((x)&1)<<0)
#define v_W2_BLEND_EN(x)          (((x)&1)<<1)
#define v_W1_BLEND_EN(x)          (((x)&1)<<2)
#define v_W0_BLEND_EN(x)          (((x)&1)<<3)
#define v_HWC_BLEND_FACTOR(x)    (((x)&15)<<4)
#define v_W2_BLEND_FACTOR(x)     (((x)&0xff)<<8)
#define v_W1_BLEND_FACTOR(x)     (((x)&0xff)<<16)
#define v_W0_BLEND_FACTOR(x)     (((x)&0xff)<<24)


//LCDC_WIN0_COLOR_KEY_CTRL / LCDC_WIN1_COLOR_KEY_CTRL
#define m_KEYCOLOR          (0xffffff<<0)
#define m_KEYCOLOR_B          (0xff<<0)
#define m_KEYCOLOR_G          (0xff<<8)
#define m_KEYCOLOR_R          (0xff<<16)
#define m_COLORKEY_EN         (1<<24)
#define v_KEYCOLOR(x)          (((x)&0xffffff)<<0)
#define v_KEYCOLOR_B(x)          (((x)&0xff)<<0)
#define v_KEYCOLOR_G(x)         (((x)&0xff)<<8)
#define v_KEYCOLOR_R(x)          (((x)&0xff)<<16)
#define v_COLORKEY_EN(x)         (((x)&1)<<24)

//LCDC_DEFLICKER_SCL_OFFSET
#define m_W0_YRGB_VSD_OFFSET      (0xff<<0)
#define m_W0_YRGB_VSP_OFFSET      (0xff<<8)
#define m_W1_VSD_OFFSET           (0xff<<16)
#define m_W1_VSP_OFFSET           (0xff<<24)
#define v_W0_YRGB_VSD_OFFSET(x)      (((x)&0xff)<<0)
#define v_W0_YRGB_VSP_OFFSET(x)      (((x)&0xff)<<8)
#define v_W1_VSD_OFFSET(x)           (((x)&0xff)<<16)
#define v_W1_VSP_OFFSET(x)           (((x)&0xff)<<24)

//LCDC_DSP_CTRL_REG0
#define m_DISPLAY_FORMAT             (0xf<<0)
#define m_HSYNC_POLARITY             (1<<4)
#define m_VSYNC_POLARITY             (1<<5)
#define m_DEN_POLARITY               (1<<6)
#define m_DCLK_POLARITY              (1<<7)
#define m_COLOR_SPACE_CONVERSION     (3<<8)
#define m_DITHER_UP_EN               (1<<10)
#define m_DITHER_DOWN_MODE           (1<<11)
#define m_DITHER_DOWN_EN             (1<<12)
#define m_INTERLACE_FIELD_POLARITY   (1<<13)
#define m_YUV_CLIP                   (1<<14)
#define m_W1_TRANSP_FROM             (1<<15)
#define m_W0_TRANSP_FROM             (1<<16)
#define m_W0W1_POSITION_SWAP         (1<<17)
#define m_W1_CLIP_EN                 (1<<18)
#define m_W0_CLIP_EN                 (1<<19)
#define m_W0_YCBR_PRIORITY_MODE      (1<<20)
#define m_CBR_FILTER_656             (1<<21)
#define m_W2_CHIP_EN                 (1<<22)

#define v_DISPLAY_FORMAT(x)            (((x)&0xf)<<0)
#define v_HSYNC_POLARITY(x)             (((x)&1)<<4)
#define v_VSYNC_POLARITY(x)             (((x)&1)<<5)
#define v_DEN_POLARITY(x)               (((x)&1)<<6)
#define v_DCLK_POLARITY(x)              (((x)&1)<<7)
#define v_COLOR_SPACE_CONVERSION(x)     (((x)&3)<<8)
#define v_DITHER_UP_EN(x)               (((x)&1)<<10)
#define v_DITHER_DOWN_MODE(x)           (((x)&1)<<11)
#define v_DITHER_DOWN_EN(x)             (((x)&1)<<12)
#define v_INTERLACE_FIELD_POLARITY(x)   (((x)&1)<<13)
#define v_YUV_CLIP(x)                   (((x)&1)<<14)
#define v_W1_TRANSP_FROM(x)             (((x)&1)<<15)
#define v_W0_TRANSP_FROM(x)             (((x)&1)<<16)
#define v_W0W1_POSITION_SWAP(x)         (((x)&1)<<17)
#define v_W1_CLIP_EN(x)                 (((x)&1)<<18)
#define v_W0_CLIP_EN(x)                 (((x)&1)<<19)
#define v_W0_YCBR_PRIORITY_MODE(x)      (((x)&1)<<20)
#define v_CBR_FILTER_656(x)             (((x)&1)<<21)
#define v_W2_CHIP_EN(x)                 (((x)&1)<<22)


//LCDC_DSP_CTRL_REG1
#define m_BG_COLOR                    (0xffffff<<0)
#define m_BG_B                        (0xff<<0)
#define m_BG_G                        (0xff<<8)
#define m_BG_R                        (0xff<<16)
#define m_BLANK_MODE                  (1<<24)
#define m_BLACK_MODE                  (1<<25)
#define m_DISP_FILTER_FACTOR          (3<<26)
#define m_DISP_FILTER_MODE            (1<<28)
#define m_DISP_FILTER_EN              (1<<29)
#define v_BG_COLOR(x)                 (((x)&0xffffff)<<0)
#define v_BG_B(x)                     (((x)&0xff)<<0)
#define v_BG_G(x)                     (((x)&0xff)<<8)
#define v_BG_R(x)                     (((x)&0xff)<<16)
#define v_BLANK_MODE(x)               (((x)&1)<<24)
#define v_BLACK_MODE(x)               (((x)&1)<<25)
#define v_DISP_FILTER_FACTOR(x)       (((x)&3)<<26)
#define v_DISP_FILTER_MODE(x)         (((x)&1)<<28)
#define v_DISP_FILTER_EN(x)           (((x)&1)<<29)

//LCDC_INT_STATUS
#define m_HOR_START         (1<<0)
#define m_FRM_START         (1<<1)
#define m_SCANNING_FLAG     (1<<2)
#define m_HOR_STARTMASK     (1<<3)
#define m_FRM_STARTMASK     (1<<4)
#define m_SCANNING_MASK     (1<<5)
#define m_HOR_STARTCLEAR    (1<<6)
#define m_FRM_STARTCLEAR    (1<<7)
#define m_SCANNING_CLEAR    (1<<8)
#define m_SCAN_LINE_NUM     (0x7ff<<9)
#define v_HOR_START(x)         (((x)&1)<<0)
#define v_FRM_START(x)         (((x)&1)<<1)
#define v_SCANNING_FLAG(x)     (((x)&1)<<2)
#define v_HOR_STARTMASK(x)     (((x)&1)<<3)
#define v_FRM_STARTMASK(x)     (((x)&1)<<4)
#define v_SCANNING_MASK(x)     (((x)&1)<<5)
#define v_HOR_STARTCLEAR(x)    (((x)&1)<<6)
#define v_FRM_STARTCLEAR(x)    (((x)&1)<<7)
#define v_SCANNING_CLEAR(x)    (((x)&1)<<8)
#define v_SCAN_LINE_NUM(x)     (((x)&0x7ff)<<9)

//AXI MS ID
#define m_W0_YRGB_CH_ID        (0xF<<0)
#define m_W0_CBR_CH_ID         (0xF<<4)
#define m_W1_YRGB_CH_ID        (0xF<<8)
#define m_W2_CH_ID             (0xF<<12)
#define m_HWC_CH_ID            (0xF<<16)
#define v_W0_YRGB_CH_ID(x)        (((x)&0xF)<<0)
#define v_W0_CBR_CH_ID(x)         (((x)&0xF)<<4)
#define v_W1_YRGB_CH_ID(x)        (((x)&0xF)<<8)
#define v_W2_CH_ID(x)             (((x)&0xF)<<12)
#define v_HWC_CH_ID(x)            (((x)&0xF)<<16)


/* Low Bits Mask */
#define m_WORDLO            (0xffff<<0)
#define m_WORDHI            (0xffff<<16)
#define v_WORDLO(x)         (((x)&0xffff)<<0)
#define v_WORDHI(x)         (((x)&0xffff)<<16)

#define m_BIT11LO           (0x7ff<<0)
#define m_BIT11HI           (0x7ff<<16)
#define v_BIT11LO(x)        (((x)&0x7ff)<<0)
#define v_BIT11HI(x)        (((x)&0x7ff)<<16)

#define m_BIT12LO           (0xfff<<0)
#define m_BIT12HI           (0xfff<<16)
#define v_BIT12LO(x)        (((x)&0xfff)<<0)
#define v_BIT12HI(x)        (((x)&0xfff)<<16)


#define m_VIRWIDTH       (0xffff<<0)
#define m_VIRHEIGHT      (0xffff<<16)
#define v_VIRWIDTH(x)       (((x)&0xffff)<<0)
#define v_VIRHEIGHT(x)      (((x)&0xffff)<<16)

#define m_ACTWIDTH       (0xffff<<0)
#define m_ACTHEIGHT      (0xffff<<16)
#define v_ACTWIDTH(x)       (((x)&0xffff)<<0)
#define v_ACTHEIGHT(x)      (((x)&0xffff)<<16)

#define m_VIRST_X      (0xffff<<0)
#define m_VIRST_Y      (0xffff<<16)
#define v_VIRST_X(x)      (((x)&0xffff)<<0)
#define v_VIRST_Y(x)      (((x)&0xffff)<<16)

#define m_PANELST_X      (0x3ff<<0)
#define m_PANELST_Y      (0x3ff<<16)
#define v_PANELST_X(x)      (((x)&0x3ff)<<0)
#define v_PANELST_Y(x)      (((x)&0x3ff)<<16)

#define m_PANELWIDTH       (0x3ff<<0)
#define m_PANELHEIGHT      (0x3ff<<16)
#define v_PANELWIDTH(x)       (((x)&0x3ff)<<0)
#define v_PANELHEIGHT(x)      (((x)&0x3ff)<<16)

#define m_HWC_B                 (0xff<<0)
#define m_HWC_G                 (0xff<<8)
#define m_HWC_R                 (0xff<<16)
#define m_W0_YRGB_HSP_OFFSET    (0xff<<24)
#define m_W0_YRGB_HSD_OFFSET    (0xff<<24)
#define v_HWC_B(x)                 (((x)&0xff)<<0)
#define v_HWC_G(x)                 (((x)&0xff)<<8)
#define v_HWC_R(x)                 (((x)&0xff)<<16)
#define v_W0_YRGB_HSP_OFFSET(x)    (((x)&0xff)<<24)
#define v_W0_YRGB_HSD_OFFSET(x)    (((x)&0xff)<<24)


//Panel display scanning
#define m_PANEL_HSYNC_WIDTH             (0x3ff<<0)
#define m_PANEL_HORIZONTAL_PERIOD       (0x3ff<<16)
#define v_PANEL_HSYNC_WIDTH(x)             (((x)&0x3ff)<<0)
#define v_PANEL_HORIZONTAL_PERIOD(x)       (((x)&0x3ff)<<16)

#define m_PANEL_END              (0x3ff<<0)
#define m_PANEL_START            (0x3ff<<16)
#define v_PANEL_END(x)              (((x)&0x3ff)<<0)
#define v_PANEL_START(x)            (((x)&0x3ff)<<16)

#define m_PANEL_VSYNC_WIDTH             (0x3ff<<0)
#define m_PANEL_VERTICAL_PERIOD       (0x3ff<<16)
#define v_PANEL_VSYNC_WIDTH(x)             (((x)&0x3ff)<<0)
#define v_PANEL_VERTICAL_PERIOD(x)       (((x)&0x3ff)<<16)
//-----------

#define m_HSCALE_FACTOR        (0xffff<<0)
#define m_VSCALE_FACTOR        (0xffff<<16)
#define v_HSCALE_FACTOR(x)        (((x)&0xffff)<<0)
#define v_VSCALE_FACTOR(x)        (((x)&0xffff)<<16)

#define m_W0_CBR_HSD_OFFSET   (0xff<<0)
#define m_W0_CBR_HSP_OFFSET   (0xff<<8)
#define m_W0_CBR_VSD_OFFSET   (0xff<<16)
#define m_W0_CBR_VSP_OFFSET   (0xff<<24)
#define v_W0_CBR_HSD_OFFSET(x)   (((x)&0xff)<<0)
#define v_W0_CBR_HSP_OFFSET(x)   (((x)&0xff)<<8)
#define v_W0_CBR_VSD_OFFSET(x)   (((x)&0xff)<<16)
#define v_W0_CBR_VSP_OFFSET(x)   (((x)&0xff)<<24)


#define m_WIN1_FIFO_FULL_LEVEL 		(0x7f << 0)
#define m_WIN2_FIFO_FULL_LEVEL 		(0x1f << 7)
#define v_WIN1_FIFO_FULL_LEVEL(x)	(((x)&0x7f) << 0)
#define v_WIN2_FIFO_FULL_LEVEL(x)	(((x)&0x1f) << 7)


#define m_WIN0_YRGB_CHANNEL_ID	      	((0x0f)<<0)
#define m_WIN0_CBR_CHANNEL_ID	      	((0x0f)<<4)
#define m_WIN1_YRGB_CHANNEL_ID	      	((0x0f)<<8)
#define m_WIN2_CHANNEL_ID	      	((0x0f)<<12)
#define m_HWC_CHANNEL_ID	      	((0x0f)<<16)
#define v_WIN0_YRGB_CHANNEL_ID(x)	(((x)&0x0f)<<0)
#define v_WIN0_CBR_CHANNEL_ID(x)	(((x)&0x0f)<<4)
#define v_WIN1_YRGB_CHANNEL_ID(x)	(((x)&0x0f)<<8)
#define v_WIN2_CHANNEL_ID(x)	      	(((x)&0x0f)<<12)
#define v_HWC_CHANNEL_ID(x)	      	(((x)&0x0f)<<16)


//LCDC_WINx_SCL_FACTOR_Y/CBCR
#define v_X_SCL_FACTOR(x)  ((x)<<0)
#define v_Y_SCL_FACTOR(x)  ((x)<<16)

//LCDC_DSP_HTOTAL_HS_END
#define v_HSYNC(x)  ((x)<<0)   //hsync pulse width
#define v_HORPRD(x) ((x)<<16)   //horizontal period


//LCDC_DSP_HACT_ST_END
#define v_HAEP(x) ((x)<<0)  //horizontal active end point
#define v_HASP(x) ((x)<<16) //horizontal active start point

//LCDC_DSP_VTOTAL_VS_END
#define v_VSYNC(x) ((x)<<0)
#define v_VERPRD(x) ((x)<<16)

//LCDC_DSP_VACT_ST_END
#define v_VAEP(x) ((x)<<0)
#define v_VASP(x) ((x)<<16)

//LCDC_WIN0_ACT_INFO
#define v_ACT_WIDTH(x)     ((x)<<0)
#define v_ACT_HEIGHT(x)    ((x)<<16)

//LCDC_WIN0_DSP_INFO
#define v_DSP_WIDTH(x)     ((x)<<0)
#define v_DSP_HEIGHT(x)    ((x)<<16)

//LCDC_WIN0_DSP_ST    //x,y start point of the panel scanning
#define v_DSP_STX(x)      (x<<0)
#define v_DSP_STY(x)      (x<<16)


/********************************************************************
**                          �ṹ����                                *
********************************************************************/
/* LCDC�ļĴ����ṹ */

typedef volatile struct tagLCDC_REG
{
    /* offset 0x00~0xc0 */
    unsigned int SYS_CFG;              //0x00 SYSTEM configure register
    unsigned int SWAP_CTRL;               //0x04 Data SWAP control
    unsigned int MCU_CTRL;         //0x08 MCU TIMING control register
    unsigned int BLEND_CTRL;              //0x0c Blending control register
    unsigned int WIN0_COLOR_KEY_CTRL;     //0x10 Win0 blending control register
    unsigned int WIN1_COLOR_KEY_CTRL;     //0x14 Win1 blending control register
    unsigned int WIN2_VIR;                //0x18 WIN2 virtual display width
    unsigned int DSP_CTRL0;               //0x1c Display control register0
    unsigned int DSP_CTRL1;               //0x20 Display control register1
    unsigned int INT_STATUS;              //0x24 Interrupt status register
    unsigned int WIN0_VIR;                //0x28 WIN0 virtual display width/height
    unsigned int WIN0_YRGB_MST;           //0x2c Win0 active YRGB memory start address
    unsigned int WIN0_CBR_MST;            //0x30 Win0 active Cbr memory start address
    unsigned int WIN0_ACT_INFO;           //0x34 Win0 active window width/height
    unsigned int WIN0_DSP_ST;             //0x38 Win0 display start point on panel
    unsigned int WIN0_DSP_INFO;           //0x3c Win0 display width/height on panel
    unsigned int WIN1_VIR;                //0x40 Win1 virtual display width/height
    unsigned int WIN1_YRGB_MST;           //0x44 Win1 active  memory start address
    unsigned int WIN1_DSP_INFO;           //0x48 Win1 display width/height on panel
    unsigned int WIN1_DSP_ST;             //0x4c Win1 display start point on panel
    unsigned int WIN2_MST;                //0X50 Win2 memory start address
    unsigned int WIN2_DSP_INFO;           //0x54 Win1 display width/height on panel
    unsigned int WIN2_DSP_ST;             //0x58 Win1 display start point on panel
    unsigned int HWC_MST;                 //0x5C HWC memory start address
    unsigned int HWC_DSP_ST;              //0x60 HWC display start point on panel
    unsigned int HWC_COLOR_LUT0;          //0x64 Hardware cursor color 2��b01 look up table 0
    unsigned int HWC_COLOR_LUT1;          //0x68 Hardware cursor color 2��b10 look up table 1
    unsigned int HWC_COLOR_LUT2;          //0x6c Hardware cursor color 2��b11 look up table 2
    unsigned int DSP_HTOTAL_HS_END;       //0x70 Panel scanning horizontal width and hsync pulse end point
    unsigned int DSP_HACT_ST_END;         //0x74 Panel active horizontal scanning start/end point
    unsigned int DSP_VTOTAL_VS_END;       //0x78 Panel scanning vertical height and vsync pulse end point
    unsigned int DSP_VACT_ST_END;         //0x7c Panel active vertical scanning start/end point
    unsigned int DSP_VS_ST_END_F1;        //0x80 Vertical scanning start point and vsync pulse end point of even filed in interlace mode
    unsigned int DSP_VACT_ST_END_F1;      //0x84 Vertical scanning active start/end point of even filed in interlace mode
    unsigned int WIN0_SCL_FACTOR_YRGB;    //0x88 Win0 YRGB scaling down factor setting
    unsigned int WIN0_SCL_FACTOR_CBR;     //0x8c Win0 YRGB scaling up factor setting
    unsigned int WIN0_SCL_OFFSET;         //0x90 Win0 Cbr scaling start point offset
    unsigned int FIFO_WATER_MARK;         //0x94 Fifo water mark
    unsigned int AXI_MS_ID;               //0x98 Axi master ID
    unsigned int reserved0;               //0x9c
    unsigned int REG_CFG_DONE;            //0xa0 REGISTER CONFIG FINISH
    unsigned int reserved1[(0x100-0xa4)/4];
    unsigned int MCU_BYPASS_WPORT;         //0x100 MCU BYPASS MODE, DATA Write Only Port
    unsigned int reserved2[(0x200-0x104)/4];
    unsigned int MCU_BYPASS_RPORT;         //0x200 MCU BYPASS MODE, DATA Read Only Port
} LCDC_REG, *pLCDC_REG;

#define Win2_LUT_ADDR 0x400
#define DSP_LUT_ADDR  0x800

//roate
#define 	ROTATE_0		0
#define 	ROTATE_90		90
#define 	ROTATE_180		180
#define 	ROTATE_270		270
#define 	X_MIRROR		(1<<10)
#define		Y_MIRROR		(1<<11)




#define CalScale(x, y)	             (((u32)x*0x1000)/y)
struct rk3066b_lcdc_device{
	int id;
	struct rk_lcdc_device_driver driver;
	rk_screen *screen;
	
	LCDC_REG *preg;         // LCDC reg base address and backup reg 
    	LCDC_REG regbak;

	void __iomem *reg_vir_base;  	// virtual basic address of lcdc register
	u32 reg_phy_base;       	// physical basic address of lcdc register
	u32 len;               		// physical map length of lcdc register
	spinlock_t  reg_lock;		//one time only one process allowed to config the register
	int __iomem *dsp_lut_addr_base;
	bool clk_on;			//if aclk or hclk is closed ,acess to register is not allowed
	u8 atv_layer_cnt;		//active layer counter,when  atv_layer_cnt = 0,disable lcdc

	unsigned int		irq;

	struct clk		*pd;				//lcdc power domain
	struct clk		*hclk;				//lcdc AHP clk
	struct clk		*dclk;				//lcdc dclk
	struct clk		*aclk;				//lcdc share memory frequency
	struct clk		*aclk_parent;		//lcdc aclk divider frequency source
	struct clk		*aclk_ddr_lcdc; 	//DDR LCDC AXI clock disable.
	struct clk		*aclk_disp_matrix;	//DISPLAY matrix AXI clock disable.
	struct clk		*hclk_cpu_display;	//CPU DISPLAY AHB bus clock disable.
	struct clk		*pd_display;		// display power domain
	u32	pixclock;
};

struct lcdc_info{
/*LCD CLK*/
	struct rk3066b_lcdc_device lcdc0;
	struct rk3066b_lcdc_device lcdc1;

};


struct win_set {
	volatile u32 y_offset;
	volatile u32 c_offset;
};

struct win0_par {
    u32 refcount;
    u32	pseudo_pal[16];
    u32 y_offset;
    u32 c_offset;
    u32 xpos;         //size in panel
    u32 ypos;
    u32 xsize;        //start point in panel
    u32 ysize;
    enum data_format format;

    wait_queue_head_t wait;
    struct win_set mirror;
    struct win_set displ;
    struct win_set done;

    u8 par_seted;
    u8 addr_seted;
};

#endif
