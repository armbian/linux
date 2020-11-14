/* drivers/video/rk_fb.h
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

#ifndef __ARCH_ARM_MACH_RK30_FB_H
#define __ARCH_ARM_MACH_RK30_FB_H

#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>
#include <linux/rk_screen.h>
#if defined(CONFIG_OF)
#include <dt-bindings/display/rk_fb.h>
#endif
#include "../../drivers/staging/android/sw_sync.h"
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>


#define RK30_MAX_LCDC_SUPPORT	2
#define RK30_MAX_LAYER_SUPPORT	5
#define RK_MAX_FB_SUPPORT       5
#define RK_WIN_MAX_AREA		4
#define RK_MAX_BUF_NUM		11

#define FB0_IOCTL_STOP_TIMER_FLUSH		0x6001
#define FB0_IOCTL_SET_PANEL				0x6002

#ifdef CONFIG_FB_WIMO
#define FB_WIMO_FLAG
#endif
#ifdef FB_WIMO_FLAG
#define FB0_IOCTL_SET_BUF				0x6017
#define FB0_IOCTL_COPY_CURBUF				0x6018
#define FB0_IOCTL_CLOSE_BUF				0x6019
#endif

#define RK_FBIOGET_PANEL_SIZE				0x5001
#define RK_FBIOSET_YUV_ADDR				0x5002
#define RK_FBIOGET_SCREEN_STATE    			0X4620
#define RK_FBIOGET_16OR32    				0X4621
#define RK_FBIOGET_IDLEFBUff_16OR32			0X4622
#define RK_FBIOSET_COMPOSE_LAYER_COUNTS    		0X4623
#define RK_FBIOSET_HWC_ADDR				0x4624

#define RK_FBIOGET_DMABUF_FD            		0x5003
#define RK_FBIOSET_DMABUF_FD				0x5004
#define RK_FB_IOCTL_SET_I2P_ODD_ADDR       		0x5005
#define RK_FB_IOCTL_SET_I2P_EVEN_ADDR      		0x5006
#define RK_FBIOSET_OVERLAY_STA    			0x5018
#define RK_FBIOGET_OVERLAY_STA   			0X4619
#define RK_FBIOSET_ENABLE				0x5019
#define RK_FBIOGET_ENABLE				0x5020
#define RK_FBIOSET_CONFIG_DONE				0x4628
#define RK_FBIOSET_VSYNC_ENABLE				0x4629
#define RK_FBIOPUT_NUM_BUFFERS				0x4625
#define RK_FBIOPUT_COLOR_KEY_CFG			0x4626
#define RK_FBIOGET_DSP_ADDR     			0x4630
#define RK_FBIOGET_LIST_STA  				0X4631
#define RK_FBIOGET_IOMMU_STA				0x4632
#define RK_FBIOSET_CLEAR_FB				0x4633


/**rk fb events**/
#define RK_LF_STATUS_FC                  0xef
#define RK_LF_STATUS_FR                  0xee
#define RK_LF_STATUS_NC                  0xfe
#define RK_LF_MAX_TIMEOUT 			 (1600000UL << 6)	//>0.64s

/**
* pixel align value for gpu,align as 64 bytes in an odd number of times
*/
#define ALIGN_PIXEL_64BYTE_RGB565		32	/* 64/2*/
#define ALIGN_PIXEL_64BYTE_RGB8888		16	/* 64/4*/
#define ALIGN_N_TIMES(x, align)			(((x) % (align) == 0) ? (x) : (((x) + ((align) - 1)) & (~((align) - 1))))
#define ALIGN_ODD_TIMES(x, align)		(((x) % ((align) * 2) == 0) ? ((x) + (align)) : (x))
#define ALIGN_64BYTE_ODD_TIMES(x, align)	ALIGN_ODD_TIMES(ALIGN_N_TIMES(x, align), align)

#define DUMP_FRAME_NUM 3

//#define USE_ION_MMU 1
#if defined(CONFIG_ION_ROCKCHIP)
extern struct ion_client *rockchip_ion_client_create(const char *name);
#endif

extern int rk_fb_poll_prmry_screen_vblank(void);
extern u32 rk_fb_get_prmry_screen_ft(void);
extern u32 rk_fb_get_prmry_screen_vbt(void);
extern u64 rk_fb_get_prmry_screen_framedone_t(void);
extern int rk_fb_set_prmry_screen_status(int status);
extern bool rk_fb_poll_wait_frame_complete(void);

