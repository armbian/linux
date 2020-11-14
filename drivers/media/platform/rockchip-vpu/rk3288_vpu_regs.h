/*
 * Rockchip RK3288 VPU codec driver
 *
 * Copyright (C) 2014 Google, Inc.
 *	Tomasz Figa <tfiga@chromium.org>
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

#ifndef RK3288_VPU_REGS_H_
#define RK3288_VPU_REGS_H_

/* Encoder registers. */
#define VEPU_REG_INTERRUPT			0x004
#define     VEPU_REG_INTERRUPT_DIS_BIT		BIT(1)
#define     VEPU_REG_INTERRUPT_BIT		BIT(0)
#define VEPU_REG_AXI_CTRL			0x008
#define     VEPU_REG_AXI_CTRL_OUTPUT_SWAP16	BIT(15)
#define     VEPU_REG_AXI_CTRL_INPUT_SWAP16	BIT(14)
#define     VEPU_REG_AXI_CTRL_BURST_LEN(x)	((x) << 8)
#define     VEPU_REG_AXI_CTRL_GATE_BIT		BIT(4)
#define     VEPU_REG_AXI_CTRL_OUTPUT_SWAP32	BIT(3)
#define     VEPU_REG_AXI_CTRL_INPUT_SWAP32	BIT(2)
#define     VEPU_REG_AXI_CTRL_OUTPUT_SWAP8	BIT(1)
#define     VEPU_REG_AXI_CTRL_INPUT_SWAP8	BIT(0)
#define VEPU_REG_ADDR_OUTPUT_STREAM		0x014
#define VEPU_REG_ADDR_OUTPUT_CTRL		0x018
#define VEPU_REG_ADDR_REF_LUMA			0x01c
#define VEPU_REG_ADDR_REF_CHROMA		0x020
#define VEPU_REG_ADDR_REC_LUMA			0x024
#define VEPU_REG_ADDR_REC_CHROMA		0x028
#define VEPU_REG_ADDR_IN_LUMA			0x02c
#define VEPU_REG_ADDR_IN_CB			0x030
#define VEPU_REG_ADDR_IN_CR			0x034
#define VEPU_REG_ENC_CTRL			0x038
#define     VEPU_REG_ENC_CTRL_TIMEOUT_EN	BIT(31)
#define     VEPU_REG_ENC_CTRL_NAL_MODE_BIT	BIT(29)
#define     VEPU_REG_ENC_CTRL_WIDTH(w)		((w) << 19)
#define     VEPU_REG_ENC_CTRL_HEIGHT(h)		((h) << 10)
#define     VEPU_REG_PIC_TYPE(x)		(((x) & 0x3) << 3)
#define     VEPU_REG_ENC_CTRL_KEYFRAME_BIT	BIT(3)
#define     VEPU_REG_ENC_CTRL_ENC_MODE_H264	(0x3 << 1)
#define     VEPU_REG_ENC_CTRL_ENC_MODE_VP8	(0x1 << 1)
#define     VEPU_REG_ENC_CTRL_EN_BIT		BIT(0)
#define VEPU_REG_IN_IMG_CTRL			0x03c
#define     VEPU_REG_IN_IMG_CTRL_ROW_LEN(x)	((x) << 12)
#define     VEPU_REG_IN_IMG_CTRL_OVRFLR_D4(x)	((x) << 10)
#define     VEPU_REG_IN_IMG_CTRL_OVRFLB_D4(x)	((x) << 6)
#define     VEPU_REG_IN_IMG_CTRL_FMT(x)		((x) << 2)
#define VEPU_REG_ENC_CTRL0			0x040
#define    VEPU_REG_ENC_CTRL0_INIT_QP(x)		((x) << 26)
#define    VEPU_REG_ENC_CTRL0_SLICE_ALPHA(x)		((x) << 22)
#define    VEPU_REG_ENC_CTRL0_SLICE_BETA(x)		((x) << 18)
#define    VEPU_REG_ENC_CTRL0_CHROMA_QP_OFFSET(x)	((x) << 13)
#define    VEPU_REG_ENC_CTRL0_FILTER_DIS(x)		((x) << 5)
#define    VEPU_REG_ENC_CTRL0_IDR_PICID(x)		((x) << 1)
#define    VEPU_REG_ENC_CTRL0_CONSTR_INTRA_PRED	BIT(0)
#define VEPU_REG_ENC_CTRL1			0x044
#define    VEPU_REG_ENC_CTRL1_PPS_ID(x)			((x) << 24)
#define    VEPU_REG_ENC_CTRL1_INTRA_PRED_MODE(x)	((x) << 16)
#define    VEPU_REG_ENC_CTRL1_FRAME_NUM(x)		((x))
#define VEPU_REG_ENC_CTRL2			0x048
#define    VEPU_REG_ENC_CTRL2_DEBLOCKING_FILETER_MODE(x)	((x) << 30)
#define    VEPU_REG_ENC_CTRL2_H264_SLICE_SIZE(x)		((x) << 23)
#define    VEPU_REG_ENC_CTRL2_DISABLE_QUARTER_PIXMV		BIT(22)
#define    VEPU_REG_ENC_CTRL2_TRANS8X8_MODE_EN			BIT(21)
#define    VEPU_REG_ENC_CTRL2_CABAC_INIT_IDC(x)			((x) << 19)
#define    VEPU_REG_ENC_CTRL2_ENTROPY_CODING_MODE		BIT(18)
#define    VEPU_REG_ENC_CTRL2_H264_INTER4X4_MODE		BIT(17)
#define    VEPU_REG_ENC_CTRL2_H264_STREAM_MODE			BIT(16)
#define    VEPU_REG_ENC_CTRL2_INTRA16X16_MODE(x)		((x))
#define VEPU_REG_ENC_CTRL3			0x04c
#define    VEPU_REG_ENC_CTRL3_MUTIMV_EN			BIT(30)
#define    VEPU_REG_ENC_CTRL3_MV_PENALTY_1_4P(x)	((x) << 20)
#define    VEPU_REG_ENC_CTRL3_MV_PENALTY_4P(x)		((x) << 10)
#define    VEPU_REG_ENC_CTRL3_MV_PENALTY_1P(x)		((x))
#define VEPU_REG_ENC_CTRL4			0x050
#define    VEPU_REG_ENC_CTRL4_MV_PENALTY_16X8_8X16(x)	((x) << 20)
#define    VEPU_REG_ENC_CTRL4_MV_PENALTY_8X8(x)		((x) << 10)
#define    VEPU_REG_ENC_CTRL4_8X4_4X8(x)		((x))
#define VEPU_REG_ENC_CTRL5			0x054
#define    VEPU_REG_ENC_CTRL5_MACROBLOCK_PENALTY(x)	((x) << 24)
#define    VEPU_REG_ENC_CTRL5_COMPLETE_SLICES(x)	((x) << 16)
#define    VEPU_REG_ENC_CTRL5_INTER_MODE(x)		((x))
#define VEPU_REG_STR_HDR_REM_MSB		0x058
#define VEPU_REG_STR_HDR_REM_LSB		0x05c
#define VEPU_REG_STR_BUF_LIMIT			0x060
#define VEPU_REG_MAD_CTRL			0x064
#define    VEPU_REG_MAD_CTRL_QP_ADJUST(x)	((x) << 28)
#define    VEPU_REG_MAD_CTRL_MAD_THREDHOLD(x)	((x) << 22)
#define    VEPU_REG_MAD_CTRL_QP_SUM_DIV2(x)	((x))
#define VEPU_REG_ADDR_VP8_PROB_CNT		0x068
#define VEPU_REG_QP_VAL				0x06c
#define    VEPU_REG_QP_VAL_LUM(x)		((x) << 26)
#define    VEPU_REG_QP_VAL_MAX(x)		((x) << 20)
#define    VEPU_REG_QP_VAL_MIN(x)		((x) << 14)
#define    VEPU_REG_QP_VAL_CHECKPOINT_DISTAN(x)	((x))
#define VEPU_REG_VP8_QP_VAL(i)			(0x06c + ((i) * 0x4))
#define VEPU_REG_CHECKPOINT(i)			(0x070 + ((i) * 0x4))
#define     VEPU_REG_CHECKPOINT_CHECK0(x)	(((x) & 0xffff))
#define     VEPU_REG_CHECKPOINT_CHECK1(x)	(((x) & 0xffff) << 16)
#define     VEPU_REG_CHECKPOINT_RESULT(x)	((((x) >> (16 - 16 \
						 * (i & 1))) & 0xffff) \
						 * 32)
