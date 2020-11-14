/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
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
//============================================================
// Description:
//
// This file is for 92CE/92CU dynamic mechanism only
//
//
//============================================================
#define _RTL8723A_DM_C_

//============================================================
// include files
//============================================================
#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <rtw_byteorder.h>

#include <rtl8723a_hal.h>

//============================================================
// Global var
//============================================================


static VOID
dm_CheckProtection(
	IN	PADAPTER	Adapter
	)
{
#if 0
	PMGNT_INFO		pMgntInfo = &(Adapter->MgntInfo);
	u1Byte			CurRate, RateThreshold;

	if(pMgntInfo->pHTInfo->bCurBW40MHz)
		RateThreshold = MGN_MCS1;
	else
		RateThreshold = MGN_MCS3;

	if(Adapter->TxStats.CurrentInitTxRate <= RateThreshold)
	{
		pMgntInfo->bDmDisableProtect = TRUE;
		DbgPrint("Forced disable protect: %x\n", Adapter->TxStats.CurrentInitTxRate);
	}
	else
	{
		pMgntInfo->bDmDisableProtect = FALSE;
		DbgPrint("Enable protect: %x\n", Adapter->TxStats.CurrentInitTxRate);
	}
#endif
}

static VOID
dm_CheckStatistics(
	IN	PADAPTER	Adapter
	)
{
#if 0
	if(!Adapter->MgntInfo.bMediaConnect)
		return;

	//2008.12.10 tynli Add for getting Current_Tx_Rate_Reg flexibly.
	rtw_hal_get_hwreg( Adapter, HW_VAR_INIT_TX_RATE, (pu1Byte)(&Adapter->TxStats.CurrentInitTxRate) );

	// Calculate current Tx Rate(Successful transmited!!)

	// Calculate current Rx Rate(Successful received!!)

	//for tx tx retry count
	rtw_hal_get_hwreg( Adapter, HW_VAR_RETRY_COUNT, (pu1Byte)(&Adapter->TxStats.NumTxRetryCount) );
#endif
}

#ifdef CONFIG_SUPPORT_HW_WPS_PBC
static void dm_CheckPbcGPIO(_adapter *padapter)
{
	u8	tmp1byte;
	u8	bPbcPressed = _FALSE;

	if(!padapter->registrypriv.hw_wps_pbc)
		return;

#ifdef CONFIG_USB_HCI
	tmp1byte = rtw_read8(padapter, GPIO_IO_SEL);
	tmp1byte |= (HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(padapter, GPIO_IO_SEL, tmp1byte);	//enable GPIO[2] as output mode

	tmp1byte &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(padapter,  GPIO_IN, tmp1byte);		//reset the floating voltage level

	tmp1byte = rtw_read8(padapter, GPIO_IO_SEL);
	tmp1byte &= ~(HAL_8192C_HW_GPIO_WPS_BIT);
	rtw_write8(padapter, GPIO_IO_SEL, tmp1byte);	//enable GPIO[2] as input mode

	tmp1byte =rtw_read8(padapter, GPIO_IN);

	if (tmp1byte == 0xff)
		return ;

	if (tmp1byte&HAL_8192C_HW_GPIO_WPS_BIT)
	{
		bPbcPressed = _TRUE;
	}
#else
	tmp1byte = rtw_read8(padapter, GPIO_IN);
	//RT_TRACE(COMP_IO, DBG_TRACE, ("dm_CheckPbcGPIO - %x\n", tmp1byte));

	if (tmp1byte == 0xff || padapter->init_adpt_in_progress)
		return ;

	if((tmp1byte&HAL_8192C_HW_GPIO_WPS_BIT)==0)
	{
		bPbcPressed = _TRUE;
	}
#endif

	if( _TRUE == bPbcPressed)
	{
		// Here we only set bPbcPressed to true
		// After trigger PBC, the variable will be set to false
		DBG_8192C("CheckPbcGPIO - PBC is pressed\n");

#ifdef RTK_DMP_PLATFORM
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12))
		kobject_uevent(&padapter->pnetdev->dev.kobj, KOBJ_NET_PBC);
#else
		kobject_hotplug(&padapter->pnetdev->class_dev.kobj, KOBJ_NET_PBC);
#endif
#else

		if ( padapter->pid[0] == 0 )
		{	//	0 is the default value and it means the application monitors the HW PBC doesn't privde its pid to driver.
			return;
		}

#ifdef PLATFORM_LINUX
		rtw_signal_process(padapter->pid[0], SIGUSR1);
#endif
#endif
	}
}
#endif //#ifdef CONFIG_SUPPORT_HW_WPS_PBC

