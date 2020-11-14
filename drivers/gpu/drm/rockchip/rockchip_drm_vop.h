/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author:Mark Yao <mark.yao@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ROCKCHIP_DRM_VOP_H
#define _ROCKCHIP_DRM_VOP_H

/*
 * major: IP major vertion, used for IP structure
 * minor: big feature change under same structure
 */
#define VOP_VERSION(major, minor)	((major) << 8 | (minor))
#define VOP_MAJOR(version) 	((version) >> 8)
#define VOP_MINOR(version) 	((version) & 0xff)

#define ROCKCHIP_OUTPUT_DSI_DUAL_CHANNEL	BIT(0)
#define ROCKCHIP_OUTPUT_DSI_DUAL_LINK		BIT(1)

#define AFBDC_FMT_RGB565	0x0
#define AFBDC_FMT_U8U8U8U8	0x5
#define AFBDC_FMT_U8U8U8	0x4

enum bcsh_out_mode {
	BCSH_OUT_MODE_BLACK,
	BCSH_OUT_MODE_BLUE,
	BCSH_OUT_MODE_COLOR_BAR,
	BCSH_OUT_MODE_NORMAL_VIDEO,
};

enum cabc_stage_mode {
	LAST_FRAME_PWM_VAL	= 0x0,
	CUR_FRAME_PWM_VAL	= 0x1,
	STAGE_BY_STAGE		= 0x2
};

enum cabc_stage_up_mode {
	MUL_MODE,
	ADD_MODE,
};

#define DSP_BG_SWAP		0x1
#define DSP_RB_SWAP		0x2
#define DSP_RG_SWAP		0x4
#define DSP_DELTA_SWAP		0x8

enum vop_csc_format {
	CSC_BT601L,
	CSC_BT709L,
	CSC_BT601F,
	CSC_BT2020,
};

enum vop_csc_mode {
	CSC_RGB,
	CSC_YUV,
};

enum vop_data_format {
	VOP_FMT_ARGB8888 = 0,
	VOP_FMT_RGB888,
	VOP_FMT_RGB565,
	VOP_FMT_YUV420SP = 4,
	VOP_FMT_YUV422SP,
	VOP_FMT_YUV444SP,
};

enum vop_raw_format {
	VOP_RAW8	= 0,
	VOP_RAW10,
	VOP_RAW_RESERVED,
	VOP_RAW16,
};

struct vop_reg_data {
	uint32_t offset;
	uint32_t value;
};

struct vop_reg {
	uint32_t mask;
	uint32_t offset:12;
	uint32_t shift:5;
	uint32_t begin_minor:4;
	uint32_t end_minor:4;
	uint32_t major:3;
	uint32_t write_mask:1;
};

struct vop_csc {
	struct vop_reg y2r_en;
	struct vop_reg r2r_en;
	struct vop_reg r2y_en;
	struct vop_reg csc_mode;

	uint32_t y2r_offset;
	uint32_t r2r_offset;
	uint32_t r2y_offset;
};

struct vop_ctrl {
	struct vop_reg version;
	struct vop_reg standby;
	struct vop_reg dma_stop;
	struct vop_reg axi_outstanding_max_num;
	struct vop_reg axi_max_outstanding_en;
	struct vop_reg htotal_pw;
	struct vop_reg hact_st_end;
	struct vop_reg vtotal_pw;
	struct vop_reg vact_st_end;
	struct vop_reg vact_st_end_f1;
	struct vop_reg vs_st_end_f1;
	struct vop_reg hpost_st_end;
	struct vop_reg vpost_st_end;
	struct vop_reg vpost_st_end_f1;
	struct vop_reg post_scl_factor;
	struct vop_reg post_scl_ctrl;
	struct vop_reg dsp_interlace;
	struct vop_reg global_regdone_en;
	struct vop_reg auto_gate_en;
	struct vop_reg post_lb_mode;
	struct vop_reg dsp_layer_sel;
	struct vop_reg overlay_mode;
	struct vop_reg core_dclk_div;
	struct vop_reg dclk_ddr;
	struct vop_reg p2i_en;
	struct vop_reg hdmi_dclk_out_en;
	struct vop_reg rgb_en;
	struct vop_reg lvds_en;
	struct vop_reg edp_en;
	struct vop_reg hdmi_en;
	struct vop_reg mipi_en;
	struct vop_reg data01_swap;
	struct vop_reg mipi_dual_channel_en;
	struct vop_reg dp_en;
	struct vop_reg dclk_pol;
	struct vop_reg pin_pol;
	struct vop_reg rgb_dclk_pol;
	struct vop_reg rgb_pin_pol;
	struct vop_reg lvds_dclk_pol;
	struct vop_reg lvds_pin_pol;
	struct vop_reg hdmi_dclk_pol;
	struct vop_reg hdmi_pin_pol;
	struct vop_reg edp_dclk_pol;
	struct vop_reg edp_pin_pol;
	struct vop_reg mipi_dclk_pol;
	struct vop_reg mipi_pin_pol;
	struct vop_reg dp_dclk_pol;
	struct vop_reg dp_pin_pol;
	struct vop_reg dither_down_sel;
	struct vop_reg dither_down_mode;
	struct vop_reg dither_down_en;
	struct vop_reg pre_dither_down_en;
	struct vop_reg dither_up_en;

