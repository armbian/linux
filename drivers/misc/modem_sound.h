/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MODEM_SOUND_H__
#define __MODEM_SOUND_H__
#include <linux/ioctl.h>

#define MODEM_SOUND                   0x1B

#define IOCTL_MODEM_EAR_PHOEN      	        _IO(MODEM_SOUND, 0x01)
#define IOCTL_MODEM_SPK_PHONE      	        _IO(MODEM_SOUND, 0x02) 
#define IOCTL_MODEM_HP_WITHMIC_PHONE      	        _IO(MODEM_SOUND, 0x03)
#define IOCTL_MODEM_BT_PHONE      	        _IO(MODEM_SOUND, 0x04)
#define IOCTL_MODEM_STOP_PHONE      	    _IO(MODEM_SOUND, 0x05) 
#define IOCTL_MODEM_HP_NOMIC_PHONE      	_IO(MODEM_SOUND, 0x06) 

#define IOCTL_SET_EAR_VALUME      	    _IO(MODEM_SOUND, 0x11) 
#define IOCTL_SET_SPK_VALUME      	    _IO(MODEM_SOUND, 0x12) 
#define IOCTL_SET_HP_WITHMIC_VALUME      	    _IO(MODEM_SOUND, 0x13) 
#define IOCTL_SET_BT_VALUME      	    _IO(MODEM_SOUND, 0x14) 
#define IOCTL_SET_HP_NOMIC_PHONE            _IO(MODEM_SOUND, 0x15)

enum {
	OFF,
	RCV,
	SPK_PATH,
	HP_PATH,
	HP_NO_MIC,
	BT,
};

struct modem_sound_data {
	int spkctl_io;
	int spkctl_active;
	int codec_flag;
	struct semaphore power_sem;
	struct workqueue_struct *wq;
	struct work_struct work;
};

#endif