enum {
	CSC_BT601,
	CSC_BT709,
	CSC_BT2020,
};
#define CSC_SHIFT	6
#define CSC_MASK	(0x3 << CSC_SHIFT)
#define CSC_FORMAT(x)	(((x) & CSC_MASK) >> CSC_SHIFT)

#define BT601(x)	((CSC_BT601 << CSC_SHIFT) | ((x) & ~CSC_MASK))
#define BT709(x)	((CSC_BT709 << CSC_SHIFT) | ((x) & ~CSC_MASK))
#define BT2020(x)	((CSC_BT2020 << CSC_SHIFT) | ((x) & ~CSC_MASK))

enum {
	SDR_DATA,
	HDR_DATA,
};

/**
 * pixel format definitions,this is copy from android/system/core/include/system/graphics.h
 */
enum {
	HAL_PIXEL_FORMAT_RGBA_8888 = 1,
	HAL_PIXEL_FORMAT_RGBX_8888 = 2,
	HAL_PIXEL_FORMAT_RGB_888 = 3,
	HAL_PIXEL_FORMAT_RGB_565 = 4,
	HAL_PIXEL_FORMAT_BGRA_8888 = 5,
	HAL_PIXEL_FORMAT_RGBA_5551 = 6,
	HAL_PIXEL_FORMAT_RGBA_4444 = 7,

	/* 0x8 - 0xFF range unavailable */

	/*
	 * 0x100 - 0x1FF
	 *
	 * This range is reserved for pixel formats that are specific to the HAL
	 * implementation.  Implementations can use any value in this range to
	 * communicate video pixel formats between their HAL modules.  These formats
	 * must not have an alpha channel.  Additionally, an EGLimage created from a
	 * gralloc buffer of one of these formats must be supported for use with the
	 * GL_OES_EGL_image_external OpenGL ES extension.
	 */

	/*
	 * Android YUV format:
	 *
	 * This format is exposed outside of the HAL to software decoders and
	 * applications.  EGLImageKHR must support it in conjunction with the
	 * OES_EGL_image_external extension.
	 *
	 * YV12 is a 4:2:0 YCrCb planar format comprised of a WxH Y plane followed
	 * by (W/2) x (H/2) Cr and Cb planes.
	 *
	 * This format assumes
	 * - an even width
	 * - an even height
	 * - a horizontal stride multiple of 16 pixels
	 * - a vertical stride equal to the height
	 *
	 *   y_size = stride * height
	 *   c_size = ALIGN(stride/2, 16) * height/2
	 *   size = y_size + c_size * 2
	 *   cr_offset = y_size
	 *   cb_offset = y_size + c_size
	 *
	 */
	HAL_PIXEL_FORMAT_YV12 = 0x32315659,	// YCrCb 4:2:0 Planar

	/* Legacy formats (deprecated), used by ImageFormat.java */

	/*
	 * YCbCr format default is BT601.
	 */
	HAL_PIXEL_FORMAT_YCbCr_422_SP = 0x10,	// NV16
	HAL_PIXEL_FORMAT_YCrCb_420_SP = 0x11,	// NV21
	HAL_PIXEL_FORMAT_YCbCr_422_I = 0x14,	// YUY2
	HAL_PIXEL_FORMAT_YCrCb_NV12 = 0x20,	// YUY2
	HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO = 0x21,	// YUY2
	
	HAL_PIXEL_FORMAT_YCrCb_NV12_10	    = 0x22, // YUV420_1obit
	HAL_PIXEL_FORMAT_YCbCr_422_SP_10	= 0x23, // YUV422_1obit
	HAL_PIXEL_FORMAT_YCrCb_444_SP_10	= 0x24, //YUV444_1obit

	HAL_PIXEL_FORMAT_YCrCb_444 = 0x25,	//yuv444
	HAL_PIXEL_FORMAT_FBDC_RGB565	= 0x26,
	HAL_PIXEL_FORMAT_FBDC_U8U8U8U8	= 0x27, /*ARGB888*/
	HAL_PIXEL_FORMAT_FBDC_U8U8U8	= 0x28, /*RGBP888*/
	HAL_PIXEL_FORMAT_FBDC_RGBA888	= 0x29, /*ABGR888*/
	HAL_PIXEL_FORMAT_BGRX_8888 = 0x30,
	HAL_PIXEL_FORMAT_BGR_888 = 0x31,
	HAL_PIXEL_FORMAT_BGR_565 = 0x32,