#ifdef CONFIG_PCI_HCI
//
//	Description:
//		Perform interrupt migration dynamically to reduce CPU utilization.
//
//	Assumption:
//		1. Do not enable migration under WIFI test.
//
//	Created by Roger, 2010.03.05.
//
VOID
dm_InterruptMigration(
	IN	PADAPTER	Adapter
	)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	struct mlme_priv	*pmlmepriv = &(Adapter->mlmepriv);
	BOOLEAN			bCurrentIntMt, bCurrentACIntDisable;
	BOOLEAN			IntMtToSet = _FALSE;
	BOOLEAN			ACIntToSet = _FALSE;


	// Retrieve current interrupt migration and Tx four ACs IMR settings first.
	bCurrentIntMt = pHalData->bInterruptMigration;
	bCurrentACIntDisable = pHalData->bDisableTxInt;

	//
	// <Roger_Notes> Currently we use busy traffic for reference instead of RxIntOK counts to prevent non-linear Rx statistics
	// when interrupt migration is set before. 2010.03.05.
	//
	if(!Adapter->registrypriv.wifi_spec &&
		(check_fwstate(pmlmepriv, _FW_LINKED)== _TRUE) &&
		pmlmepriv->LinkDetectInfo.bHigherBusyTraffic)
	{
		IntMtToSet = _TRUE;

		// To check whether we should disable Tx interrupt or not.
		if(pmlmepriv->LinkDetectInfo.bHigherBusyRxTraffic )
			ACIntToSet = _TRUE;
	}

	//Update current settings.
	if( bCurrentIntMt != IntMtToSet ){
		DBG_8192C("%s(): Update interrrupt migration(%d)\n",__FUNCTION__,IntMtToSet);
		if(IntMtToSet)
		{
			//
			// <Roger_Notes> Set interrrupt migration timer and corresponging Tx/Rx counter.
			// timer 25ns*0xfa0=100us for 0xf packets.
			// 2010.03.05.
			//
			rtw_write32(Adapter, REG_INT_MIG, 0xff000fa0);// 0x306:Rx, 0x307:Tx
			pHalData->bInterruptMigration = IntMtToSet;
		}
		else
		{
			// Reset all interrupt migration settings.
			rtw_write32(Adapter, REG_INT_MIG, 0);
			pHalData->bInterruptMigration = IntMtToSet;
		}
	}

	/*if( bCurrentACIntDisable != ACIntToSet ){
		DBG_8192C("%s(): Update AC interrrupt(%d)\n",__FUNCTION__,ACIntToSet);
		if(ACIntToSet) // Disable four ACs interrupts.
		{
			//
			// <Roger_Notes> Disable VO, VI, BE and BK four AC interrupts to gain more efficient CPU utilization.
			// When extremely highly Rx OK occurs, we will disable Tx interrupts.
			// 2010.03.05.
			//
			UpdateInterruptMask8192CE( Adapter, 0, RT_AC_INT_MASKS );
			pHalData->bDisableTxInt = ACIntToSet;
		}
		else// Enable four ACs interrupts.
		{
			UpdateInterruptMask8192CE( Adapter, RT_AC_INT_MASKS, 0 );
			pHalData->bDisableTxInt = ACIntToSet;
		}
	}*/

}

#endif

