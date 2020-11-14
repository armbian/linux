/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __CHARGER_BQ25700_H_
#define __CHARGER_BQ25700_H_

#define CHARGER_CURRENT_EVENT	0x01
#define INPUT_CURRENT_EVENT	0x02

void bq25700_charger_set_current(unsigned long event, int current_value);

#endif /* __CHARGER_BQ25700_H_ */
