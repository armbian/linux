#include "S070WV20_MIPI_RGB.h"

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static u32 debug_mask = 0;

#define dprintk(fmt,arg...)    if(unlikely(debug_mask )) \
        printk("[***DISP_DBG***]:"fmt, ## arg)
module_param_named(debug_mask,debug_mask,int,S_IRUGO | S_IWUSR | S_IWGRP);

#define Bist_mode	(0)

static void LCD_cfg_panel_info(panel_extend_para * info)
{
	u32 i = 0, j=0;
	u32 items;
	u8 lcd_gamma_tbl[][2] =
	{
		//{input value, corrected value}
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

//	u8 lcd_bright_curve_tbl[][2] =
//	{
//		//{input value, corrected value}
//		{0    ,0  },//0
//		{15   ,3  },//0
//		{30   ,6  },//0
//		{45   ,9  },// 1
//		{60   ,12  },// 2
//		{75   ,16  },// 5
//		{90   ,22  },//9
//		{105   ,28 }, //15
//		{120  ,36 },//23
//		{135  ,44 },//33
//		{150  ,54 },
//		{165  ,67 },
//		{180  ,84 },
//		{195  ,108},
//		{210  ,137},
//		{225 ,171},
//		{240 ,210},
//		{255 ,255},
//	};

	u32 lcd_cmap_tbl[2][3][4] = {
	{
		{LCD_CMAP_G0,LCD_CMAP_B1,LCD_CMAP_G2,LCD_CMAP_B3},
		{LCD_CMAP_B0,LCD_CMAP_R1,LCD_CMAP_B2,LCD_CMAP_R3},
		{LCD_CMAP_R0,LCD_CMAP_G1,LCD_CMAP_R2,LCD_CMAP_G3},
		},
		{
		{LCD_CMAP_B3,LCD_CMAP_G2,LCD_CMAP_B1,LCD_CMAP_G0},
		{LCD_CMAP_R3,LCD_CMAP_B2,LCD_CMAP_R1,LCD_CMAP_B0},
		{LCD_CMAP_G3,LCD_CMAP_R2,LCD_CMAP_G1,LCD_CMAP_R0},
		},
	};

	//memset(info,0,sizeof(panel_extend_para));

	items = sizeof(lcd_gamma_tbl)/2;
	for(i=0; i<items-1; i++) {
		u32 num = lcd_gamma_tbl[i+1][0] - lcd_gamma_tbl[i][0];

		for(j=0; j<num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] + ((lcd_gamma_tbl[i+1][1] - lcd_gamma_tbl[i][1]) * j)/num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] = (value<<16) + (value<<8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items-1][1]<<16) + (lcd_gamma_tbl[items-1][1]<<8) + lcd_gamma_tbl[items-1][1];

//	items = sizeof(lcd_bright_curve_tbl)/2;
//	for(i=0; i<items-1; i++) {
//		u32 num = lcd_bright_curve_tbl[i+1][0] - lcd_bright_curve_tbl[i][0];
//
//		for(j=0; j<num; j++) {
//			u32 value = 0;
//
//			value = lcd_bright_curve_tbl[i][1] + ((lcd_bright_curve_tbl[i+1][1] - lcd_bright_curve_tbl[i][1]) * j)/num;
//			info->lcd_bright_curve_tbl[lcd_bright_curve_tbl[i][0] + j] = value;
//		}
//	}
//	info->lcd_bright_curve_tbl[255] = lcd_bright_curve_tbl[items-1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 LCD_open_flow(u32 sel)
{        
	LCD_OPEN_FUNC(sel, LCD_power_on, 200);   //open lcd power, and delay 50ms
	LCD_OPEN_FUNC(sel, LCD_panel_init, 150);   //open lcd power, than delay 200ms
	 LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable,300);  //open lcd controller, and delay 100ms
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);     //open lcd backlight, and delay 0ms
	dprintk("***LCD_open_flow_kernel\n");
	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);       //close lcd backlight, and delay 0ms
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);         //close lcd controller, and delay 0ms
	LCD_CLOSE_FUNC(sel, LCD_panel_exit,	20);   //open lcd power, than delay 200ms
	LCD_CLOSE_FUNC(sel, LCD_power_off, 500);   //close lcd power, and delay 500ms
	dprintk("***LCD_close_flow_kernel\n");
	return 0;
}

static void LCD_power_on(u32 sel)
{
	sunxi_lcd_power_enable(sel, 0);//config lcd_power pin to open lcd power0
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_power_enable(sel, 1);//config lcd_power pin to open lcd power1
	sunxi_lcd_delay_ms(10);
	power_en(1);
	sunxi_lcd_delay_ms(50);
	icn_en(1);
	//sunxi_lcd_delay_ms(10);
	//icn_en(0);
	//sunxi_lcd_delay_ms(30);
	//icn_en(1);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_pin_cfg(sel, 1);
	dprintk("***LCD_power_on_kernel\n");
	
}

static void LCD_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(10);
	icn_en(0);
	sunxi_lcd_delay_ms(10);
	power_en(0);
	sunxi_lcd_delay_ms(50);
	sunxi_lcd_power_disable(sel, 1);//config lcd_power pin to close lcd power0
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_power_disable(sel, 0);//config lcd_power pin to close lcd power1
	dprintk("***LCD_power_off_kernel\n");

}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);//open pwm module
	sunxi_lcd_backlight_enable(sel);//config lcd_bl_en pin to open lcd backlight
	dprintk("***LCD_bl_open_kernel\n");
}

static void LCD_bl_close(u32 sel)
{
	sunxi_lcd_backlight_disable(sel);//config lcd_bl_en pin to close lcd backlight
	sunxi_lcd_pwm_disable(sel);//close pwm module
	dprintk("***LCD_bl_close_kernel\n");
}

static void LCD_panel_init(u32 sel)
{
	dprintk("***LCD_panel_init_kernel:Begin!!!\n");
	sunxi_lcd_dsi_gen_write_1para(sel,0x7A,0xC1);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x20,0x20);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x21,0xE0);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x22,0x13);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x23,0x28);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x24,0x30);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x25,0x28);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x26,0x00);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x27,0x0D);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x28,0x03);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x29,0x1D);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x34,0x80);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x36,0x28);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0xB5,0xA0);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x5C,0xFF);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x2A,0x01);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x56,0x92);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x6B,0x71);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x69,0x2B);//2B
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x10,0x40);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x11,0x98);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0xB6,0x20);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_gen_write_1para(sel,0x51,0x20);
	sunxi_lcd_delay_ms(10);
#if Bist_mode
	sunxi_lcd_dsi_gen_write_1para(sel,0x14,0x43);
	sunxi_lcd_dsi_gen_write_1para(sel,0x2A,0x49);
#endif
	sunxi_lcd_dsi_gen_write_1para(sel,0x09,0x10);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_clk_enable(sel);

	dprintk("***LCD_panel_init_kernel:End!!!\n");
	return;
}

static void LCD_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_clk_disable(sel);
	dprintk("***LCD_panel_exit_kernel\n");
	return ;
}

//sel: 0:lcd0; 1:lcd1
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t S070WV20_MIPI_RGB_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
	.name = "S070WV20_MIPI_RGB",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
	},
};