	HAL_PIXEL_FORMAT_YUYV422 = 0x33,
	HAL_PIXEL_FORMAT_YUYV420 = 0x34,
	HAL_PIXEL_FORMAT_UYVY422 = 0x35,
	HAL_PIXEL_FORMAT_UYVY420 = 0x36,

	HAL_PIXEL_FORMAT_YCrCb_NV12_BT709 =
			BT709(HAL_PIXEL_FORMAT_YCrCb_NV12),
	HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO_BT709 =
			BT709(HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO),
	HAL_PIXEL_FORMAT_YCbCr_422_SP_BT709 =
			BT709(HAL_PIXEL_FORMAT_YCbCr_422_SP),
	HAL_PIXEL_FORMAT_YCrCb_444_BT709 =
			BT709(HAL_PIXEL_FORMAT_YCrCb_444),

	HAL_PIXEL_FORMAT_YCrCb_NV12_10_BT709 =
			BT709(HAL_PIXEL_FORMAT_YCrCb_NV12_10),
	HAL_PIXEL_FORMAT_YCbCr_422_SP_10_BT709	=
			BT709(HAL_PIXEL_FORMAT_YCbCr_422_SP_10),
	HAL_PIXEL_FORMAT_YCrCb_420_SP_10_BT709	=
			BT709(HAL_PIXEL_FORMAT_YCrCb_444_SP_10),

	HAL_PIXEL_FORMAT_YCrCb_NV12_10_BT2020 =
			BT2020(HAL_PIXEL_FORMAT_YCrCb_NV12_10),
	HAL_PIXEL_FORMAT_YCbCr_422_SP_10_BT2020	=
			BT2020(HAL_PIXEL_FORMAT_YCbCr_422_SP_10),
	HAL_PIXEL_FORMAT_YCrCb_420_SP_10_BT2020	=
			BT2020(HAL_PIXEL_FORMAT_YCrCb_444_SP_10),
};

//display data format
enum data_format {
	ARGB888,/*don't update and insert other format*/
	RGB888, /*don't update and insert other format*/
	RGB565, /*don't update and insert other format*/
	BGR888,
	XRGB888,
	XBGR888,
	ABGR888,
	BGR565,
	FBDC_RGB_565,
	FBDC_ARGB_888,
	FBDC_RGBX_888,
	FBDC_ABGR_888,
	YUV420,
	YUV422,
	YUV444,
	YUV420_A,
	YUV422_A,
	YUV444_A,
	YUV420_NV21,
	YUYV422,
	YUYV420,
	UYVY422,
	UYVY420
};
#define IS_YUV_FMT(fmt) ((fmt >= YUV420) ? 1 : 0)
#define IS_RGB_FMT(fmt) ((fmt < YUV420) ? 1 : 0)
#define IS_FBDC_FMT(fmt) \
	(((fmt >= FBDC_RGB_565) && (fmt <= FBDC_ABGR_888)) ? 1 : 0)

enum
{
	SCALE_NONE = 0x0,
	SCALE_UP   = 0x1,
	SCALE_DOWN = 0x2
};

typedef enum {
	BRIGHTNESS	= 0x0,
	CONTRAST        = 0x1,
	SAT_CON		= 0x2
} bcsh_bcs_mode;

typedef enum {
	H_SIN		= 0x0,
	H_COS       	= 0x1
} bcsh_hue_mode;

typedef enum {
	SCREEN_PREPARE_DDR_CHANGE = 0x0,
	SCREEN_UNPREPARE_DDR_CHANGE,
} screen_status;

typedef enum {
	GET_PAGE_FAULT	= 0x0,
	CLR_PAGE_FAULT  = 0x1,
	UNMASK_PAGE_FAULT = 0x2,
	UPDATE_CABC_PWM = 0x3,
	SET_DSP_MIRROR = 0x4
} extern_func;

enum rk_vop_feature {
	SUPPORT_VOP_IDENTIFY	= BIT(0),
	SUPPORT_IFBDC		= BIT(1),
	SUPPORT_AFBDC		= BIT(2),
	SUPPORT_WRITE_BACK	= BIT(3),
	SUPPORT_YUV420_OUTPUT	= BIT(4)
};

struct rk_vop_property {
	u32 feature;
	u32 max_output_x;
	u32 max_output_y;
};

enum rk_win_feature {
	SUPPORT_WIN_IDENTIFY	= BIT(0),
	SUPPORT_HW_EXIST	= BIT(1),
	SUPPORT_SCALE		= BIT(2),
	SUPPORT_YUV		= BIT(3),
	SUPPORT_YUV10BIT	= BIT(4),
	SUPPORT_MULTI_AREA	= BIT(5),
	SUPPORT_HWC_LAYER	= BIT(6)
};