	struct vop_reg sw_dac_sel;
	struct vop_reg tve_sw_mode;
	struct vop_reg tve_dclk_pol;
	struct vop_reg tve_dclk_en;
	struct vop_reg sw_genlock;
	struct vop_reg sw_uv_offset_en;
	struct vop_reg dsp_out_yuv;
	struct vop_reg dsp_data_swap;
	struct vop_reg dsp_ccir656_avg;
	struct vop_reg dsp_black;
	struct vop_reg dsp_blank;
	struct vop_reg dsp_outzero;
	struct vop_reg update_gamma_lut;
	struct vop_reg lut_buffer_index;
	struct vop_reg dsp_lut_en;

	struct vop_reg out_mode;

	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg dsp_background;

	/* AFBDC */
	struct vop_reg afbdc_en;
	struct vop_reg afbdc_sel;
	struct vop_reg afbdc_format;
	struct vop_reg afbdc_hreg_block_split;
	struct vop_reg afbdc_pic_size;
	struct vop_reg afbdc_hdr_ptr;
	struct vop_reg afbdc_rstn;
	struct vop_reg afbdc_pic_vir_width;
	struct vop_reg afbdc_pic_offset;
	struct vop_reg afbdc_axi_ctrl;

	/* CABC */
	struct vop_reg cabc_total_num;
	struct vop_reg cabc_config_mode;
	struct vop_reg cabc_stage_up_mode;
	struct vop_reg cabc_scale_cfg_value;
	struct vop_reg cabc_scale_cfg_enable;
	struct vop_reg cabc_global_dn_limit_en;
	struct vop_reg cabc_lut_en;
	struct vop_reg cabc_en;
	struct vop_reg cabc_handle_en;
	struct vop_reg cabc_stage_up;
	struct vop_reg cabc_stage_down;
	struct vop_reg cabc_global_dn;
	struct vop_reg cabc_calc_pixel_num;

	/* BCSH */
	struct vop_reg bcsh_brightness;
	struct vop_reg bcsh_contrast;
	struct vop_reg bcsh_sat_con;
	struct vop_reg bcsh_sin_hue;
	struct vop_reg bcsh_cos_hue;
	struct vop_reg bcsh_r2y_csc_mode;
	struct vop_reg bcsh_r2y_en;
	struct vop_reg bcsh_y2r_csc_mode;
	struct vop_reg bcsh_y2r_en;
	struct vop_reg bcsh_color_bar;
	struct vop_reg bcsh_out_mode;
	struct vop_reg bcsh_en;

	/* HDR */
	struct vop_reg level2_overlay_en;
	struct vop_reg alpha_hard_calc;
	struct vop_reg hdr2sdr_en;
	struct vop_reg hdr2sdr_en_win0_csc;
	struct vop_reg hdr2sdr_src_min;
	struct vop_reg hdr2sdr_src_max;
	struct vop_reg hdr2sdr_normfaceetf;
	struct vop_reg hdr2sdr_dst_min;
	struct vop_reg hdr2sdr_dst_max;
	struct vop_reg hdr2sdr_normfacgamma;

	struct vop_reg bt1886eotf_pre_conv_en;
	struct vop_reg rgb2rgb_pre_conv_en;
	struct vop_reg rgb2rgb_pre_conv_mode;
	struct vop_reg st2084oetf_pre_conv_en;
	struct vop_reg bt1886eotf_post_conv_en;
	struct vop_reg rgb2rgb_post_conv_en;
	struct vop_reg rgb2rgb_post_conv_mode;
	struct vop_reg st2084oetf_post_conv_en;
	struct vop_reg win_csc_mode_sel;

