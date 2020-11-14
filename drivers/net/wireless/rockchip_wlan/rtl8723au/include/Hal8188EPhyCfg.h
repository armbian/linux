/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __INC_HAL8188EPHYCFG_H__
#define __INC_HAL8188EPHYCFG_H__


/*--------------------------Define Parameters-------------------------------*/
#define LOOP_LIMIT				5
#define MAX_STALL_TIME			50		//us
#define AntennaDiversityValue	0x80	//(Adapter->bSoftwareAntennaDiversity ? 0x00:0x80)
#define MAX_TXPWR_IDX_NMODE_92S	63
#define Reset_Cnt_Limit			3

#define IQK_MAC_REG_NUM		4
#define IQK_ADDA_REG_NUM		16
#define IQK_BB_REG_NUM			9
#define HP_THERMAL_NUM		8

#ifdef CONFIG_PCI_HCI
#define MAX_AGGR_NUM	0x0B
#else
#define MAX_AGGR_NUM	0x07
#endif // CONFIG_PCI_HCI


/*--------------------------Define Parameters-------------------------------*/


/*------------------------------Define structure----------------------------*/ 
typedef enum _SwChnlCmdID{
	CmdID_End,
	CmdID_SetTxPowerLevel,
	CmdID_BBRegWrite10,
	CmdID_WritePortUlong,
	CmdID_WritePortUshort,
	CmdID_WritePortUchar,
	CmdID_RF_WriteReg,
}SwChnlCmdID;


/* 1. Switch channel related */
typedef struct _SwChnlCmd{
	SwChnlCmdID	CmdID;
	u32			Para1;
	u32			Para2;
	u32			msDelay;
}SwChnlCmd;

typedef enum _HW90_BLOCK{
	HW90_BLOCK_MAC = 0,
	HW90_BLOCK_PHY0 = 1,
	HW90_BLOCK_PHY1 = 2,
	HW90_BLOCK_RF = 3,
	HW90_BLOCK_MAXIMUM = 4, // Never use this
}HW90_BLOCK_E, *PHW90_BLOCK_E;

typedef enum _RF_RADIO_PATH{
	RF_PATH_A = 0,			//Radio Path A
	RF_PATH_B = 1,			//Radio Path B
	RF_PATH_C = 2,			//Radio Path C
	RF_PATH_D = 3,			//Radio Path D
	//RF_PATH_MAX				//Max RF number 90 support 
}RF_RADIO_PATH_E, *PRF_RADIO_PATH_E;

#define MAX_PG_GROUP 13

#define	RF_PATH_MAX		2
#define 	MAX_RF_PATH		RF_PATH_MAX
#define	MAX_TX_COUNT_88E			1
#define 	MAX_TX_COUNT				MAX_TX_COUNT_88E  // 4 //path numbers

#define CHANNEL_MAX_NUMBER		14	// 14 is the max channel number
#define MAX_CHNL_GROUP_24G		6	// ch1~2, ch3~5, ch6~8,ch9~11,ch12~13,CH 14 total six groups
#define CHANNEL_GROUP_MAX_88E    	6

typedef enum _WIRELESS_MODE {
	WIRELESS_MODE_UNKNOWN = 0x00,
	WIRELESS_MODE_A 		= BIT2,
	WIRELESS_MODE_B 		= BIT0,
	WIRELESS_MODE_G 		= BIT1,
	WIRELESS_MODE_AUTO 	= BIT5,
	WIRELESS_MODE_N_24G 	= BIT3,
	WIRELESS_MODE_N_5G 	= BIT4,
	WIRELESS_MODE_AC		= BIT6
} WIRELESS_MODE;


typedef enum _PHY_Rate_Tx_Power_Offset_Area{
	RA_OFFSET_LEGACY_OFDM1,
	RA_OFFSET_LEGACY_OFDM2,
	RA_OFFSET_HT_OFDM1,
	RA_OFFSET_HT_OFDM2,
	RA_OFFSET_HT_OFDM3,
	RA_OFFSET_HT_OFDM4,
	RA_OFFSET_HT_CCK,
}RA_OFFSET_AREA,*PRA_OFFSET_AREA;


/* BB/RF related */
typedef	enum _RF_TYPE_8190P{
	RF_TYPE_MIN,	// 0
	RF_8225=1,			// 1 11b/g RF for verification only
	RF_8256=2,			// 2 11b/g/n 
	RF_8258=3,			// 3 11a/b/g/n RF
	RF_6052=4,		// 4 11b/g/n RF
	//RF_6052=5,		// 4 11b/g/n RF
	// TODO: We sholud remove this psudo PHY RF after we get new RF.
	RF_PSEUDO_11N=5,	// 5, It is a temporality RF. 
}RF_TYPE_8190P_E,*PRF_TYPE_8190P_E;