struct rk_win_property {
	u32 feature;
	u32 max_input_x;
	u32 max_input_y;
};

struct rk_fb_rgb {
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
};

struct rk_fb_frame_time {
	u64 last_framedone_t;
	u64 framedone_t;
	u32 ft;
};

struct rk_fb_vsync {
	wait_queue_head_t wait;
	ktime_t timestamp;
	int active;
	bool irq_stop;
	int irq_refcount;
	struct mutex irq_lock;
	struct task_struct *thread;
};

struct color_key_cfg {
	u32 win0_color_key_cfg;
	u32 win1_color_key_cfg;
	u32 win2_color_key_cfg;
};

struct pwr_ctr {
	char name[32];
	int type;
	int is_rst;
	int gpio;
	int atv_val;
	const char *rgl_name;
	int volt;
	int delay;
};

struct rk_disp_pwr_ctr_list {
	struct list_head list;
	struct pwr_ctr pwr_ctr;
};

typedef enum _TRSP_MODE {
	TRSP_CLOSE = 0,
	TRSP_FMREG,
	TRSP_FMREGEX,
	TRSP_FMRAM,
	TRSP_FMRAMEX,
	TRSP_MASK,
	TRSP_INVAL
} TRSP_MODE;

struct rk_lcdc_post_cfg {
	u32 xpos;
	u32 ypos;
	u32 xsize;
	u32 ysize;
};

struct rk_fb_wb_cfg {
	u8  data_format;
	short ion_fd;
	u32 phy_addr;
	u16 xsize;
	u16 ysize;
	u8 reserved0;
	u32 reversed1;
};

struct rk_lcdc_bcsh {
	bool enable;
	u16 brightness;
	u16 contrast;
	u16 sat_con;
	u16 sin_hue;
	u16 cos_hue;
};

struct rk_lcdc_win_area {
	bool state;
	enum data_format format;
	u8 data_space;		/* SDR or HDR */
	u8 fmt_cfg;
	u8 yuyv_fmt;
	u8 swap_rb;
	u8 swap_uv;
	u32 y_offset;		/*yuv/rgb offset  -->LCDC_WINx_YRGB_MSTx*/
	u32 c_offset;		/*cb cr offset--->LCDC_WINx_CBR_MSTx*/
	u16 xpos;		/*start point in panel  --->LCDC_WINx_DSP_ST*/
	u16 ypos;
	u16 xsize;		/* display window width/height  -->LCDC_WINx_DSP_INFO*/
	u16 ysize;
	u16 xact;		/*origin display window size -->LCDC_WINx_ACT_INFO*/
	u16 yact;
	u16 xvir;		/*virtual width/height     -->LCDC_WINx_VIR*/
	u16 yvir;
	u16 xoff;		/*mem offset*/
	u16 yoff;
	unsigned long smem_start;
	unsigned long cbr_start;	/*Cbr memory start address*/
#if defined(CONFIG_ION_ROCKCHIP)
		struct ion_handle *ion_hdl;
		int dma_buf_fd;
		struct dma_buf *dma_buf;
#endif
	u16 dsp_stx;
	u16 dsp_sty;
	u16 y_vir_stride;
	u16 uv_vir_stride;
	u32 y_addr;
	u32 uv_addr;

	u8  fbdc_en;
	u8  fbdc_cor_en;
	u8  fbdc_data_format;
	u8  fbdc_dsp_width_ratio;
	u8  fbdc_fmt_cfg;
	u16 fbdc_mb_vir_width;
	u16 fbdc_mb_vir_height;
	u16 fbdc_mb_width;
	u16 fbdc_mb_height;
	u16 fbdc_mb_xst;
	u16 fbdc_mb_yst;
	u16 fbdc_num_tiles;
	u16 fbdc_cmp_index_init;
};


struct rk_lcdc_win {
	char name[5];
	int id;
	struct rk_win_property property;
	bool state;		/*on or off*/
	bool last_state;		/*on or off*/
	u32 pseudo_pal[16];
	int z_order;		/*win sel layer*/
	u8 fmt_10;
	u8 colorspace;
	u32 reserved;
	u32 area_num;
	u32 scale_yrgb_x;
	u32 scale_yrgb_y;
	u32 scale_cbcr_x;
	u32 scale_cbcr_y;
	bool support_3d;