	/* MCU OUTPUT */
	struct vop_reg mcu_pix_total;
	struct vop_reg mcu_cs_pst;
	struct vop_reg mcu_cs_pend;
	struct vop_reg mcu_rw_pst;
	struct vop_reg mcu_rw_pend;
	struct vop_reg mcu_clk_sel;
	struct vop_reg mcu_hold_mode;
	struct vop_reg mcu_frame_st;
	struct vop_reg mcu_rs;
	struct vop_reg mcu_bypass;
	struct vop_reg mcu_type;
	struct vop_reg mcu_rw_bypass_port;

	/* VOP RAW */
	struct vop_reg frame_st;
	struct vop_reg work_mode; /* bypass, pingpong, hold mode */
	struct vop_reg pdaf_en;
	struct vop_reg pdaf_type; /* hblank or vblank mode */
	struct vop_reg pdaf_vc_num;

	struct vop_reg reg_done_frm;
	struct vop_reg cfg_done;
};

struct vop_intr {
	const int *intrs;
	uint32_t nintrs;
	struct vop_reg line_flag_num[2];
	struct vop_reg enable;
	struct vop_reg clear;
	struct vop_reg status;
};

struct vop_scl_extension {
	struct vop_reg cbcr_vsd_mode;
	struct vop_reg cbcr_vsu_mode;
	struct vop_reg cbcr_hsd_mode;
	struct vop_reg cbcr_ver_scl_mode;
	struct vop_reg cbcr_hor_scl_mode;
	struct vop_reg yrgb_vsd_mode;
	struct vop_reg yrgb_vsu_mode;
	struct vop_reg yrgb_hsd_mode;
	struct vop_reg yrgb_ver_scl_mode;
	struct vop_reg yrgb_hor_scl_mode;
	struct vop_reg line_load_mode;
	struct vop_reg cbcr_axi_gather_num;
	struct vop_reg yrgb_axi_gather_num;
	struct vop_reg vsd_cbcr_gt2;
	struct vop_reg vsd_cbcr_gt4;
	struct vop_reg vsd_yrgb_gt2;
	struct vop_reg vsd_yrgb_gt4;
	struct vop_reg bic_coe_sel;
	struct vop_reg cbcr_axi_gather_en;
	struct vop_reg yrgb_axi_gather_en;
	struct vop_reg lb_mode;
};

struct vop_scl_regs {
	const struct vop_scl_extension *ext;

	struct vop_reg scale_yrgb_x;
	struct vop_reg scale_yrgb_y;
	struct vop_reg scale_cbcr_x;
	struct vop_reg scale_cbcr_y;
};

struct vop_csc_table {
	const uint32_t *y2r_bt601;
	const uint32_t *y2r_bt601_12_235;
	const uint32_t *y2r_bt601_10bit;
	const uint32_t *y2r_bt601_10bit_12_235;
	const uint32_t *r2y_bt601;
	const uint32_t *r2y_bt601_12_235;
	const uint32_t *r2y_bt601_10bit;
	const uint32_t *r2y_bt601_10bit_12_235;

	const uint32_t *y2r_bt709;
	const uint32_t *y2r_bt709_10bit;
	const uint32_t *r2y_bt709;
	const uint32_t *r2y_bt709_10bit;

	const uint32_t *y2r_bt2020;
	const uint32_t *r2y_bt2020;

	const uint32_t *r2r_bt709_to_bt2020;
	const uint32_t *r2r_bt2020_to_bt709;
};

struct vop_hdr_table {
	const uint32_t hdr2sdr_eetf_oetf_y0_offset;
	const uint32_t hdr2sdr_eetf_oetf_y1_offset;
	const uint32_t *hdr2sdr_eetf_yn;
	const uint32_t *hdr2sdr_bt1886oetf_yn;
	const uint32_t hdr2sdr_sat_y0_offset;
	const uint32_t hdr2sdr_sat_y1_offset;
	const uint32_t *hdr2sdr_sat_yn;

	const uint32_t hdr2sdr_src_range_min;
	const uint32_t hdr2sdr_src_range_max;
	const uint32_t hdr2sdr_normfaceetf;
	const uint32_t hdr2sdr_dst_range_min;
	const uint32_t hdr2sdr_dst_range_max;
	const uint32_t hdr2sdr_normfacgamma;