#define VEPU_REG_CHKPT_WORD_ERR(i)		(0x084 + ((i) * 0x4))
#define     VEPU_REG_CHKPT_WORD_ERR_CHK0(x)	(((x) & 0xffff))
#define     VEPU_REG_CHKPT_WORD_ERR_CHK1(x)	(((x) & 0xffff) << 16)
#define VEPU_REG_VP8_BOOL_ENC			0x08c
#define VEPU_REG_CHKPT_DELTA_QP			0x090
#define     VEPU_REG_CHKPT_DELTA_QP_CHK0(x)	(((x) & 0x0f) << 0)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK1(x)	(((x) & 0x0f) << 4)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK2(x)	(((x) & 0x0f) << 8)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK3(x)	(((x) & 0x0f) << 12)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK4(x)	(((x) & 0x0f) << 16)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK5(x)	(((x) & 0x0f) << 20)
#define     VEPU_REG_CHKPT_DELTA_QP_CHK6(x)	(((x) & 0x0f) << 24)
#define VEPU_REG_VP8_CTRL0			0x090
#define VEPU_REG_RLC_CTRL			0x094
#define     VEPU_REG_RLC_CTRL_STR_OFFS_SHIFT	23
#define     VEPU_REG_RLC_CTRL_STR_OFFS_MASK	(0x3f << 23)
#define     VEPU_REG_RLC_CTRL_RLC_SUM(x)	((x))
#define VEPU_REG_MB_CTRL			0x098
#define     VEPU_REG_MB_CNT_OUT(x)		(((x) & 0xffff))
#define     VEPU_REG_MB_CNT_SET(x)		(((x) & 0xffff) << 16)
#define VEPU_REG_ADDR_NEXT_PIC			0x09c
#define VEPU_REG_STABLILIZATION_OUTPUT		0x0A0
#define VEPU_REG_ADDR_CABAC_TBL			0x0cc
#define VEPU_REG_ADDR_MV_OUT			0x0d0
#define VEPU_REG_RGB_YUV_COEFF(i)		(0x0d4 + ((i) * 0x4))
#define VEPU_REG_RGB_MASK_MSB			0x0dc
#define VEPU_REG_INTRA_AREA_CTRL		0x0e0
#define VEPU_REG_CIR_INTRA_CTRL			0x0e4
#define VEPU_REG_INTRA_SLICE_BITMAP(i)		(0x0e8 + ((i) * 0x4))
#define VEPU_REG_ADDR_VP8_DCT_PART(i)		(0x0e8 + ((i) * 0x4))
#define VEPU_REG_FIRST_ROI_AREA			0x0f0
#define VEPU_REG_SECOND_ROI_AREA		0x0f4
#define VEPU_REG_MVC_CTRL			0x0f8
#define	VEPU_REG_MVC_CTRL_MV16X16_FAVOR(x)	((x) << 28)
#define VEPU_REG_VP8_INTRA_PENALTY(i)		(0x100 + ((i) * 0x4))
#define VEPU_REG_ADDR_VP8_SEG_MAP		0x11c
#define VEPU_REG_VP8_SEG_QP(i)			(0x120 + ((i) * 0x4))
#define VEPU_REG_DMV_4P_1P_PENALTY(i)		(0x180 + ((i) * 0x4))
#define     VEPU_REG_DMV_4P_1P_PENALTY_BIT(x, i)	(x << i * 8)
#define VEPU_REG_DMV_QPEL_PENALTY(i)		(0x200 + ((i) * 0x4))
#define     VEPU_REG_DMV_QPEL_PENALTY_BIT(x, i)	(x << i * 8)
#define VEPU_REG_VP8_CTRL1			0x280
#define VEPU_REG_VP8_BIT_COST_GOLDEN		0x284
#define VEPU_REG_VP8_LOOP_FLT_DELTA(i)		(0x288 + ((i) * 0x4))