typedef struct _BB_REGISTER_DEFINITION{
	u32 rfintfs;			// set software control: 
							//		0x870~0x877[8 bytes]
							
	u32 rfintfi;			// readback data: 
							//		0x8e0~0x8e7[8 bytes]
							
	u32 rfintfo; 		// output data: 
							//		0x860~0x86f [16 bytes]
							
	u32 rfintfe; 		// output enable: 
							//		0x860~0x86f [16 bytes]
							
	u32 rf3wireOffset;	// LSSI data:
							//		0x840~0x84f [16 bytes]
							
	u32 rfLSSI_Select;	// BB Band Select: 
							//		0x878~0x87f [8 bytes]
							
	u32 rfTxGainStage;	// Tx gain stage: 
							//		0x80c~0x80f [4 bytes]
							
	u32 rfHSSIPara1; 	// wire parameter control1 : 
							//		0x820~0x823,0x828~0x82b, 0x830~0x833, 0x838~0x83b [16 bytes]
							
	u32 rfHSSIPara2; 	// wire parameter control2 : 
							//		0x824~0x827,0x82c~0x82f, 0x834~0x837, 0x83c~0x83f [16 bytes]
								
	u32 rfSwitchControl; //Tx Rx antenna control : 
							//		0x858~0x85f [16 bytes]
								
	u32 rfAGCControl1; 	//AGC parameter control1 : 
							//		0xc50~0xc53,0xc58~0xc5b, 0xc60~0xc63, 0xc68~0xc6b [16 bytes] 
								
	u32 rfAGCControl2; 	//AGC parameter control2 : 
							//		0xc54~0xc57,0xc5c~0xc5f, 0xc64~0xc67, 0xc6c~0xc6f [16 bytes] 
							
	u32 rfRxIQImbalance; //OFDM Rx IQ imbalance matrix : 
							//		0xc14~0xc17,0xc1c~0xc1f, 0xc24~0xc27, 0xc2c~0xc2f [16 bytes]
							
	u32 rfRxAFE;  		//Rx IQ DC ofset and Rx digital filter, Rx DC notch filter : 
							//		0xc10~0xc13,0xc18~0xc1b, 0xc20~0xc23, 0xc28~0xc2b [16 bytes]
							
	u32 rfTxIQImbalance; //OFDM Tx IQ imbalance matrix
							//		0xc80~0xc83,0xc88~0xc8b, 0xc90~0xc93, 0xc98~0xc9b [16 bytes]
							
	u32 rfTxAFE; 		//Tx IQ DC Offset and Tx DFIR type
							//		0xc84~0xc87,0xc8c~0xc8f, 0xc94~0xc97, 0xc9c~0xc9f [16 bytes]
								
	u32 rfLSSIReadBack; 	//LSSI RF readback data SI mode
								//		0x8a0~0x8af [16 bytes]

	u32 rfLSSIReadBackPi; 	//LSSI RF readback data PI mode 0x8b8-8bc for Path A and B

}BB_REGISTER_DEFINITION_T, *PBB_REGISTER_DEFINITION_T;

typedef struct _R_ANTENNA_SELECT_OFDM{	
	u32			r_tx_antenna:4;	
	u32			r_ant_l:4;
	u32			r_ant_non_ht:4;	
	u32			r_ant_ht1:4;
	u32			r_ant_ht2:4;
	u32			r_ant_ht_s1:4;
	u32			r_ant_non_ht_s1:4;
	u32			OFDM_TXSC:2;
	u32			Reserved:2;
}R_ANTENNA_SELECT_OFDM;

typedef struct _R_ANTENNA_SELECT_CCK{
	u8			r_cckrx_enable_2:2;	
	u8			r_cckrx_enable:2;
	u8			r_ccktx_enable:4;
}R_ANTENNA_SELECT_CCK;

/*------------------------------Define structure----------------------------*/ 


/*------------------------Export global variable----------------------------*/
/*------------------------Export global variable----------------------------*/


/*------------------------Export Marco Definition---------------------------*/
/*------------------------Export Marco Definition---------------------------*/


