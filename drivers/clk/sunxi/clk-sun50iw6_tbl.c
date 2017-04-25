#include "clk-sun50iw6.h"
/*
 * freq table from hardware, need follow rules
 * 1)   each table  named as
 *      factor_pll1_tbl
 *      factor_pll2_tbl
 *      ...
 * 2) for each table line
 *      a) follow the format PLLx(n, k, m, p, d1, d2, freq), and keep the
 *         factors order
 *      b) if any factor not used, skip it
 *      c) the factor is the value to write registers, not means factor + 1
 *
 *      example
 *      PLL1(9, 0, 0, 2, 60000000) means PLL1(n, k, m, p, freq)
 *      PLLVIDEO0(3, 0, 96000000) means PLLVIDEO0(n, m, freq)
 *
 */

/* PLLCPU(n, m, p, freq)	F_N8X8_M0X2_P16x2 */
struct sunxi_clk_factor_freq factor_pllcpu_tbl[] = {
PLLCPU(11  ,    0  ,    0  ,      288000000U),
};

/* PLLDDR0(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllddr0_tbl[] = {
PLLDDR0(11 ,    0  ,    0  ,    288000000U),
};

/* PLLPERIPH0(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllperiph0_tbl[] = {
PLLPERIPH0(11 ,    0  ,    0  ,    288000000U),
};

/* PLLPERIPH1(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllperiph1_tbl[] = {
PLLPERIPH1(11 ,    0  ,    0  ,    288000000U),
};

/* PLLGPU(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllgpu_tbl[] = {
PLLGPU(11 ,    2  ,    0  ,    288000000U),
};

/* PLLVIDEO0(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllvideo0_tbl[] = {
PLLVIDEO0(11 ,    0  ,    288000000U),
};

/* PLLVIDEO1(n, d1, freq)	F_N8X8_D1V1X1 */
struct sunxi_clk_factor_freq factor_pllvideo1_tbl[] = {
PLLVIDEO1(11 ,    0  ,    288000000U),
};

/* PLLVE(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllve_tbl[] = {
PLLVE(11 ,    0  ,    0  ,    288000000U),
};

/* PLLDE(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllde_tbl[] = {
PLLDE(11 ,    0  ,    0  ,    288000000U),
};

/* PLLHSIC(n, d1, d2, freq)	F_N8X8_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllhsic_tbl[] = {
PLLHSIC(11 ,    0  ,    0  ,    288000000U),
};

/* PLLAUDIO(n, p, d1, d2, freq)	F_N8X8_P16X6_D1V1X1_D2V0X1 */
struct sunxi_clk_factor_freq factor_pllaudio_tbl[] = {
PLLAUDIO(11 ,    0  ,    0  ,    0  ,    288000000U),
};

static unsigned int pllcpu_max, pllddr0_max, pllperiph0_max, pllperiph1_max,
		    pllgpu_max, pllvideo0_max, pllvideo1_max, pllve_max,
		    pllde_max, pllhsic_max, pllaudio_max;

#define PLL_MAX_ASSIGN(name) (pll##name##_max = \
	factor_pll##name##_tbl[ARRAY_SIZE(factor_pll##name##_tbl)-1].freq)

void sunxi_clk_factor_initlimits(void)
{
	PLL_MAX_ASSIGN(cpu);
	PLL_MAX_ASSIGN(ddr0);
	PLL_MAX_ASSIGN(periph0);
	PLL_MAX_ASSIGN(periph1);
	PLL_MAX_ASSIGN(gpu);
	PLL_MAX_ASSIGN(video0);
	PLL_MAX_ASSIGN(video1);
	PLL_MAX_ASSIGN(ve);
	PLL_MAX_ASSIGN(de);
	PLL_MAX_ASSIGN(hsic);
	PLL_MAX_ASSIGN(audio);
}
