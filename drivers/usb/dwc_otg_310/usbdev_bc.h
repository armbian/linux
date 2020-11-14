/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _USBDEV_BC_H
#define _USBDEV_BC_H

#include <linux/power/rk_usbbc.h>

enum {
	BC_BVALID = 0,
	BC_IDDIG,
};

enum {
	SYNOP_BC_BVALID = 0,
	SYNOP_BC_IDDIG,
	SYNOP_BC_DCDENB,
	SYNOP_BC_VDATSRCENB,
	SYNOP_BC_VDATDETENB,
	SYNOP_BC_CHRGSEL,
	SYNOP_BC_CHGDET,
	SYNOP_BC_FSVPLUS,
	SYNOP_BC_FSVMINUS,
	SYNOP_BC_MAX,
};

enum {
	INNO_BC_BVALID = 0,
	INNO_BC_IDDIG,
	INNO_BC_VDMSRCEN,
	INNO_BC_VDPSRCEN,
	INNO_BC_RDMPDEN,
	INNO_BC_IDPSRCEN,
	INNO_BC_IDMSINKEN,
	INNO_BC_IDPSINKEN,
	INNO_BC_DPATTACH,
	INNO_BC_CPDET,
	INNO_BC_DCPATTACH,
	INNO_BC_MAX,
};

enum {
	RK_BC_BVALID = 0,
	RK_BC_IDDIG,
	RK_BC_LINESTATE,
	RK_BC_SOFTCTRL,
	RK_BC_OPMODE,
	RK_BC_XCVRSELECT,
	RK_BC_TERMSELECT,
	RK_BC_MAX,
};

#define T_DCD_TIMEOUT	 (400)
#define T_BC_WAIT_CHGDET (40)
#define T_BC_CHGDET_VALID (200)

enum {
	DCD_POSITIVE = 0,
	DCD_PASSED,
	DCD_TIMEOUT,
};

/***********************************
USB Port Type
0 : Disconnect
1 : SDP - pc
2 : DCP - charger
3 : CDP - pc with big currect charge
***********************************/
extern enum bc_port_type usb_battery_charger_detect(bool wait);
extern void rk_battery_charger_detect_cb(int charger_type_new);

#endif