	const uint32_t sdr2hdr_eotf_oetf_y0_offset;
	const uint32_t sdr2hdr_eotf_oetf_y1_offset;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_hlg_hdr;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_bt2020;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_hdr;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_hlg_hdr;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_bt2020;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_hdr;
	const uint32_t sdr2hdr_oetf_dx_dxpow1_offset;
	const uint32_t *sdr2hdr_st2084oetf_dxn_pow2;
	const uint32_t *sdr2hdr_st2084oetf_dxn;
	const uint32_t sdr2hdr_oetf_xn1_offset;
	const uint32_t *sdr2hdr_st2084oetf_xn;
};

enum {
	VOP_CSC_Y2R_BT601,
	VOP_CSC_Y2R_BT709,
	VOP_CSC_Y2R_BT2020,
	VOP_CSC_R2Y_BT601,
	VOP_CSC_R2Y_BT709,
	VOP_CSC_R2Y_BT2020,
	VOP_CSC_R2R_BT2020_TO_BT709,
	VOP_CSC_R2R_BT709_TO_2020,
};

enum _vop_overlay_mode {
	VOP_RGB_DOMAIN,
	VOP_YUV_DOMAIN
};

enum _vop_sdr2hdr_func {
	SDR2HDR_FOR_BT2020,
	SDR2HDR_FOR_HDR,
	SDR2HDR_FOR_HLG_HDR,
};

enum _vop_rgb2rgb_conv_mode {
	BT709_TO_BT2020,
	BT2020_TO_BT709,
};

enum _MCU_IOCTL {
	MCU_WRCMD = 0,
	MCU_WRDATA,
	MCU_SETBYPASS,
};

struct vop_win_phy {
	const struct vop_scl_regs *scl;
	const uint32_t *data_formats;
	uint32_t nformats;

	struct vop_reg gate;
	struct vop_reg enable;
	struct vop_reg format;
	struct vop_reg fmt_10;
	struct vop_reg csc_mode;
	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg rb_swap;
	struct vop_reg act_info;
	struct vop_reg dsp_info;
	struct vop_reg dsp_st;
	struct vop_reg yrgb_mst;
	struct vop_reg uv_mst;
	struct vop_reg yrgb_vir;
	struct vop_reg uv_vir;

	struct vop_reg channel;
	struct vop_reg dst_alpha_ctl;
	struct vop_reg src_alpha_ctl;
	struct vop_reg alpha_mode;
	struct vop_reg alpha_en;
	struct vop_reg alpha_pre_mul;
	struct vop_reg global_alpha_val;
	struct vop_reg key_color;
	struct vop_reg key_en;
	struct vop_reg yrgb_mst1;
	struct vop_reg ex_wc;
	struct vop_reg data_type;
	struct vop_reg vact_st_end_info;
};

struct vop_win_data {
	uint32_t base;
	enum drm_plane_type type;
	const struct vop_win_phy *phy;
	const struct vop_win_phy **area;
	const struct vop_csc *csc;
	unsigned int area_size;
	u64 feature;
};

struct vop_grf_ctrl {
	struct vop_reg grf_dclk_inv;
};

#define VOP_FEATURE_OUTPUT_10BIT	BIT(0)
#define VOP_FEATURE_AFBDC		BIT(1)
#define VOP_FEATURE_ALPHA_SCALE		BIT(2)

#define WIN_FEATURE_HDR2SDR		BIT(0)
#define WIN_FEATURE_SDR2HDR		BIT(1)
#define WIN_FEATURE_PRE_OVERLAY		BIT(2)
#define WIN_FEATURE_AFBDC		BIT(3)
#define WIN_FEATURE_PDAF_AFTER_VBLANK	BIT(4)

struct vop_rect {
	int width;
	int height;
};

struct vop_data {
	const struct vop_reg_data *init_table;
	unsigned int table_size;
	const struct vop_ctrl *ctrl;
	const struct vop_intr *intr;
	const struct vop_win_data *win;
	const struct vop_csc_table *csc_table;
	const struct vop_hdr_table *hdr_table;
	const struct vop_grf_ctrl *grf_ctrl;
	unsigned int win_size;
	uint32_t version;
	struct vop_rect max_input;
	struct vop_rect max_output;
	u64 feature;
};

#define CVBS_PAL_VDISPLAY		288