/*--------------------------Exported Function prototype---------------------*/
//
// BB and RF register read/write
//
u32	rtl8188e_PHY_QueryBBReg(	IN	PADAPTER	Adapter,
								IN	u32		RegAddr,
								IN	u32		BitMask	);
void	rtl8188e_PHY_SetBBReg(	IN	PADAPTER	Adapter,
								IN	u32		RegAddr,
								IN	u32		BitMask,
								IN	u32		Data	);
u32	rtl8188e_PHY_QueryRFReg(	IN	PADAPTER			Adapter,
								IN	RF_RADIO_PATH_E	eRFPath,
								IN	u32				RegAddr,
								IN	u32				BitMask	);
void	rtl8188e_PHY_SetRFReg(	IN	PADAPTER			Adapter,
								IN	RF_RADIO_PATH_E	eRFPath,
								IN	u32				RegAddr,
								IN	u32				BitMask,
								IN	u32				Data	);

//
// Initialization related function
//
/* MAC/BB/RF HAL config */
int	PHY_MACConfig8188E(IN	PADAPTER	Adapter	);
int	PHY_BBConfig8188E(IN	PADAPTER	Adapter	);
int	PHY_RFConfig8188E(IN	PADAPTER	Adapter	);

/* RF config */
int	rtl8188e_PHY_ConfigRFWithParaFile(IN PADAPTER Adapter, IN u8 * pFileName, RF_RADIO_PATH_E eRFPath);
int	rtl8188e_PHY_ConfigRFWithHeaderFile(	IN	PADAPTER			Adapter,
													IN	RF_RADIO_PATH_E		eRFPath);

/* Read initi reg value for tx power setting. */
void	rtl8192c_PHY_GetHWRegOriginalValue(	IN	PADAPTER		Adapter	);

//
// RF Power setting
//
//extern	BOOLEAN	PHY_SetRFPowerState(IN	PADAPTER			Adapter, 
//									IN	RT_RF_POWER_STATE	eRFPowerState);

//
// BB TX Power R/W
//
void	PHY_GetTxPowerLevel8188E(	IN	PADAPTER		Adapter,
											OUT u32*    		powerlevel	);
void	PHY_SetTxPowerLevel8188E(	IN	PADAPTER		Adapter,
											IN	u8			channel	);
BOOLEAN	PHY_UpdateTxPowerDbm8188E(	IN	PADAPTER	Adapter,
											IN	int		powerInDbm	);

//
VOID 
PHY_ScanOperationBackup8188E(IN	PADAPTER	Adapter,
										IN	u8		Operation	);

//
// Switch bandwidth for 8192S
//
//extern	void	PHY_SetBWModeCallback8192C(	IN	PRT_TIMER		pTimer	);
void	PHY_SetBWMode8188E(	IN	PADAPTER			pAdapter,
									IN	HT_CHANNEL_WIDTH	ChnlWidth,
									IN	unsigned char	Offset	);

//
// Set FW CMD IO for 8192S.
//
//extern	BOOLEAN HalSetIO8192C(	IN	PADAPTER			Adapter,
//									IN	IO_TYPE				IOType);

//
// Set A2 entry to fw for 8192S
//
extern	void FillA2Entry8192C(		IN	PADAPTER			Adapter,
										IN	u8				index,
										IN	u8*				val);


//
// channel switch related funciton
//
//extern	void	PHY_SwChnlCallback8192C(	IN	PRT_TIMER		pTimer	);
void	PHY_SwChnl8188E(	IN	PADAPTER		pAdapter,
									IN	u8			channel	);
				// Call after initialization
void	PHY_SwChnlPhy8192C(	IN	PADAPTER		pAdapter,
									IN	u8			channel	);

void ChkFwCmdIoDone(	IN	PADAPTER	Adapter);

//
// BB/MAC/RF other monitor API
//
void	PHY_SetMonitorMode8192C(IN	PADAPTER	pAdapter,
										IN	BOOLEAN		bEnableMonitorMode	);

BOOLEAN	PHY_CheckIsLegalRfPath8192C(IN	PADAPTER	pAdapter,
											IN	u32		eRFPath	);

VOID PHY_SetRFPathSwitch_8188E(IN	PADAPTER	pAdapter, IN	BOOLEAN		bMain);

extern	VOID
PHY_SwitchEphyParameter(
	IN	PADAPTER			Adapter
	);

extern	VOID
PHY_EnableHostClkReq(
	IN	PADAPTER			Adapter
	);

BOOLEAN
SetAntennaConfig92C(
	IN	PADAPTER	Adapter,
	IN	u8		DefaultAnt	
	);