	u8 win_lb_mode;

	u8 bic_coe_el;
	u8 yrgb_hor_scl_mode;//h 01:scale up ;10:down
	u8 yrgb_ver_scl_mode;//v 01:scale up ;10:down
	u8 yrgb_hsd_mode;//h scale down mode
	u8 yrgb_vsu_mode;//v scale up mode
	u8 yrgb_vsd_mode;//v scale down mode
	u8 cbr_hor_scl_mode;
	u8 cbr_ver_scl_mode;
	u8 cbr_hsd_mode;
	u8 cbr_vsu_mode;
	u8 cbr_vsd_mode;
	u8 vsd_yrgb_gt4;
	u8 vsd_yrgb_gt2;
	u8 vsd_cbr_gt4;
	u8 vsd_cbr_gt2;

	u8 alpha_en;
	u8 alpha_mode;
	u16 g_alpha_val;
	u32 color_key_val;
	u8 csc_mode;
	u8 xmirror;
	u8 ymirror;

	struct rk_lcdc_win_area area[RK_WIN_MAX_AREA];
	struct rk_lcdc_post_cfg post_cfg;
};

struct rk_lcdc_driver;

struct rk_fb_trsm_ops {
	int (*enable)(void);
	int (*disable)(void);
	int (*dsp_pwr_on) (void);
	int (*dsp_pwr_off) (void);
	void (*refresh)(unsigned int xpos, unsigned int ypos,
		       unsigned int xsize, unsigned int ysize);
};

struct rk_lcdc_drv_ops {
	int (*open) (struct rk_lcdc_driver *dev_drv, int layer_id, bool open);
	int (*win_direct_en)(struct rk_lcdc_driver *dev_drv, int win_id, int en);
	int (*init_lcdc) (struct rk_lcdc_driver *dev_drv);
	int (*ioctl) (struct rk_lcdc_driver *dev_drv, unsigned int cmd,
		      unsigned long arg, int layer_id);
	int (*suspend) (struct rk_lcdc_driver *dev_drv);
	int (*resume) (struct rk_lcdc_driver *dev_drv);
	int (*blank) (struct rk_lcdc_driver *dev_drv, int layer_id,
		      int blank_mode);
	int (*set_par) (struct rk_lcdc_driver *dev_drv, int layer_id);
	int (*pan_display) (struct rk_lcdc_driver *dev_drv, int layer_id);
	int (*direct_set_addr)(struct rk_lcdc_driver *drv, int win_id, u32 addr);
	int (*lcdc_reg_update) (struct rk_lcdc_driver *dev_drv);
	ssize_t(*get_disp_info) (struct rk_lcdc_driver *dev_drv, char *buf,
				  int layer_id);
	int (*load_screen) (struct rk_lcdc_driver *dev_drv, bool initscreen);
	int (*get_dspbuf_info) (struct rk_lcdc_driver *dev_drv,
				u16 *xact, u16 *yact, int *format,
				u32 *dsp_addr, int *ymirror);
	int (*post_dspbuf)(struct rk_lcdc_driver *dev_drv, u32 rgb_mst,
			   int format, u16 xact, u16 yact, u16 xvir,
			   int ymirror);