//
// Initialize GPIO setting registers
//
static void
dm_InitGPIOSetting(
	IN	PADAPTER	Adapter
	)
{
	PHAL_DATA_TYPE		pHalData = GET_HAL_DATA(Adapter);

	u8	tmp1byte;

	tmp1byte = rtw_read8(Adapter, REG_GPIO_MUXCFG);
	tmp1byte &= (GPIOSEL_GPIO | ~GPIOSEL_ENBT);

	rtw_write8(Adapter, REG_GPIO_MUXCFG, tmp1byte);
}
//============================================================
// functions
//============================================================
static void Init_ODM_ComInfo_8723a(PADAPTER	Adapter)
{

	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	PDM_ODM_T		pDM_Odm = &(pHalData->odmpriv);
	u8	cut_ver,fab_ver;
	
	//
	// Init Value
	//
	_rtw_memset(pDM_Odm,0,sizeof(pDM_Odm));
	
	pDM_Odm->Adapter = Adapter;
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_PLATFORM,ODM_CE);
	if(Adapter->interface_type == RTW_GSPI )
		ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_INTERFACE,ODM_ITRF_SDIO);
	else
		ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_INTERFACE,Adapter->interface_type);//RTL871X_HCI_TYPE
		
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_IC_TYPE,ODM_RTL8723A);
	

	if(IS_8723A_A_CUT(pHalData->VersionID))
	{
		fab_ver = ODM_UMC;
		cut_ver = ODM_CUT_A;		
	}
	else if(IS_8723A_B_CUT(pHalData->VersionID))
	{
		fab_ver = ODM_UMC;
		cut_ver = ODM_CUT_B;		
	}
	else
	{
		fab_ver = ODM_TSMC;
		cut_ver = ODM_CUT_A;	
	}
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_FAB_VER,fab_ver);		
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_CUT_VER,cut_ver);
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_MP_TEST_CHIP,IS_NORMAL_CHIP(pHalData->VersionID));
	
#ifdef CONFIG_USB_HCI	
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_BOARD_TYPE,pHalData->BoardType);

	if(pHalData->BoardType == BOARD_USB_High_PA){
		ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_EXT_LNA,_TRUE);
		ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_EXT_PA,_TRUE);
	}
#endif	
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_PATCH_ID,pHalData->CustomerID);
	//	ODM_CMNINFO_BINHCT_TEST only for MP Team
	ODM_CmnInfoInit(pDM_Odm,ODM_CMNINFO_BWIFI_TEST,Adapter->registrypriv.wifi_spec);
		
	
	if(pHalData->rf_type == RF_1T1R){
		ODM_CmnInfoUpdate(pDM_Odm,ODM_CMNINFO_RF_TYPE,ODM_1T1R);		
	}
	else if(pHalData->rf_type == RF_2T2R){
		ODM_CmnInfoUpdate(pDM_Odm,ODM_CMNINFO_RF_TYPE,ODM_2T2R);		
	}
	else if(pHalData->rf_type == RF_1T2R){		
		ODM_CmnInfoUpdate(pDM_Odm,ODM_CMNINFO_RF_TYPE,ODM_1T2R);		
	}	
}
static void Update_ODM_ComInfo_8723a(PADAPTER	Adapter)
{
	struct mlme_ext_priv	*pmlmeext = &Adapter->mlmeextpriv;
	struct mlme_priv		*pmlmepriv = &Adapter->mlmepriv;
	struct pwrctrl_priv *pwrctrlpriv = adapter_to_pwrctl(Adapter);
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	PDM_ODM_T		pDM_Odm = &(pHalData->odmpriv);
	struct dm_priv	*pdmpriv = &pHalData->dmpriv;	
	int i;

	pdmpriv->InitODMFlag = 0
		| ODM_BB_DIG
#ifdef CONFIG_ODM_REFRESH_RAMASK
		| ODM_BB_RA_MASK
#endif
		| ODM_BB_DYNAMIC_TXPWR
		| ODM_BB_FA_CNT
		| ODM_BB_RSSI_MONITOR
		| ODM_BB_CCK_PD
		| ODM_BB_PWR_SAVE
		| ODM_MAC_EDCA_TURBO
		| ODM_RF_TX_PWR_TRACK
		| ODM_RF_CALIBRATION
#ifdef CONFIG_ODM_ADAPTIVITY
		| ODM_BB_ADAPTIVITY
#endif
		;

#ifdef CONFIG_ANTENNA_DIVERSITY
	if(pHalData->AntDivCfg)
		pdmpriv->InitODMFlag |= ODM_BB_ANT_DIV;
#endif

#if (MP_DRIVER==1)
	if (Adapter->registrypriv.mp_mode == 1) {
		pdmpriv->InitODMFlag = 0
			| ODM_RF_CALIBRATION
			| ODM_RF_TX_PWR_TRACK
			;
	}
#endif//(MP_DRIVER==1)

#ifdef CONFIG_DISABLE_ODM
	pdmpriv->InitODMFlag = 0;
#endif//CONFIG_DISABLE_ODM

	//
	// Pointer reference
	//
	//ODM_CMNINFO_MAC_PHY_MODE pHalData->MacPhyMode92D
	//	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_MAC_PHY_MODE,&(pDM_Odm->u1Byte_temp));

	ODM_CmnInfoUpdate(pDM_Odm,ODM_CMNINFO_ABILITY,pdmpriv->InitODMFlag);

	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_TX_UNI,&(Adapter->xmitpriv.tx_bytes));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_RX_UNI,&(Adapter->recvpriv.rx_bytes));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_WM_MODE,&(pmlmeext->cur_wireless_mode));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_SEC_CHNL_OFFSET,&(pHalData->nCur40MhzPrimeSC));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_SEC_MODE,&(Adapter->securitypriv.dot11PrivacyAlgrthm));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_BW,&(pHalData->CurrentChannelBW ));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_CHNL,&( pHalData->CurrentChannel));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_NET_CLOSED,&( Adapter->net_closed));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_MP_MODE,&(Adapter->registrypriv.mp_mode));

	//================= only for 8192D   =================
	/*
	//pHalData->CurrentBandType92D
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_BAND,&(pDM_Odm->u1Byte_temp));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_DMSP_GET_VALUE,&(pDM_Odm->u1Byte_temp));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_BUDDY_ADAPTOR,&(pDM_Odm->PADAPTER_temp));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_DMSP_IS_MASTER,&(pDM_Odm->u1Byte_temp));
	//================= only for 8192D   =================
	// driver havn't those variable now
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_BT_OPERATION,&(pDM_Odm->u1Byte_temp));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_BT_DISABLE_EDCA,&(pDM_Odm->u1Byte_temp));
	*/
	
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_SCAN,&(pmlmepriv->bScanInProcess));
	ODM_CmnInfoHook(pDM_Odm,ODM_CMNINFO_POWER_SAVING,&(pwrctrlpriv->bpower_saving));


	for(i=0; i< NUM_STA; i++)
	{
		//pDM_Odm->pODM_StaInfo[i] = NULL;
		ODM_CmnInfoPtrArrayHook(pDM_Odm, ODM_CMNINFO_STA_STATUS,i,NULL);
	}	
}