#ifdef CONFIG_PHY_SETTING_WITH_ODM
VOID
storePwrIndexDiffRateOffset(
	IN	PADAPTER	Adapter,
	IN	u32		RegAddr,
	IN	u32		BitMask,
	IN	u32		Data
	);
#endif //CONFIG_PHY_SETTING_WITH_ODM
/*--------------------------Exported Function prototype---------------------*/

#define PHY_QueryBBReg(Adapter, RegAddr, BitMask) rtl8188e_PHY_QueryBBReg((Adapter), (RegAddr), (BitMask))
#define PHY_SetBBReg(Adapter, RegAddr, BitMask, Data) rtl8188e_PHY_SetBBReg((Adapter), (RegAddr), (BitMask), (Data))
#define PHY_QueryRFReg(Adapter, eRFPath, RegAddr, BitMask) rtl8188e_PHY_QueryRFReg((Adapter), (eRFPath), (RegAddr), (BitMask))
#define PHY_SetRFReg(Adapter, eRFPath, RegAddr, BitMask, Data) rtl8188e_PHY_SetRFReg((Adapter), (eRFPath), (RegAddr), (BitMask), (Data))

#define PHY_SetMacReg	PHY_SetBBReg
#define PHY_QueryMacReg PHY_QueryBBReg


//
// Initialization related function
//
/* MAC/BB/RF HAL config */
//extern s32 PHY_MACConfig8723(PADAPTER padapter);
//s32 PHY_BBConfig8723(PADAPTER padapter);
//s32 PHY_RFConfig8723(PADAPTER padapter);



//==================================================================
// Note: If SIC_ENABLE under PCIE, because of the slow operation
//	you should 
//	2) "#define RTL8723_FPGA_VERIFICATION	1"				in Precomp.h.WlanE.Windows
//	3) "#define RTL8190_Download_Firmware_From_Header	0"	in Precomp.h.WlanE.Windows if needed.
//
#if (RTL8188E_SUPPORT == 1) && (RTL8188E_FPGA_TRUE_PHY_VERIFICATION == 1)
#define	SIC_ENABLE				1
#define	SIC_HW_SUPPORT		1
#else
#define	SIC_ENABLE				0
#define	SIC_HW_SUPPORT		0
#endif
//==================================================================


#define	SIC_MAX_POLL_CNT		5

#if(SIC_HW_SUPPORT == 1)
#define	SIC_CMD_READY			0
#define	SIC_CMD_PREWRITE		0x1
#if(RTL8188E_SUPPORT == 1)
#define	SIC_CMD_WRITE			0x40
#define	SIC_CMD_PREREAD		0x2
#define	SIC_CMD_READ			0x80
#define	SIC_CMD_INIT			0xf0
#define	SIC_INIT_VAL			0xff

#define	SIC_INIT_REG			0x1b7
#define	SIC_CMD_REG			0x1EB		// 1byte
#define	SIC_ADDR_REG			0x1E8		// 1b4~1b5, 2 bytes
#define	SIC_DATA_REG			0x1EC		// 1b0~1b3
#else
#define	SIC_CMD_WRITE			0x11
#define	SIC_CMD_PREREAD		0x2
#define	SIC_CMD_READ			0x12
#define	SIC_CMD_INIT			0x1f
#define	SIC_INIT_VAL			0xff

#define	SIC_INIT_REG			0x1b7
#define	SIC_CMD_REG			0x1b6		// 1byte
#define	SIC_ADDR_REG			0x1b4		// 1b4~1b5, 2 bytes
#define	SIC_DATA_REG			0x1b0		// 1b0~1b3
#endif
#else
#define	SIC_CMD_READY			0
#define	SIC_CMD_WRITE			1
#define	SIC_CMD_READ			2

#if(RTL8188E_SUPPORT == 1)
#define	SIC_CMD_REG			0x1EB		// 1byte
#define	SIC_ADDR_REG			0x1E8		// 1b9~1ba, 2 bytes
#define	SIC_DATA_REG			0x1EC		// 1bc~1bf
#else
#define	SIC_CMD_REG			0x1b8		// 1byte
#define	SIC_ADDR_REG			0x1b9		// 1b9~1ba, 2 bytes
#define	SIC_DATA_REG			0x1bc		// 1bc~1bf
#endif
#endif

#if(SIC_ENABLE == 1)
VOID SIC_Init(IN PADAPTER Adapter);
#endif


#endif	// __INC_HAL8192CPHYCFG_H