	int (*get_win_state) (struct rk_lcdc_driver *dev_drv, int layer_id, int area_id);
	int (*ovl_mgr) (struct rk_lcdc_driver *dev_drv, int swap, bool set);	/*overlay manager*/
	int (*fps_mgr) (struct rk_lcdc_driver *dev_drv, int fps, bool set);
	int (*fb_get_win_id) (struct rk_lcdc_driver *dev_drv, const char *id);	/*find layer for fb*/
	int (*fb_win_remap) (struct rk_lcdc_driver *dev_drv,
			     u16 fb_win_map_order);
	int (*set_dsp_lut) (struct rk_lcdc_driver *dev_drv, int *lut);
	int (*set_cabc_lut)(struct rk_lcdc_driver *dev_drv, int *lut);
	int (*set_hwc_lut) (struct rk_lcdc_driver *dev_drv, int *hwc_lut, int mode);
	int (*read_dsp_lut) (struct rk_lcdc_driver *dev_drv, int *lut);
	int (*lcdc_hdmi_process) (struct rk_lcdc_driver *dev_drv, int mode);	/*some lcdc need to some process in hdmi mode*/
	int (*set_irq_to_cpu)(struct rk_lcdc_driver *dev_drv,int enable);
	int (*poll_vblank) (struct rk_lcdc_driver *dev_drv);
	int (*lcdc_rst) (struct rk_lcdc_driver *dev_drv);
	int (*dpi_open) (struct rk_lcdc_driver *dev_drv, bool open);
	int (*dpi_win_sel) (struct rk_lcdc_driver *dev_drv, int layer_id);
	int (*dpi_status) (struct rk_lcdc_driver *dev_drv);
	int (*get_dsp_addr)(struct rk_lcdc_driver *dev_drv, unsigned int dsp_addr[][4]);
	int (*set_dsp_cabc) (struct rk_lcdc_driver *dev_drv, int mode, int calc, int up, int down, int global);
	int (*set_dsp_bcsh_hue) (struct rk_lcdc_driver *dev_drv,int sin_hue, int cos_hue);
	int (*set_dsp_bcsh_bcs)(struct rk_lcdc_driver *dev_drv,bcsh_bcs_mode mode,int value);
	int (*get_dsp_bcsh_hue) (struct rk_lcdc_driver *dev_drv,bcsh_hue_mode mode);
	int (*get_dsp_bcsh_bcs)(struct rk_lcdc_driver *dev_drv,bcsh_bcs_mode mode);
	int (*open_bcsh)(struct rk_lcdc_driver *dev_drv, bool open);
        int (*set_screen_scaler) (struct rk_lcdc_driver *dev_drv, struct rk_screen *screen, bool enable);
	int (*dump_reg) (struct rk_lcdc_driver *dev_drv);
	int (*mmu_en) (struct rk_lcdc_driver *dev_drv);
	int (*cfg_done) (struct rk_lcdc_driver *dev_drv);
	int (*set_overscan) (struct rk_lcdc_driver *dev_drv,
			     struct overscan *overscan);
	int (*dsp_black) (struct rk_lcdc_driver *dev_drv, int enable);
	int (*backlight_close)(struct rk_lcdc_driver *dev_drv, int enable);
	int (*area_support_num)(struct rk_lcdc_driver *dev_drv, unsigned int *area_support);
	int (*extern_func)(struct rk_lcdc_driver *dev_drv, int cmd);
	int (*wait_frame_start)(struct rk_lcdc_driver *dev_drv, int enable);
	int (*set_wb)(struct rk_lcdc_driver *dev_drv);
};

struct rk_fb_area_par {
	u8  data_format;        /*layer data fmt*/
	short ion_fd;
	u32 phy_addr;
	short acq_fence_fd;
	u16  x_offset;
	u16  y_offset;
	u16 xpos;	/*start point in panel  --->LCDC_WINx_DSP_ST*/
	u16 ypos;
	u16 xsize;	/* display window width/height  -->LCDC_WINx_DSP_INFO*/
	u16 ysize;
	u16 xact;	/*origin display window size -->LCDC_WINx_ACT_INFO*/
	u16 yact;
	u16 xvir;	/*virtual width/height     -->LCDC_WINx_VIR*/
	u16 yvir;
	u8  fbdc_en;
	u8  fbdc_cor_en;
	u8  fbdc_data_format;
	u16 data_space;	/* SDR or HDR */
	u32 reserved0;
};


struct rk_fb_win_par {
	u8  win_id;
	u8  z_order;		/*win sel layer*/
	u8  alpha_mode;
	u16 g_alpha_val;
	u8  mirror_en;
	struct rk_fb_area_par area_par[RK_WIN_MAX_AREA];
	u32 reserved0;
};

struct rk_fb_win_cfg_data {
	u8  wait_fs;
	short ret_fence_fd;
	short rel_fence_fd[RK_MAX_BUF_NUM];
	struct  rk_fb_win_par win_par[RK30_MAX_LAYER_SUPPORT];
	struct  rk_fb_wb_cfg wb_cfg;
};

struct rk_fb_reg_wb_data {
	bool state;
	u8 data_format;
	struct ion_handle *ion_handle;
	unsigned long smem_start;
	unsigned long cbr_start;	/*Cbr memory start address*/
	u16 xsize;
	u16 ysize;
};