void
rtl8723a_InitHalDm(
	IN	PADAPTER	Adapter
	)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_priv	*pdmpriv = &pHalData->dmpriv;
	PDM_ODM_T		pDM_Odm = &(pHalData->odmpriv);
	
	u8	i;

	pdmpriv->DM_Type = DM_Type_ByDriver;
	pdmpriv->DMFlag = DYNAMIC_FUNC_DISABLE;

#ifdef CONFIG_BT_COEXIST
	pdmpriv->DMFlag |= DYNAMIC_FUNC_BT;
//	btdm_InitBtCoexistDM(Adapter); // Move to BT_CoexistMechanism()
#endif
	pdmpriv->InitDMFlag = pdmpriv->DMFlag;
	
	Update_ODM_ComInfo_8723a(Adapter);
	ODM_DMInit(pDM_Odm);
	// Save REG_INIDATA_RATE_SEL value for TXDESC.
	for(i = 0 ; i<32 ; i++)
	{
		pdmpriv->INIDATA_RATE[i] = rtw_read8(Adapter, REG_INIDATA_RATE_SEL+i) & 0x3f;
	}

}

VOID
rtl8723a_HalDmWatchDog(
	IN	PADAPTER	Adapter
	)
{
	BOOLEAN		bFwCurrentInPSMode = _FALSE;
	BOOLEAN		bFwPSAwake = _TRUE;
	u8 hw_init_completed = _FALSE;
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_priv	*pdmpriv = &pHalData->dmpriv;
#ifdef CONFIG_CONCURRENT_MODE
	PADAPTER pbuddy_adapter = Adapter->pbuddy_adapter;
#endif //CONFIG_CONCURRENT_MODE

	hw_init_completed = Adapter->hw_init_completed;

#if (MP_DRIVER == 1)	
if (Adapter->registrypriv.mp_mode == 1 && Adapter->mppriv.mp_dm ==0)
		return;
#endif

	if (hw_init_completed == _FALSE)
		goto skip_dm;

#ifdef CONFIG_LPS
	bFwCurrentInPSMode = adapter_to_pwrctl(Adapter)->bFwCurrentInPSMode;
	rtw_hal_get_hwreg(Adapter, HW_VAR_FWLPS_RF_ON, (u8 *)(&bFwPSAwake));
#endif

#ifdef CONFIG_P2P_PS
	// Fw is under p2p powersaving mode, driver should stop dynamic mechanism.
	// modifed by thomas. 2011.06.11.
	if(Adapter->wdinfo.p2p_ps_mode)
		bFwPSAwake = _FALSE;
#endif //CONFIG_P2P_PS

	if( (hw_init_completed == _TRUE)
		&& ((!bFwCurrentInPSMode) && bFwPSAwake))
	{
		//
		// Calculate Tx/Rx statistics.
		//
		dm_CheckStatistics(Adapter);
		

#ifdef CONFIG_CONCURRENT_MODE
		if(Adapter->adapter_type > PRIMARY_ADAPTER)
			goto _record_initrate;
#endif

		//
		// Dynamically switch RTS/CTS protection.
		//
		//dm_CheckProtection(Adapter);

#ifdef CONFIG_PCI_HCI
		// 20100630 Joseph: Disable Interrupt Migration mechanism temporarily because it degrades Rx throughput.
		// Tx Migration settings.
		//dm_InterruptMigration(Adapter);

		//if(Adapter->HalFunc.TxCheckStuckHandler(Adapter))
		//	PlatformScheduleWorkItem(&(GET_HAL_DATA(Adapter)->HalResetWorkItem));
#endif
_record_initrate:

		// Read REG_INIDATA_RATE_SEL value for TXDESC.
		if(check_fwstate(&Adapter->mlmepriv, WIFI_STATION_STATE) == _TRUE)
		{
			pdmpriv->INIDATA_RATE[0] = rtw_read8(Adapter, REG_INIDATA_RATE_SEL) & 0x3f;
		}
		else
		{
			u8	i;
			for(i=1 ; i < (Adapter->stapriv.asoc_sta_count + 1); i++)
			{
				pdmpriv->INIDATA_RATE[i] = rtw_read8(Adapter, (REG_INIDATA_RATE_SEL+i)) & 0x3f;
			}
		}
	}
	

	//ODM
	if (hw_init_completed == _TRUE)
	{
		u8	bLinked=_FALSE;
		u8	bsta_state=_FALSE;

		#ifdef CONFIG_DISABLE_ODM
		pHalData->odmpriv.SupportAbility = 0;
		#endif

		if(rtw_linked_check(Adapter))
			bLinked = _TRUE;
		
#ifdef CONFIG_CONCURRENT_MODE
		if(pbuddy_adapter && rtw_linked_check(pbuddy_adapter))
			bLinked = _TRUE;
#endif //CONFIG_CONCURRENT_MODE

		ODM_CmnInfoUpdate(&pHalData->odmpriv ,ODM_CMNINFO_LINK, bLinked);

		if (check_fwstate(&Adapter->mlmepriv, WIFI_STATION_STATE))
			bsta_state = _TRUE;
#ifdef CONFIG_CONCURRENT_MODE
		if(pbuddy_adapter && check_fwstate(&pbuddy_adapter->mlmepriv, WIFI_STATION_STATE))
			bsta_state = _TRUE;
#endif //CONFIG_CONCURRENT_MODE	
		ODM_CmnInfoUpdate(&pHalData->odmpriv ,ODM_CMNINFO_STATION_STATE, bsta_state);


		ODM_DMWatchdog(&pHalData->odmpriv);

	}

skip_dm:
#ifdef CONFIG_SUPPORT_HW_WPS_PBC
	// Check GPIO to determine current Pbc status.
	dm_CheckPbcGPIO(Adapter);
#endif	
	return;
}

void rtl8723a_init_dm_priv(IN PADAPTER Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_priv	*pdmpriv = &pHalData->dmpriv;
	PDM_ODM_T 		podmpriv = &pHalData->odmpriv;
	_rtw_memset(pdmpriv, 0, sizeof(struct dm_priv));
	Init_ODM_ComInfo_8723a(Adapter);
#ifdef CONFIG_SW_ANTENNA_DIVERSITY	
	//_init_timer(&(pdmpriv->SwAntennaSwitchTimer),  Adapter->pnetdev , odm_SW_AntennaSwitchCallback, Adapter);	
	ODM_InitAllTimers(podmpriv );	
#endif
}

void rtl8723a_deinit_dm_priv(IN PADAPTER Adapter)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(Adapter);
	struct dm_priv	*pdmpriv = &pHalData->dmpriv;
	PDM_ODM_T 		podmpriv = &pHalData->odmpriv;
#ifdef CONFIG_SW_ANTENNA_DIVERSITY	
	//_cancel_timer_ex(&pdmpriv->SwAntennaSwitchTimer);	
	ODM_CancelAllTimers(podmpriv);	
#endif
}