/* interrupt define */
#define DSP_HOLD_VALID_INTR		BIT(0)
#define FS_INTR				BIT(1)
#define LINE_FLAG_INTR			BIT(2)
#define BUS_ERROR_INTR			BIT(3)
#define FS_NEW_INTR			BIT(4)
#define ADDR_SAME_INTR			BIT(5)
#define LINE_FLAG1_INTR			BIT(6)
#define WIN0_EMPTY_INTR			BIT(7)
#define WIN1_EMPTY_INTR			BIT(8)
#define WIN2_EMPTY_INTR			BIT(9)
#define WIN3_EMPTY_INTR			BIT(10)
#define HWC_EMPTY_INTR			BIT(11)
#define POST_BUF_EMPTY_INTR		BIT(12)
#define PWM_GEN_INTR			BIT(13)
#define DMA_FINISH_INTR			BIT(14)
#define FS_FIELD_INTR			BIT(15)
#define FE_INTR				BIT(16)

#define INTR_MASK			(DSP_HOLD_VALID_INTR | FS_INTR | \
					 LINE_FLAG_INTR | BUS_ERROR_INTR | \
					 FS_NEW_INTR | LINE_FLAG1_INTR | \
					 WIN0_EMPTY_INTR | WIN1_EMPTY_INTR | \
					 WIN2_EMPTY_INTR | WIN3_EMPTY_INTR | \
					 HWC_EMPTY_INTR | \
					 POST_BUF_EMPTY_INTR | \
					 DMA_FINISH_INTR | FS_FIELD_INTR | \
					 FE_INTR)

#define DSP_LINE_NUM(x)			(((x) & 0x1fff) << 12)
#define DSP_LINE_NUM_MASK		(0x1fff << 12)

/* src alpha ctrl define */
#define SRC_FADING_VALUE(x)		(((x) & 0xff) << 24)
#define SRC_GLOBAL_ALPHA(x)		(((x) & 0xff) << 16)
#define SRC_FACTOR_M0(x)		(((x) & 0x7) << 6)
#define SRC_ALPHA_CAL_M0(x)		(((x) & 0x1) << 5)
#define SRC_BLEND_M0(x)			(((x) & 0x3) << 3)
#define SRC_ALPHA_M0(x)			(((x) & 0x1) << 2)
#define SRC_COLOR_M0(x)			(((x) & 0x1) << 1)
#define SRC_ALPHA_EN(x)			(((x) & 0x1) << 0)
/* dst alpha ctrl define */
#define DST_FACTOR_M0(x)		(((x) & 0x7) << 6)

/*
 * display output interface supported by rockchip lcdc
 */
#define ROCKCHIP_OUT_MODE_P888		0
#define ROCKCHIP_OUT_MODE_P666		1
#define ROCKCHIP_OUT_MODE_P565		2
#define ROCKCHIP_OUT_MODE_S888		8
#define ROCKCHIP_OUT_MODE_S888_DUMMY	12
#define ROCKCHIP_OUT_MODE_YUV420	14
/* for use special outface */
#define ROCKCHIP_OUT_MODE_AAAA		15