struct rk_fb_reg_area_data {
	struct sync_fence *acq_fence;
	u8 data_format;        /*layer data fmt*/
	u8 data_space;		/* indicate SDR or HDR */
	u8  index_buf;          /*judge if the buffer is index*/
	u32 y_offset;		/*yuv/rgb offset  -->LCDC_WINx_YRGB_MSTx*/
	u32 c_offset;		/*cb cr offset--->LCDC_WINx_CBR_MSTx*/
	u32 y_vir_stride;
	u32 uv_vir_stride;
	u32 buff_len;
	u16 xpos;		/*start point in panel  --->LCDC_WINx_DSP_ST*/
	u16 ypos;
	u16 xsize;		/* display window width/height  -->LCDC_WINx_DSP_INFO*/
	u16 ysize;
	u16 xact;		/*origin display window size -->LCDC_WINx_ACT_INFO*/
	u16 yact;
	u16 xvir;		/*virtual width/height     -->LCDC_WINx_VIR*/
	u16 yvir;
	u16 xoff;		/*mem offset*/
	u16 yoff;
	unsigned long smem_start;
	unsigned long cbr_start;	/*Cbr memory start address*/
	u32 line_length;	
	struct ion_handle *ion_handle;
#ifdef 	USE_ION_MMU
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;
#endif
	u8  fbdc_en;
	u8  fbdc_cor_en;
	u8  fbdc_data_format;
};

struct rk_fb_reg_win_data {
	int win_id;
	int z_order;		/*win sel layer*/
	u32 area_num;		/*maybe two region have the same dma buff,*/
	u32 area_buf_num;     /*so area_num  maybe not equal to area_buf_num*/
	u8 alpha_en;
	u8 alpha_mode;
	u16 g_alpha_val;
	u8  mirror_en;
	u8 colorspace;

	struct rk_fb_reg_area_data reg_area_data[RK_WIN_MAX_AREA];
};

struct rk_fb_reg_data {
	struct list_head list;
	int    win_num;
	int    buf_num;
	int    acq_num;
	struct rk_fb_reg_win_data reg_win_data[RK30_MAX_LAYER_SUPPORT];
	struct rk_fb_reg_wb_data reg_wb_data;
};

struct rk_lcdc_driver {
	char name[6];
	int  te_irq;
	int  id;
	int  prop;
	struct device *dev;
	u32 version;
	struct rk_vop_property property;

	struct rk_lcdc_win *win[RK_MAX_FB_SUPPORT];
	struct rk_fb_reg_wb_data wb_data;
	int lcdc_win_num;
	int num_buf;		//the num_of buffer
	int atv_layer_cnt;
	int fb_index_base;	//the first fb index of the lcdc device
	struct rk_screen *screen0;	//some platform have only one lcdc,but extend
	struct rk_screen *screen1;	//two display devices for dual display,such as rk2918,rk2928
	struct rk_screen *cur_screen;	//screen0 is primary screen ,like lcd panel,screen1 is  extend screen,like hdmi
	u32 pixclock;
	u16 rotate_mode;
	u16 cabc_mode;
	u16 overlay_mode;
	u16 pre_overlay;
	u16 output_color;

	u16  fb_win_map;
	char fb0_win_id;
	char fb1_win_id;
	char fb2_win_id;
	char fb3_win_id;
	char fb4_win_id;
	
	char mmu_dts_name[40];
	struct device *mmu_dev;
	int iommu_enabled;
	int dsp_mode;
	bool hot_plug_state;

	struct rk_fb_reg_area_data reg_area_data;
	/*
	 * front_regs means this config is scaning on the devices.
	 */
	struct rk_fb_reg_data *front_regs;
	struct mutex front_lock;

	struct mutex fb_win_id_mutex;
	struct mutex win_config;

	struct mutex switch_screen; /*for switch screen*/
	struct completion frame_done;	/*sync for pan_display,whe we set a new
					  frame address to lcdc register,we must
					  make sure the frame begain to display*/
	spinlock_t cpl_lock;	/*lock for completion  frame done */
	int first_frame;
	struct rk_fb_vsync vsync_info;
	struct rk_fb_frame_time frame_time;
	int wait_fs;		/*wait for new frame start in kernel */
	struct sw_sync_timeline *timeline;
	int			timeline_max;
	int			suspend_flag;
	int			shutdown_flag;
	int standby;
	struct list_head	update_regs_list;
	struct list_head	saved_list;
	struct mutex		update_regs_list_lock;
	struct kthread_worker	update_regs_worker;
	struct task_struct	*update_regs_thread;
	struct kthread_work	update_regs_work;
	wait_queue_head_t 	update_regs_wait;

