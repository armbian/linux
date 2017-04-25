#ifndef _POWER_H
#define _POWER_H

/*
 * Copyright (c) 2011-2015 yanggq.young@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
extern s32 axp19x_set_volt(u32 id, u32 voltage);
extern s32 axp19x_get_volt(u32 id);
extern s32 axp19x_set_state(u32 id, u32 state);
extern s32 axp19x_get_state(u32 id);
extern s32 axp19x_suspend(u32 id);
extern s32 axp152_set_volt(u32 id, u32 voltage);
extern s32 axp152_get_volt(u32 id);
extern s32 axp152_set_state(u32 id, u32 state);
extern s32 axp152_get_state(u32 id);
extern s32 axp152_suspend(u32 id);

extern s32	axp15_get_state(u32	id);
extern s32	axp20_get_state(u32	id);
extern s32	axp22_get_state(u32	id);
extern s32	axp15_set_state(u32	id,	u32	state);
extern s32	axp20_set_state(u32	id,	u32	state);
extern s32	axp22_set_state(u32	id,	u32	state);
extern s32	axp15_get_volt(u32 id);
extern s32	axp20_get_volt(u32 id);
extern s32	axp22_get_volt(u32 id);
extern s32	axp15_set_volt(u32 id, u32 voltage);
extern s32	axp20_set_volt(u32 id, u32 voltage);
extern s32	axp22_set_volt(u32 id, u32 voltage);
extern s32 axp15_suspend_calc(u32 pmu_cnt, u32 id, losc_enter_ss_func *func);
extern s32 axp20_suspend_calc(u32 id, losc_enter_ss_func *func);
extern s32 axp22_suspend_calc(u32 id, losc_enter_ss_func *func);
#endif /*_PM_H*/
