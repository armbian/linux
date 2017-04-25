#ifndef  __S070WV20_MIPI_RGB_PANEL_H__
#define  __S070WV20_MIPI_RGB_PANEL_H__

#include "panels.h"

#define icn_en(val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define power_en(val)  sunxi_lcd_gpio_set_value(sel, 1, val)

extern __lcd_panel_t S070WV20_MIPI_RGB_panel;

#endif