	struct mutex		output_lock;
	struct rk29fb_info *screen_ctr_info;
	struct list_head pwrlist_head;
	struct rk_lcdc_drv_ops *ops;
	struct rk_fb_trsm_ops *trsm_ops;
#ifdef CONFIG_DRM_ROCKCHIP
	void (*irq_call_back)(struct rk_lcdc_driver *driver);
#endif
	struct overscan overscan;
	struct rk_lcdc_bcsh bcsh;
	int *hwc_lut;
	int uboot_logo;
	int bcsh_init_status;
	bool cabc_pwm_pol;
	u8  reserved_fb;
	/*1:hdmi switch uncomplete,0:complete*/
	bool hdmi_switch;
	void *trace_buf;
	struct rk_fb_win_cfg_data tmp_win_cfg[DUMP_FRAME_NUM];
	struct rk_fb_reg_data tmp_regs[DUMP_FRAME_NUM];
	unsigned int area_support[RK30_MAX_LAYER_SUPPORT];
};

struct rk_fb_par {
	int id;
	u32 state;

	unsigned long fb_phy_base;	/* Start of fb address (physical address) */
	char __iomem *fb_virt_base;	/* Start of fb address (virt address) */
	u32 fb_size;
	struct rk_lcdc_driver *lcdc_drv;

#if defined(CONFIG_ION_ROCKCHIP)
	struct ion_handle *ion_hdl;
#endif
	u32 reserved[2];
};

/*disp_mode: dual display mode
*	        NO_DUAL,no dual display,
	        ONE_DUAL,use one lcdc + rk61x for dual display
	        DUAL,use 2 lcdcs for dual display
  num_fb:       the total number of fb
  num_lcdc:    the total number of lcdc
*/

struct rk_fb {
	int disp_mode;
	int disp_policy;
	struct rk29fb_info *mach_info;
	struct fb_info *fb[RK_MAX_FB_SUPPORT*2];
	int num_fb;
	struct rk_lcdc_driver *lcdc_dev_drv[RK30_MAX_LCDC_SUPPORT];
	int num_lcdc;

#if defined(CONFIG_ION_ROCKCHIP)
       struct ion_client *ion_client;
#endif
};

extern int rk_fb_trsm_ops_register(struct rk_fb_trsm_ops *ops, int type);
extern struct rk_fb_trsm_ops *rk_fb_trsm_ops_get(int type);
extern int rk_fb_register(struct rk_lcdc_driver *dev_drv,
				struct rk_lcdc_win *win, int id);
extern int rk_fb_unregister(struct rk_lcdc_driver *dev_drv);
extern struct rk_lcdc_driver *rk_get_lcdc_drv(char *name);
extern int rk_fb_get_extern_screen(struct rk_screen *screen);
extern int rk_fb_set_vop_pwm(void);
extern int rk_fb_get_prmry_screen( struct rk_screen *screen);
extern int rk_fb_set_prmry_screen(struct rk_screen *screen);
extern u32 rk_fb_get_prmry_screen_pixclock(void);
extern int rk_disp_pwr_ctr_parse_dt(struct rk_lcdc_driver *dev_drv);
extern int rk_disp_pwr_enable(struct rk_lcdc_driver *dev_drv);
extern int rk_disp_pwr_disable(struct rk_lcdc_driver *dev_drv);
extern bool is_prmry_rk_lcdc_registered(void);
extern int rk_fb_prase_timing_dt(struct device_node *np,
		struct rk_screen *screen);
extern int rk_disp_prase_timing_dt(struct rk_lcdc_driver *dev_drv);

extern int rk_fb_dpi_open(bool open);
extern int rk_fb_dpi_layer_sel(int layer_id);
extern int rk_fb_dpi_status(void);

extern int rk_fb_switch_screen(struct rk_screen *screen, int enable, int lcdc_id);
extern int rk_fb_disp_scale(u8 scale_x, u8 scale_y, u8 lcdc_id);
extern int rkfb_create_sysfs(struct fb_info *fbi);
extern char *get_format_string(enum data_format, char *fmt);
extern int support_uboot_display(void);
extern int  rk_fb_calc_fps(struct rk_screen *screen, u32 pixclock);
extern int rk_get_real_fps(int time);
extern struct device *rk_fb_get_sysmmu_device_by_compatible(const char *compt);
extern void rk_fb_platform_set_sysmmu(struct device *sysmmu,
                                      struct device *dev);
int rk_fb_get_display_policy(void);
int rk_fb_pixel_width(int data_format);
void trace_buffer_dump(struct device *dev,
			      struct rk_lcdc_driver *dev_drv);
int rk_fb_set_car_reverse_status(struct rk_lcdc_driver *dev_drv, int status);
extern int rockchip_get_screen_type(void);
#endif