/* Decoder registers. */
#define VDPU_REG_INTERRUPT			0x004
#define     VDPU_REG_INTERRUPT_DEC_PIC_INF		BIT(24)
#define     VDPU_REG_INTERRUPT_DEC_TIMEOUT		BIT(18)
#define     VDPU_REG_INTERRUPT_DEC_SLICE_INT		BIT(17)
#define     VDPU_REG_INTERRUPT_DEC_ERROR_INT		BIT(16)
#define     VDPU_REG_INTERRUPT_DEC_ASO_INT		BIT(15)
#define     VDPU_REG_INTERRUPT_DEC_BUFFER_INT		BIT(14)
#define     VDPU_REG_INTERRUPT_DEC_BUS_INT		BIT(13)
#define     VDPU_REG_INTERRUPT_DEC_RDY_INT		BIT(12)
#define     VDPU_REG_INTERRUPT_DEC_IRQ			BIT(8)
#define     VDPU_REG_INTERRUPT_DEC_IRQ_DIS		BIT(4)
#define     VDPU_REG_INTERRUPT_DEC_E			BIT(0)
#define VDPU_REG_CONFIG				0x008
#define     VDPU_REG_CONFIG_DEC_AXI_RD_ID(x)		(((x) & 0xff) << 24)
#define     VDPU_REG_CONFIG_DEC_TIMEOUT_E		BIT(23)
#define     VDPU_REG_CONFIG_DEC_STRSWAP32_E		BIT(22)
#define     VDPU_REG_CONFIG_DEC_STRENDIAN_E		BIT(21)
#define     VDPU_REG_CONFIG_DEC_INSWAP32_E		BIT(20)
#define     VDPU_REG_CONFIG_DEC_OUTSWAP32_E		BIT(19)
#define     VDPU_REG_CONFIG_DEC_DATA_DISC_E		BIT(18)
#define     VDPU_REG_CONFIG_TILED_MODE_MSB		BIT(17)
#define     VDPU_REG_CONFIG_DEC_OUT_TILED_E		BIT(17)
#define     VDPU_REG_CONFIG_DEC_LATENCY(x)		(((x) & 0x3f) << 11)
#define     VDPU_REG_CONFIG_DEC_CLK_GATE_E		BIT(10)
#define     VDPU_REG_CONFIG_DEC_IN_ENDIAN		BIT(9)
#define     VDPU_REG_CONFIG_DEC_OUT_ENDIAN		BIT(8)
#define     VDPU_REG_CONFIG_PRIORITY_MODE(x)		(((x) & 0x7) << 5)
#define     VDPU_REG_CONFIG_TILED_MODE_LSB		BIT(7)
#define     VDPU_REG_CONFIG_DEC_ADV_PRE_DIS		BIT(6)
#define     VDPU_REG_CONFIG_DEC_SCMD_DIS		BIT(5)
#define     VDPU_REG_CONFIG_DEC_MAX_BURST(x)		(((x) & 0x1f) << 0)
#define VDPU_REG_DEC_CTRL0			0x00c
#define     VDPU_REG_DEC_CTRL0_DEC_MODE(x)		(((x) & 0xf) << 28)
#define     VDPU_REG_DEC_CTRL0_RLC_MODE_E		BIT(27)
#define     VDPU_REG_DEC_CTRL0_SKIP_MODE		BIT(26)
#define     VDPU_REG_DEC_CTRL0_DIVX3_E			BIT(25)
#define     VDPU_REG_DEC_CTRL0_PJPEG_E			BIT(24)
#define     VDPU_REG_DEC_CTRL0_PIC_INTERLACE_E		BIT(23)
#define     VDPU_REG_DEC_CTRL0_PIC_FIELDMODE_E		BIT(22)
#define     VDPU_REG_DEC_CTRL0_PIC_B_E			BIT(21)
#define     VDPU_REG_DEC_CTRL0_PIC_INTER_E		BIT(20)
#define     VDPU_REG_DEC_CTRL0_PIC_TOPFIELD_E		BIT(19)
#define     VDPU_REG_DEC_CTRL0_FWD_INTERLACE_E		BIT(18)
#define     VDPU_REG_DEC_CTRL0_SORENSON_E		BIT(17)
#define     VDPU_REG_DEC_CTRL0_REF_TOPFIELD_E		BIT(16)
#define     VDPU_REG_DEC_CTRL0_DEC_OUT_DIS		BIT(15)
#define     VDPU_REG_DEC_CTRL0_FILTERING_DIS		BIT(14)
#define     VDPU_REG_DEC_CTRL0_WEBP_E			BIT(13)
#define     VDPU_REG_DEC_CTRL0_MVC_E			BIT(13)
#define     VDPU_REG_DEC_CTRL0_PIC_FIXED_QUANT		BIT(13)
#define     VDPU_REG_DEC_CTRL0_WRITE_MVS_E		BIT(12)
#define     VDPU_REG_DEC_CTRL0_REFTOPFIRST_E		BIT(11)
#define     VDPU_REG_DEC_CTRL0_SEQ_MBAFF_E		BIT(10)
#define     VDPU_REG_DEC_CTRL0_PICORD_COUNT_E		BIT(9)
#define     VDPU_REG_DEC_CTRL0_DEC_AHB_HLOCK_E		BIT(8)
#define     VDPU_REG_DEC_CTRL0_DEC_AXI_WR_ID(x)		(((x) & 0xff) << 0)
#define VDPU_REG_DEC_CTRL1			0x010
#define     VDPU_REG_DEC_CTRL1_PIC_MB_WIDTH(x)		(((x) & 0x1ff) << 23)
#define     VDPU_REG_DEC_CTRL1_MB_WIDTH_OFF(x)		(((x) & 0xf) << 19)
#define     VDPU_REG_DEC_CTRL1_PIC_MB_HEIGHT_P(x)	(((x) & 0xff) << 11)
#define     VDPU_REG_DEC_CTRL1_MB_HEIGHT_OFF(x)		(((x) & 0xf) << 7)
#define     VDPU_REG_DEC_CTRL1_ALT_SCAN_E		BIT(6)
#define     VDPU_REG_DEC_CTRL1_TOPFIELDFIRST_E		BIT(5)
#define     VDPU_REG_DEC_CTRL1_REF_FRAMES(x)		(((x) & 0x1f) << 0)
#define     VDPU_REG_DEC_CTRL1_PIC_MB_W_EXT(x)		(((x) & 0x7) << 3)
#define     VDPU_REG_DEC_CTRL1_PIC_MB_H_EXT(x)		(((x) & 0x7) << 0)
#define     VDPU_REG_DEC_CTRL1_PIC_REFER_FLAG		BIT(0)
#define VDPU_REG_DEC_CTRL2			0x014
#define     VDPU_REG_DEC_CTRL2_STRM_START_BIT(x)	(((x) & 0x3f) << 26)
#define     VDPU_REG_DEC_CTRL2_SYNC_MARKER_E		BIT(25)
#define     VDPU_REG_DEC_CTRL2_TYPE1_QUANT_E		BIT(24)
#define     VDPU_REG_DEC_CTRL2_CH_QP_OFFSET(x)		(((x) & 0x1f) << 19)
#define     VDPU_REG_DEC_CTRL2_CH_QP_OFFSET2(x)		(((x) & 0x1f) << 14)
#define     VDPU_REG_DEC_CTRL2_FIELDPIC_FLAG_E		BIT(0)
#define     VDPU_REG_DEC_CTRL2_INTRADC_VLC_THR(x)	(((x) & 0x7) << 16)
#define     VDPU_REG_DEC_CTRL2_VOP_TIME_INCR(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL2_DQ_PROFILE		BIT(24)
#define     VDPU_REG_DEC_CTRL2_DQBI_LEVEL		BIT(23)
#define     VDPU_REG_DEC_CTRL2_RANGE_RED_FRM_E		BIT(22)
#define     VDPU_REG_DEC_CTRL2_FAST_UVMC_E		BIT(20)
#define     VDPU_REG_DEC_CTRL2_TRANSDCTAB		BIT(17)
#define     VDPU_REG_DEC_CTRL2_TRANSACFRM(x)		(((x) & 0x3) << 15)
#define     VDPU_REG_DEC_CTRL2_TRANSACFRM2(x)		(((x) & 0x3) << 13)
#define     VDPU_REG_DEC_CTRL2_MB_MODE_TAB(x)		(((x) & 0x7) << 10)
#define     VDPU_REG_DEC_CTRL2_MVTAB(x)			(((x) & 0x7) << 7)
#define     VDPU_REG_DEC_CTRL2_CBPTAB(x)		(((x) & 0x7) << 4)
#define     VDPU_REG_DEC_CTRL2_2MV_BLK_PAT_TAB(x)	(((x) & 0x3) << 2)
#define     VDPU_REG_DEC_CTRL2_4MV_BLK_PAT_TAB(x)	(((x) & 0x3) << 0)
#define     VDPU_REG_DEC_CTRL2_QSCALE_TYPE		BIT(24)
#define     VDPU_REG_DEC_CTRL2_CON_MV_E			BIT(4)
#define     VDPU_REG_DEC_CTRL2_INTRA_DC_PREC(x)		(((x) & 0x3) << 2)
#define     VDPU_REG_DEC_CTRL2_INTRA_VLC_TAB		BIT(1)
#define     VDPU_REG_DEC_CTRL2_FRAME_PRED_DCT		BIT(0)
#define     VDPU_REG_DEC_CTRL2_JPEG_QTABLES(x)		(((x) & 0x3) << 11)
#define     VDPU_REG_DEC_CTRL2_JPEG_MODE(x)		(((x) & 0x7) << 8)
#define     VDPU_REG_DEC_CTRL2_JPEG_FILRIGHT_E		BIT(7)
#define     VDPU_REG_DEC_CTRL2_JPEG_STREAM_ALL		BIT(6)
#define     VDPU_REG_DEC_CTRL2_CR_AC_VLCTABLE		BIT(5)
#define     VDPU_REG_DEC_CTRL2_CB_AC_VLCTABLE		BIT(4)
#define     VDPU_REG_DEC_CTRL2_CR_DC_VLCTABLE		BIT(3)
#define     VDPU_REG_DEC_CTRL2_CB_DC_VLCTABLE		BIT(2)
#define     VDPU_REG_DEC_CTRL2_CR_DC_VLCTABLE3		BIT(1)
#define     VDPU_REG_DEC_CTRL2_CB_DC_VLCTABLE3		BIT(0)
#define     VDPU_REG_DEC_CTRL2_STRM1_START_BIT(x)	(((x) & 0x3f) << 18)
#define     VDPU_REG_DEC_CTRL2_HUFFMAN_E		BIT(17)
#define     VDPU_REG_DEC_CTRL2_MULTISTREAM_E		BIT(16)
#define     VDPU_REG_DEC_CTRL2_BOOLEAN_VALUE(x)		(((x) & 0xff) << 8)
#define     VDPU_REG_DEC_CTRL2_BOOLEAN_RANGE(x)		(((x) & 0xff) << 0)
#define     VDPU_REG_DEC_CTRL2_ALPHA_OFFSET(x)		(((x) & 0x1f) << 5)
#define     VDPU_REG_DEC_CTRL2_BETA_OFFSET(x)		(((x) & 0x1f) << 0)
#define VDPU_REG_DEC_CTRL3			0x018
#define     VDPU_REG_DEC_CTRL3_START_CODE_E		BIT(31)
#define     VDPU_REG_DEC_CTRL3_INIT_QP(x)		(((x) & 0x3f) << 25)
#define     VDPU_REG_DEC_CTRL3_CH_8PIX_ILEAV_E		BIT(24)
#define     VDPU_REG_DEC_CTRL3_STREAM_LEN_EXT(x)	(((x) & 0xff) << 24)
#define     VDPU_REG_DEC_CTRL3_STREAM_LEN(x)		(((x) & 0xffffff) << 0)
#define VDPU_REG_DEC_CTRL4			0x01c
#define     VDPU_REG_DEC_CTRL4_CABAC_E			BIT(31)
#define     VDPU_REG_DEC_CTRL4_BLACKWHITE_E		BIT(30)
#define     VDPU_REG_DEC_CTRL4_DIR_8X8_INFER_E		BIT(29)
#define     VDPU_REG_DEC_CTRL4_WEIGHT_PRED_E		BIT(28)
#define     VDPU_REG_DEC_CTRL4_WEIGHT_BIPR_IDC(x)	(((x) & 0x3) << 26)
#define     VDPU_REG_DEC_CTRL4_AVS_H264_H_EXT		BIT(25)
#define     VDPU_REG_DEC_CTRL4_FRAMENUM_LEN(x)		(((x) & 0x1f) << 16)
#define     VDPU_REG_DEC_CTRL4_FRAMENUM(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL4_BITPLANE0_E		BIT(31)
#define     VDPU_REG_DEC_CTRL4_BITPLANE1_E		BIT(30)
#define     VDPU_REG_DEC_CTRL4_BITPLANE2_E		BIT(29)
#define     VDPU_REG_DEC_CTRL4_ALT_PQUANT(x)		(((x) & 0x1f) << 24)
#define     VDPU_REG_DEC_CTRL4_DQ_EDGES(x)		(((x) & 0xf) << 20)
#define     VDPU_REG_DEC_CTRL4_TTMBF			BIT(19)
#define     VDPU_REG_DEC_CTRL4_PQINDEX(x)		(((x) & 0x1f) << 14)
#define     VDPU_REG_DEC_CTRL4_VC1_HEIGHT_EXT		BIT(13)
#define     VDPU_REG_DEC_CTRL4_BILIN_MC_E		BIT(12)
#define     VDPU_REG_DEC_CTRL4_UNIQP_E			BIT(11)
#define     VDPU_REG_DEC_CTRL4_HALFQP_E			BIT(10)
#define     VDPU_REG_DEC_CTRL4_TTFRM(x)			(((x) & 0x3) << 8)
#define     VDPU_REG_DEC_CTRL4_2ND_BYTE_EMUL_E		BIT(7)
#define     VDPU_REG_DEC_CTRL4_DQUANT_E			BIT(6)
#define     VDPU_REG_DEC_CTRL4_VC1_ADV_E		BIT(5)
#define     VDPU_REG_DEC_CTRL4_PJPEG_FILDOWN_E		BIT(26)
#define     VDPU_REG_DEC_CTRL4_PJPEG_WDIV8		BIT(25)
#define     VDPU_REG_DEC_CTRL4_PJPEG_HDIV8		BIT(24)
#define     VDPU_REG_DEC_CTRL4_PJPEG_AH(x)		(((x) & 0xf) << 20)
#define     VDPU_REG_DEC_CTRL4_PJPEG_AL(x)		(((x) & 0xf) << 16)
#define     VDPU_REG_DEC_CTRL4_PJPEG_SS(x)		(((x) & 0xff) << 8)
#define     VDPU_REG_DEC_CTRL4_PJPEG_SE(x)		(((x) & 0xff) << 0)
#define     VDPU_REG_DEC_CTRL4_DCT1_START_BIT(x)	(((x) & 0x3f) << 26)
#define     VDPU_REG_DEC_CTRL4_DCT2_START_BIT(x)	(((x) & 0x3f) << 20)
#define     VDPU_REG_DEC_CTRL4_CH_MV_RES		BIT(13)
#define     VDPU_REG_DEC_CTRL4_INIT_DC_MATCH0(x)	(((x) & 0x7) << 9)
#define     VDPU_REG_DEC_CTRL4_INIT_DC_MATCH1(x)	(((x) & 0x7) << 6)
#define     VDPU_REG_DEC_CTRL4_VP7_VERSION		BIT(5)
#define VDPU_REG_DEC_CTRL5			0x020
#define     VDPU_REG_DEC_CTRL5_CONST_INTRA_E		BIT(31)
#define     VDPU_REG_DEC_CTRL5_FILT_CTRL_PRES		BIT(30)
#define     VDPU_REG_DEC_CTRL5_RDPIC_CNT_PRES		BIT(29)
#define     VDPU_REG_DEC_CTRL5_8X8TRANS_FLAG_E		BIT(28)
#define     VDPU_REG_DEC_CTRL5_REFPIC_MK_LEN(x)		(((x) & 0x7ff) << 17)
#define     VDPU_REG_DEC_CTRL5_IDR_PIC_E		BIT(16)
#define     VDPU_REG_DEC_CTRL5_IDR_PIC_ID(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL5_MV_SCALEFACTOR(x)	(((x) & 0xff) << 24)
#define     VDPU_REG_DEC_CTRL5_REF_DIST_FWD(x)		(((x) & 0x1f) << 19)
#define     VDPU_REG_DEC_CTRL5_REF_DIST_BWD(x)		(((x) & 0x1f) << 14)
#define     VDPU_REG_DEC_CTRL5_LOOP_FILT_LIMIT(x)	(((x) & 0xf) << 14)
#define     VDPU_REG_DEC_CTRL5_VARIANCE_TEST_E		BIT(13)
#define     VDPU_REG_DEC_CTRL5_MV_THRESHOLD(x)		(((x) & 0x7) << 10)
#define     VDPU_REG_DEC_CTRL5_VAR_THRESHOLD(x)		(((x) & 0x3ff) << 0)
#define     VDPU_REG_DEC_CTRL5_DIVX_IDCT_E		BIT(8)
#define     VDPU_REG_DEC_CTRL5_DIVX3_SLICE_SIZE(x)	(((x) & 0xff) << 0)
#define     VDPU_REG_DEC_CTRL5_PJPEG_REST_FREQ(x)	(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL5_RV_PROFILE(x)		(((x) & 0x3) << 30)
#define     VDPU_REG_DEC_CTRL5_RV_OSV_QUANT(x)		(((x) & 0x3) << 28)
#define     VDPU_REG_DEC_CTRL5_RV_FWD_SCALE(x)		(((x) & 0x3fff) << 14)
#define     VDPU_REG_DEC_CTRL5_RV_BWD_SCALE(x)		(((x) & 0x3fff) << 0)
#define     VDPU_REG_DEC_CTRL5_INIT_DC_COMP0(x)		(((x) & 0xffff) << 16)
#define     VDPU_REG_DEC_CTRL5_INIT_DC_COMP1(x)		(((x) & 0xffff) << 0)
#define VDPU_REG_DEC_CTRL6			0x024
#define     VDPU_REG_DEC_CTRL6_PPS_ID(x)		(((x) & 0xff) << 24)
#define     VDPU_REG_DEC_CTRL6_REFIDX1_ACTIVE(x)	(((x) & 0x1f) << 19)
#define     VDPU_REG_DEC_CTRL6_REFIDX0_ACTIVE(x)	(((x) & 0x1f) << 14)
#define     VDPU_REG_DEC_CTRL6_POC_LENGTH(x)		(((x) & 0xff) << 0)
#define     VDPU_REG_DEC_CTRL6_ICOMP0_E			BIT(24)
#define     VDPU_REG_DEC_CTRL6_ISCALE0(x)		(((x) & 0xff) << 16)
#define     VDPU_REG_DEC_CTRL6_ISHIFT0(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL6_STREAM1_LEN(x)		(((x) & 0xffffff) << 0)
#define     VDPU_REG_DEC_CTRL6_PIC_SLICE_AM(x)		(((x) & 0x1fff) << 0)
#define     VDPU_REG_DEC_CTRL6_COEFFS_PART_AM(x)	(((x) & 0xf) << 24)
#define VDPU_REG_FWD_PIC(i)			(0x028 + ((i) * 0x4))
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F5(x)		(((x) & 0x1f) << 25)
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F4(x)		(((x) & 0x1f) << 20)
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F3(x)		(((x) & 0x1f) << 15)
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F2(x)		(((x) & 0x1f) << 10)
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F1(x)		(((x) & 0x1f) << 5)
#define     VDPU_REG_FWD_PIC_PINIT_RLIST_F0(x)		(((x) & 0x1f) << 0)
#define     VDPU_REG_FWD_PIC1_ICOMP1_E			BIT(24)
#define     VDPU_REG_FWD_PIC1_ISCALE1(x)		(((x) & 0xff) << 16)
#define     VDPU_REG_FWD_PIC1_ISHIFT1(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_FWD_PIC1_SEGMENT_BASE(x)		((x) << 0)
#define     VDPU_REG_FWD_PIC1_SEGMENT_UPD_E		BIT(1)
#define     VDPU_REG_FWD_PIC1_SEGMENT_E			BIT(0)
#define VDPU_REG_DEC_CTRL7			0x02c
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F15(x)	(((x) & 0x1f) << 25)
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F14(x)	(((x) & 0x1f) << 20)
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F13(x)	(((x) & 0x1f) << 15)
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F12(x)	(((x) & 0x1f) << 10)
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F11(x)	(((x) & 0x1f) << 5)
#define     VDPU_REG_DEC_CTRL7_PINIT_RLIST_F10(x)	(((x) & 0x1f) << 0)
#define     VDPU_REG_DEC_CTRL7_ICOMP2_E			BIT(24)
#define     VDPU_REG_DEC_CTRL7_ISCALE2(x)		(((x) & 0xff) << 16)
#define     VDPU_REG_DEC_CTRL7_ISHIFT2(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_DEC_CTRL7_DCT3_START_BIT(x)	(((x) & 0x3f) << 24)
#define     VDPU_REG_DEC_CTRL7_DCT4_START_BIT(x)	(((x) & 0x3f) << 18)
#define     VDPU_REG_DEC_CTRL7_DCT5_START_BIT(x)	(((x) & 0x3f) << 12)
#define     VDPU_REG_DEC_CTRL7_DCT6_START_BIT(x)	(((x) & 0x3f) << 6)
#define     VDPU_REG_DEC_CTRL7_DCT7_START_BIT(x)	(((x) & 0x3f) << 0)
#define VDPU_REG_ADDR_STR			0x030
#define VDPU_REG_ADDR_DST			0x034
#define VDPU_REG_ADDR_REF(i)			(0x038 + ((i) * 0x4))
#define     VDPU_REG_ADDR_REF_FIELD_E			BIT(1)
#define     VDPU_REG_ADDR_REF_TOPC_E			BIT(0)
#define VDPU_REG_REF_PIC(i)			(0x078 + ((i) * 0x4))
#define     VDPU_REG_REF_PIC_FILT_TYPE_E		BIT(31)
#define     VDPU_REG_REF_PIC_FILT_SHARPNESS(x)	(((x) & 0x7) << 28)
#define     VDPU_REG_REF_PIC_MB_ADJ_0(x)		(((x) & 0x7f) << 21)
#define     VDPU_REG_REF_PIC_MB_ADJ_1(x)		(((x) & 0x7f) << 14)
#define     VDPU_REG_REF_PIC_MB_ADJ_2(x)		(((x) & 0x7f) << 7)
#define     VDPU_REG_REF_PIC_MB_ADJ_3(x)		(((x) & 0x7f) << 0)
#define     VDPU_REG_REF_PIC_REFER1_NBR(x)		(((x) & 0xffff) << 16)
#define     VDPU_REG_REF_PIC_REFER0_NBR(x)		(((x) & 0xffff) << 0)
#define     VDPU_REG_REF_PIC_LF_LEVEL_0(x)		(((x) & 0x3f) << 18)
#define     VDPU_REG_REF_PIC_LF_LEVEL_1(x)		(((x) & 0x3f) << 12)
#define     VDPU_REG_REF_PIC_LF_LEVEL_2(x)		(((x) & 0x3f) << 6)
#define     VDPU_REG_REF_PIC_LF_LEVEL_3(x)		(((x) & 0x3f) << 0)
#define     VDPU_REG_REF_PIC_QUANT_DELTA_0(x)	(((x) & 0x1f) << 27)
#define     VDPU_REG_REF_PIC_QUANT_DELTA_1(x)	(((x) & 0x1f) << 22)
#define     VDPU_REG_REF_PIC_QUANT_0(x)			(((x) & 0x7ff) << 11)
#define     VDPU_REG_REF_PIC_QUANT_1(x)			(((x) & 0x7ff) << 0)
#define VDPU_REG_LT_REF				0x098
#define VDPU_REG_VALID_REF			0x09c
#define VDPU_REG_ADDR_QTABLE			0x0a0
#define VDPU_REG_ADDR_DIR_MV			0x0a4
#define VDPU_REG_BD_REF_PIC(i)			(0x0a8 + ((i) * 0x4))
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_B2(x)	(((x) & 0x1f) << 25)
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_F2(x)	(((x) & 0x1f) << 20)
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_B1(x)	(((x) & 0x1f) << 15)
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_F1(x)	(((x) & 0x1f) << 10)
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_B0(x)	(((x) & 0x1f) << 5)
#define     VDPU_REG_BD_REF_PIC_BINIT_RLIST_F0(x)	(((x) & 0x1f) << 0)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_2_M1(x)	(((x) & 0x3) << 10)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_2_4(x)		(((x) & 0x3) << 8)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_4_M1(x)	(((x) & 0x3) << 6)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_4_4(x)		(((x) & 0x3) << 4)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_6_M1(x)	(((x) & 0x3) << 2)
#define     VDPU_REG_BD_REF_PIC_PRED_TAP_6_4(x)		(((x) & 0x3) << 0)
#define     VDPU_REG_BD_REF_PIC_QUANT_DELTA_2(x)	(((x) & 0x1f) << 27)
#define     VDPU_REG_BD_REF_PIC_QUANT_DELTA_3(x)	(((x) & 0x1f) << 22)
#define     VDPU_REG_BD_REF_PIC_QUANT_2(x)		(((x) & 0x7ff) << 11)
#define     VDPU_REG_BD_REF_PIC_QUANT_3(x)		(((x) & 0x7ff) << 0)
#define VDPU_REG_BD_P_REF_PIC			0x0bc
#define     VDPU_REG_BD_P_REF_PIC_QUANT_DELTA_4(x)	(((x) & 0x1f) << 27)
#define     VDPU_REG_BD_P_REF_PIC_PINIT_RLIST_F3(x)	(((x) & 0x1f) << 25)
#define     VDPU_REG_BD_P_REF_PIC_PINIT_RLIST_F2(x)	(((x) & 0x1f) << 20)
#define     VDPU_REG_BD_P_REF_PIC_PINIT_RLIST_F1(x)	(((x) & 0x1f) << 15)
#define     VDPU_REG_BD_P_REF_PIC_PINIT_RLIST_F0(x)	(((x) & 0x1f) << 10)
#define     VDPU_REG_BD_P_REF_PIC_BINIT_RLIST_B15(x)	(((x) & 0x1f) << 5)
#define     VDPU_REG_BD_P_REF_PIC_BINIT_RLIST_F15(x)	(((x) & 0x1f) << 0)
#define VDPU_REG_ERR_CONC			0x0c0
#define     VDPU_REG_ERR_CONC_STARTMB_X(x)		(((x) & 0x1ff) << 23)
#define     VDPU_REG_ERR_CONC_STARTMB_Y(x)		(((x) & 0xff) << 15)
#define VDPU_REG_PRED_FLT			0x0c4
#define     VDPU_REG_PRED_FLT_PRED_BC_TAP_0_0(x)	(((x) & 0x3ff) << 22)
#define     VDPU_REG_PRED_FLT_PRED_BC_TAP_0_1(x)	(((x) & 0x3ff) << 12)
#define     VDPU_REG_PRED_FLT_PRED_BC_TAP_0_2(x)	(((x) & 0x3ff) << 2)
#define VDPU_REG_REF_BUF_CTRL			0x0cc
#define     VDPU_REG_REF_BUF_CTRL_REFBU_E		BIT(31)
#define     VDPU_REG_REF_BUF_CTRL_REFBU_THR(x)		(((x) & 0xfff) << 19)
#define     VDPU_REG_REF_BUF_CTRL_REFBU_PICID(x)	(((x) & 0x1f) << 14)
#define     VDPU_REG_REF_BUF_CTRL_REFBU_EVAL_E		BIT(13)
#define     VDPU_REG_REF_BUF_CTRL_REFBU_FPARMOD_E	BIT(12)
#define     VDPU_REG_REF_BUF_CTRL_REFBU_Y_OFFSET(x)	(((x) & 0x1ff) << 0)
#define VDPU_REG_REF_BUF_CTRL2			0x0dc
#define     VDPU_REG_REF_BUF_CTRL2_REFBU2_BUF_E		BIT(31)
#define     VDPU_REG_REF_BUF_CTRL2_REFBU2_THR(x)	(((x) & 0xfff) << 19)
#define     VDPU_REG_REF_BUF_CTRL2_REFBU2_PICID(x)	(((x) & 0x1f) << 14)
#define     VDPU_REG_REF_BUF_CTRL2_APF_THRESHOLD(x)	(((x) & 0x3fff) << 0)

#endif /* RK3288_VPU_REGS_H_ */