#define ROCKCHIP_OUT_MODE_TYPE(x)	((x) >> 16)
#define ROCKCHIP_OUT_MODE(x)		((x) & 0xffff)
#define ROCKCHIP_DSP_MODE(type, mode) \
		(DRM_MODE_CONNECTOR_##type << 16) | \
		(ROCKCHIP_OUT_MODE_##mode & 0xffff)

enum alpha_mode {
	ALPHA_STRAIGHT,
	ALPHA_INVERSE,
};

enum global_blend_mode {
	ALPHA_GLOBAL,
	ALPHA_PER_PIX,
	ALPHA_PER_PIX_GLOBAL,
};

enum alpha_cal_mode {
	ALPHA_SATURATION,
	ALPHA_NO_SATURATION,
};

enum color_mode {
	ALPHA_SRC_PRE_MUL,
	ALPHA_SRC_NO_PRE_MUL,
};

enum factor_mode {
	ALPHA_ZERO,
	ALPHA_ONE,
	ALPHA_SRC,
	ALPHA_SRC_INVERSE,
	ALPHA_SRC_GLOBAL,
};

enum scale_mode {
	SCALE_NONE = 0x0,
	SCALE_UP   = 0x1,
	SCALE_DOWN = 0x2
};

enum lb_mode {
	LB_YUV_3840X5 = 0x0,
	LB_YUV_2560X8 = 0x1,
	LB_RGB_3840X2 = 0x2,
	LB_RGB_2560X4 = 0x3,
	LB_RGB_1920X5 = 0x4,
	LB_RGB_1280X8 = 0x5
};

enum sacle_up_mode {
	SCALE_UP_BIL = 0x0,
	SCALE_UP_BIC = 0x1
};

enum scale_down_mode {
	SCALE_DOWN_BIL = 0x0,
	SCALE_DOWN_AVG = 0x1
};

enum dither_down_mode {
	RGB888_TO_RGB565 = 0x0,
	RGB888_TO_RGB666 = 0x1
};

enum dither_down_mode_sel {
	DITHER_DOWN_ALLEGRO = 0x0,
	DITHER_DOWN_FRC = 0x1
};

enum vop_pol {
	HSYNC_POSITIVE = 0,
	VSYNC_POSITIVE = 1,
	DEN_NEGATIVE   = 2,
};

enum vop_pdaf_mode {
	VOP_HOLD_MODE = 0,
	VOP_NORMAL_MODE,
	VOP_PINGPONG_MODE,
	VOP_BYPASS_MODE,
	VOP_BACKGROUND_MODE,
	VOP_ONEFRAME_MODE,
	VOP_ONEFRAME_NOSEND_MODE
};

enum vop_pdaf_type {
	VOP_PDAF_TYPE_DEFAULT = 0,
	VOP_PDAF_TYPE_HBLANK,
	VOP_PDAF_TYPE_VBLANK,
	VOP_PDAF_TYPE_HBLANK_VBLANK,
	VOP_PDAF_TYPE_INTERWEAVE,
};

#define FRAC_16_16(mult, div)    (((mult) << 16) / (div))
#define SCL_FT_DEFAULT_FIXPOINT_SHIFT	12
#define SCL_MAX_VSKIPLINES		4
#define MIN_SCL_FT_AFTER_VSKIP		1

static inline uint16_t scl_cal_scale(int src, int dst, int shift)
{
	return ((src * 2 - 3) << (shift - 1)) / (dst - 1);
}

static inline uint16_t scl_cal_scale2(int src, int dst)
{
	return ((src - 1) << 12) / (dst - 1);
}

#define GET_SCL_FT_BILI_DN(src, dst)	scl_cal_scale(src, dst, 12)
#define GET_SCL_FT_BILI_UP(src, dst)	scl_cal_scale(src, dst, 16)
#define GET_SCL_FT_BIC(src, dst)	scl_cal_scale(src, dst, 16)

static inline uint16_t scl_get_bili_dn_vskip(int src_h, int dst_h,
					     int vskiplines)
{
	int act_height;

	act_height = (src_h + vskiplines - 1) / vskiplines;

	if (act_height == dst_h)
		return GET_SCL_FT_BILI_DN(src_h, dst_h) / vskiplines;

	return GET_SCL_FT_BILI_DN(act_height, dst_h);
}

static inline enum scale_mode scl_get_scl_mode(int src, int dst)
{
	if (src < dst)
		return SCALE_UP;
	else if (src > dst)
		return SCALE_DOWN;

	return SCALE_NONE;
}

static inline int scl_get_vskiplines(uint32_t srch, uint32_t dsth)
{
	uint32_t vskiplines;

	for (vskiplines = SCL_MAX_VSKIPLINES; vskiplines > 1; vskiplines /= 2)
		if (srch >= vskiplines * dsth * MIN_SCL_FT_AFTER_VSKIP)
			break;

	return vskiplines;
}

static inline int scl_vop_cal_lb_mode(int width, bool is_yuv)
{
	int lb_mode;

	if (!is_yuv && (width > 2560))
		lb_mode = LB_RGB_3840X2;
	else if (!is_yuv && (width > 1920))
		lb_mode = LB_RGB_2560X4;
	else if (!is_yuv)
		lb_mode = LB_RGB_1920X5;
	else if (width > 1280)
		lb_mode = LB_YUV_3840X5;
	else
		lb_mode = LB_YUV_2560X8;

	return lb_mode;
}

static inline int us_to_vertical_line(struct drm_display_mode *mode, int us)
{
	return us * mode->clock / mode->htotal / 1000;
}

static inline int interpolate(int x1, int y1, int x2, int y2, int x)
{
	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

extern const struct component_ops vop_component_ops;

#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
int drm_debugfs_vop_add(struct drm_crtc *crtc, struct dentry *root);
int vop_plane_dump(struct vop_dump_info *dump_info, int frame_count);
#endif

#endif /* _ROCKCHIP_DRM_VOP_H */
