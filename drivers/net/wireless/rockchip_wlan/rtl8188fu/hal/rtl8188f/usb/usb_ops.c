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
  *******************************************************************************/
#define _USB_OPS_C_

#include <rtl8188f_hal.h>

#ifdef CONFIG_SUPPORT_USB_INT
void interrupt_handler_8188fu(_adapter *padapter, u16 pkt_len, u8 *pbuf)
{
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
	struct reportpwrstate_parm pwr_rpt;

	if (pkt_len != INTERRUPT_MSG_FORMAT_LEN) {
		DBG_8192C("%s Invalid interrupt content length (%d)!\n", __func__, pkt_len);
		return;
	}

	/* HISR */
	_rtw_memcpy(&(pHalData->IntArray[0]), &(pbuf[USB_INTR_CONTENT_HISR_OFFSET]), 4);
	_rtw_memcpy(&(pHalData->IntArray[1]), &(pbuf[USB_INTR_CONTENT_HISRE_OFFSET]), 4);

#if 0 /*DBG */
	{
		u32 hisr = 0 , hisr_ex = 0;

		_rtw_memcpy(&hisr, &(pHalData->IntArray[0]), 4);
		hisr = le32_to_cpu(hisr);

		_rtw_memcpy(&hisr_ex, &(pHalData->IntArray[1]), 4);
		hisr_ex = le32_to_cpu(hisr_ex);

		if ((hisr != 0) || (hisr_ex != 0))
			DBG_871X("===> %s hisr:0x%08x ,hisr_ex:0x%08x\n", __func__, hisr, hisr_ex);
	}
#endif


#ifdef CONFIG_LPS_LCLK
	if (pHalData->IntArray[0]  & IMR_CPWM_88E) {
		_rtw_memcpy(&pwr_rpt.state, &(pbuf[USB_INTR_CONTENT_CPWM1_OFFSET]), 1);
		/*_rtw_memcpy(&pwr_rpt.state2, &(pbuf[USB_INTR_CONTENT_CPWM2_OFFSET]), 1); */

		/*88e's cpwm value only change BIT0, so driver need to add PS_STATE_S2 for LPS flow. */
		pwr_rpt.state |= PS_STATE_S2;
		_set_workitem(&(adapter_to_pwrctl(padapter)->cpwm_event));
	}
#endif/*CONFIG_LPS_LCLK */

#ifdef CONFIG_INTERRUPT_BASED_TXBCN
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
	if (pHalData->IntArray[0] & IMR_BCNDMAINT0_88E)
		/* suspect code indent for conditional statements */
#endif
#ifdef CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
	if (pHalData->IntArray[0] & (IMR_TBDER_88E | IMR_TBDOK_88E))
		/* suspect code indent for conditional statements */
#endif
	{
		struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
#if 0
		if (pHalData->IntArray[0] & IMR_BCNDMAINT0_88E)
			DBG_8192C("%s: HISR_BCNERLY_INT\n", __func__);
		if (pHalData->IntArray[0] & IMR_TBDOK_88E)
			DBG_8192C("%s: HISR_TXBCNOK\n", __func__);
		if (pHalData->IntArray[0] & IMR_TBDER_88E)
			DBG_8192C("%s: HISR_TXBCNERR\n", __func__);
#endif
		if (check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
			/*send_beacon(padapter); */
			if (pmlmepriv->update_bcn == _TRUE) {
				/*tx_beacon_hdl(padapter, NULL); */
				set_tx_beacon_cmd(padapter);
			}
		}
#ifdef CONFIG_CONCURRENT_MODE
		if (check_buddy_fwstate(padapter, WIFI_AP_STATE)) {
			/*send_beacon(padapter); */
			if (padapter->pbuddy_adapter->mlmepriv.update_bcn == _TRUE) {
				/*tx_beacon_hdl(padapter, NULL); */
				set_tx_beacon_cmd(padapter->pbuddy_adapter);
			}
		}
#endif
	}
#endif /*CONFIG_INTERRUPT_BASED_TXBCN */




#ifdef DBG_CONFIG_ERROR_DETECT_INT
	if (pHalData->IntArray[1]  & IMR_TXERR_8188F)
		DBG_871X("===> %s Tx Error Flag Interrupt Status\n", __func__);
	if (pHalData->IntArray[1]  & IMR_RXERR_8188F)
		DBG_871X("===> %s Rx Error Flag INT Status\n", __func__);
	if (pHalData->IntArray[1]  & IMR_TXFOVW_8188F)
		DBG_871X("===> %s Transmit FIFO Overflow\n", __func__);
	if (pHalData->IntArray[1]  & IMR_RXFOVW_8188F)
		DBG_871X("===> %s Receive FIFO Overflow\n", __func__);
#endif/*DBG_CONFIG_ERROR_DETECT_INT */


	/* C2H Event */
	if (pbuf[0] != 0) {
		_rtw_memcpy(&(pHalData->C2hArray[0]), &(pbuf[USB_INTR_CONTENT_C2H_OFFSET]), 16);
		/*rtw_c2h_wk_cmd(padapter); to do.. */
	}

}
#endif

static s32 pre_recv_entry(union recv_frame *precvframe, u8 *pphy_status)
{
	s32 ret = _SUCCESS;
#ifdef CONFIG_CONCURRENT_MODE
	u8 *primary_myid, *secondary_myid, *paddr1;
	union recv_frame	*precvframe_if2 = NULL;
	_adapter *primary_padapter = precvframe->u.hdr.adapter;
	_adapter *secondary_padapter = primary_padapter->pbuddy_adapter;
	struct recv_priv *precvpriv = &primary_padapter->recvpriv;
	_queue *pfree_recv_queue = &precvpriv->free_recv_queue;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(primary_padapter);

	if (!secondary_padapter)
		return ret;

	paddr1 = GetAddr1Ptr(precvframe->u.hdr.rx_data);

	if (IS_MCAST(paddr1) == _FALSE) { /*unicast packets */
		/*primary_myid = myid(&primary_padapter->eeprompriv); */
		/*secondary_myid = myid(&secondary_padapter->eeprompriv); */
		secondary_myid = adapter_mac_addr(secondary_padapter);

		if (_rtw_memcmp(paddr1, secondary_myid, ETH_ALEN)) {
			/*change to secondary interface */
			precvframe->u.hdr.adapter = secondary_padapter;
		}

		/*ret = recv_entry(precvframe); */

	} else { /* Handle BC/MC Packets */
		/*clone/copy to if2 */
		_pkt	 *pkt_copy = NULL;
		struct rx_pkt_attrib *pattrib = NULL;

		precvframe_if2 = rtw_alloc_recvframe(pfree_recv_queue);

		if (!precvframe_if2)
			return _FAIL;

		precvframe_if2->u.hdr.adapter = secondary_padapter;
		_rtw_memcpy(&precvframe_if2->u.hdr.attrib, &precvframe->u.hdr.attrib, sizeof(struct rx_pkt_attrib));
		pattrib = &precvframe_if2->u.hdr.attrib;

		/*driver need to set skb len for skb_copy(). */
		/*If skb->len is zero, skb_copy() will not copy data from original skb. */
		skb_put(precvframe->u.hdr.pkt, pattrib->pkt_len);

		pkt_copy = rtw_skb_copy(precvframe->u.hdr.pkt);
		if (pkt_copy == NULL) {
			if ((pattrib->mfrag == 1) && (pattrib->frag_num == 0)) {
				DBG_8192C("pre_recv_entry(): rtw_skb_copy fail , drop frag frame\n");
				rtw_free_recvframe(precvframe, &precvpriv->free_recv_queue);
				return ret;
			}

			pkt_copy = rtw_skb_clone(precvframe->u.hdr.pkt);
			if (pkt_copy == NULL) {
				DBG_8192C("pre_recv_entry(): rtw_skb_clone fail , drop frame\n");
				rtw_free_recvframe(precvframe, &precvpriv->free_recv_queue);
				return ret;
			}
		}

		pkt_copy->dev = secondary_padapter->pnetdev;

		precvframe_if2->u.hdr.pkt = pkt_copy;
		precvframe_if2->u.hdr.rx_head = pkt_copy->head;
		precvframe_if2->u.hdr.rx_data = pkt_copy->data;
		precvframe_if2->u.hdr.rx_tail = skb_tail_pointer(pkt_copy);
		precvframe_if2->u.hdr.rx_end = skb_end_pointer(pkt_copy);
		precvframe_if2->u.hdr.len = pkt_copy->len;

		/*recvframe_put(precvframe_if2, pattrib->pkt_len); */

		if (pHalData->ReceiveConfig & RCR_APPFCS)
			recvframe_pull_tail(precvframe_if2, IEEE80211_FCS_LEN);

		if (pattrib->physt)
			rx_query_phy_status(precvframe_if2, pphy_status);

		if (rtw_recv_entry(precvframe_if2) != _SUCCESS) {
			RT_TRACE(_module_rtl871x_recv_c_, _drv_err_,
					 ("recvbuf2recvframe: rtw_recv_entry(precvframe) != _SUCCESS\n"));
		}
	}

	/*if (precvframe->u.hdr.attrib.physt) */
	/*	rx_query_phy_status(precvframe, pphy_status); */

	/*ret = rtw_recv_entry(precvframe); */
#endif

	return ret;

}

#if 0
static s32 pre_recv_entry(union recv_frame *precvframe, u8 *pphy_status)
{
	s32 ret = _SUCCESS;
#ifdef CONFIG_CONCURRENT_MODE
	u8 *primary_myid, *secondary_myid, *paddr1;
	union recv_frame	*precvframe_if2 = NULL;
	_adapter *primary_padapter = precvframe->u.hdr.adapter;
	_adapter *secondary_padapter = primary_padapter->pbuddy_adapter;
	struct recv_priv *precvpriv = &primary_padapter->recvpriv;
	_queue *pfree_recv_queue = &precvpriv->free_recv_queue;
	u8	*pbuf = precvframe->u.hdr.rx_data;

	if (!secondary_padapter)
		return ret;

	paddr1 = GetAddr1Ptr(pbuf);

	if (IS_MCAST(paddr1) == _FALSE) { /*unicast packets */
		/*primary_myid = myid(&primary_padapter->eeprompriv); */
		secondary_myid = myid(&secondary_padapter->eeprompriv);

		if (_rtw_memcmp(paddr1, secondary_myid, ETH_ALEN)) {
			/*change to secondary interface */
			precvframe->u.hdr.adapter = secondary_padapter;
		}

		/*ret = recv_entry(precvframe); */

	} else { /* Handle BC/MC Packets */

		u8 clone = _TRUE;
#if 0
		u8 type, subtype, *paddr2, *paddr3;

		type =  GetFrameType(pbuf);
		subtype = GetFrameSubType(pbuf); /*bit(7)~bit(2) */

		switch (type) {
		case WIFI_MGT_TYPE: /*Handle BC/MC mgnt Packets */
			if (subtype == WIFI_BEACON) {
				paddr3 = GetAddr3Ptr(precvframe->u.hdr.rx_data);

				if (check_fwstate(&secondary_padapter->mlmepriv, _FW_LINKED) &&
					_rtw_memcmp(paddr3, get_bssid(&secondary_padapter->mlmepriv), ETH_ALEN)) {
					/*change to secondary interface */
					precvframe->u.hdr.adapter = secondary_padapter;
					clone = _FALSE;
				}

				if (check_fwstate(&primary_padapter->mlmepriv, _FW_LINKED) &&
					_rtw_memcmp(paddr3, get_bssid(&primary_padapter->mlmepriv), ETH_ALEN)) {
					if (clone == _FALSE)
						clone = _TRUE;
					else
						clone = _FALSE;

					precvframe->u.hdr.adapter = primary_padapter;
				}

				if (check_fwstate(&primary_padapter->mlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING) ||
					check_fwstate(&secondary_padapter->mlmepriv, _FW_UNDER_SURVEY | _FW_UNDER_LINKING)) {
					clone = _TRUE;
					precvframe->u.hdr.adapter = primary_padapter;
				}

			} else if (subtype == WIFI_PROBEREQ) {
				/*probe req frame is only for interface2 */
				/*change to secondary interface */
				precvframe->u.hdr.adapter = secondary_padapter;
				clone = _FALSE;
			}
			break;
		case WIFI_CTRL_TYPE: /* Handle BC/MC ctrl Packets */

			break;
		case WIFI_DATA_TYPE: /*Handle BC/MC data Packets */
			/*Notes: AP MODE never rx BC/MC data packets */

			paddr2 = GetAddr2Ptr(precvframe->u.hdr.rx_data);

			if (_rtw_memcmp(paddr2, get_bssid(&secondary_padapter->mlmepriv), ETH_ALEN)) {
				/*change to secondary interface */
				precvframe->u.hdr.adapter = secondary_padapter;
				clone = _FALSE;
			}

			break;
		default:

			break;
		}
#endif

		if (_TRUE == clone) {
			/*clone/copy to if2 */
			struct rx_pkt_attrib *pattrib = NULL;

			precvframe_if2 = rtw_alloc_recvframe(pfree_recv_queue);
			if (precvframe_if2) {
				precvframe_if2->u.hdr.adapter = secondary_padapter;

				_rtw_init_listhead(&precvframe_if2->u.hdr.list);
				precvframe_if2->u.hdr.precvbuf = NULL;	/*can't access the precvbuf for new arch. */
				precvframe_if2->u.hdr.len = 0;

				_rtw_memcpy(&precvframe_if2->u.hdr.attrib, &precvframe->u.hdr.attrib, sizeof(struct rx_pkt_attrib));

				pattrib = &precvframe_if2->u.hdr.attrib;

				if (rtw_os_alloc_recvframe(secondary_padapter, precvframe_if2, pbuf, NULL) == _SUCCESS) {
					recvframe_put(precvframe_if2, pattrib->pkt_len);
					/*recvframe_pull(precvframe_if2, drvinfo_sz + RXDESC_SIZE); */

					if (pattrib->physt && pphy_status)
						rx_query_phy_status(precvframe_if2, pphy_status);

					ret = rtw_recv_entry(precvframe_if2);
				} else {
					rtw_free_recvframe(precvframe_if2, pfree_recv_queue);
					DBG_8192C("%s()-%d: alloc_skb() failed!\n", __func__, __LINE__);
				}

			}

		}

	}
	/*if (precvframe->u.hdr.attrib.physt) */
	/*	rx_query_phy_status(precvframe, pphy_status); */

	/*ret = rtw_recv_entry(precvframe); */

#endif

	return ret;

}
#endif

int recvbuf2recvframe(PADAPTER padapter, void *ptr)
{
	u8 *pbuf;
	u8 pkt_cnt = 0;
	u32 pkt_offset;
	s32 transfer_len;
	u8 *pdata, *pphy_status;
	union recv_frame *precvframe = NULL;
	struct rx_pkt_attrib *pattrib = NULL;
	PHAL_DATA_TYPE pHalData;
	struct recv_priv *precvpriv;
	_queue *pfree_recv_queue;
	_pkt *pskb;


	pHalData = GET_HAL_DATA(padapter);
	precvpriv = &padapter->recvpriv;
	pfree_recv_queue = &precvpriv->free_recv_queue;

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	pskb = NULL;
	transfer_len = (s32)((struct recv_buf *)ptr)->transfer_len;
	pbuf = ((struct recv_buf *)ptr)->pbuf;
#else /* !CONFIG_USE_USB_BUFFER_ALLOC_RX */
	pskb = (_pkt *)ptr;
	transfer_len = (s32)pskb->len;
	pbuf = pskb->data;
#endif /* !CONFIG_USE_USB_BUFFER_ALLOC_RX */

#ifdef CONFIG_USB_RX_AGGREGATION
	pkt_cnt = GET_RX_STATUS_DESC_USB_AGG_PKTNUM_8188F(pbuf);
#endif

	do {
		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if (precvframe == NULL) {
			DBG_8192C("%s: rtw_alloc_recvframe() failed! RX Drop!\n", __func__);
			goto _exit_recvbuf2recvframe;
		}

		if (transfer_len > 1500)
			_rtw_init_listhead(&precvframe->u.hdr.list);
		precvframe->u.hdr.precvbuf = NULL;	/*can't access the precvbuf for new arch. */
		precvframe->u.hdr.len = 0;

		rtl8188f_query_rx_desc_status(precvframe, pbuf);

		pattrib = &precvframe->u.hdr.attrib;

		if ((padapter->registrypriv.mp_mode == 0)
			&& ((pattrib->crc_err) || (pattrib->icv_err))) {
			DBG_8192C("%s: RX Warning! crc_err=%d icv_err=%d, skip!\n",
					  __func__, pattrib->crc_err, pattrib->icv_err);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + pattrib->pkt_len;
		if ((pattrib->pkt_len <= 0) || (pkt_offset > transfer_len)) {
			DBG_8192C("%s: RX Error! pkt_len=%d pkt_offset=%d transfer_len=%d\n",
					  __func__, pattrib->pkt_len, pkt_offset, transfer_len);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		pdata = pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz;
		if (rtw_os_alloc_recvframe(padapter, precvframe, pdata, pskb) == _FAIL) {
			DBG_8192C("%s: RX Error! rtw_os_alloc_recvframe FAIL!\n", __func__);

			rtw_free_recvframe(precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		recvframe_put(precvframe, pattrib->pkt_len);

		if (pattrib->pkt_rpt_type == NORMAL_RX) {
			if (pattrib->physt)
				pphy_status = pbuf + RXDESC_OFFSET;
			else
				pphy_status = NULL;

#ifdef CONFIG_CONCURRENT_MODE
			if (rtw_buddy_adapter_up(padapter)) {
				if (pre_recv_entry(precvframe, pphy_status) != _SUCCESS) {
					/* Return fail except data frame */
					/*DBG_8192C("%s: RX Error! (concurrent)pre_recv_entry FAIL!\n", __func__); */
				}
			}
#endif /* CONFIG_CONCURRENT_MODE */

			if (pphy_status)
				rx_query_phy_status(precvframe, pphy_status);

			if (rtw_recv_entry(precvframe) != _SUCCESS) {
				/* Return fail except data frame */
				/*DBG_8192C("%s: RX Error! rtw_recv_entry FAIL!\n", __func__); */
			}
		} else {
#ifdef CONFIG_C2H_PACKET_EN
			if (pattrib->pkt_rpt_type == C2H_PACKET)
				rtl8188f_c2h_packet_handler(padapter, precvframe->u.hdr.rx_data, pattrib->pkt_len);
			else {
				DBG_8192C("%s: [WARNNING] RX type(%d) not be handled!\n",
						  __func__, pattrib->pkt_rpt_type);
			}
#endif /* CONFIG_C2H_PACKET_EN */
			rtw_free_recvframe(precvframe, pfree_recv_queue);
		}

#ifdef CONFIG_USB_RX_AGGREGATION
		/* jaguar 8-byte alignment */
		pkt_offset = (u16)_RND8(pkt_offset);
		pkt_cnt--;
		pbuf += pkt_offset;
#endif
		transfer_len -= pkt_offset;
		precvframe = NULL;
	} while (transfer_len > 0);

_exit_recvbuf2recvframe:

	return _SUCCESS;
}


void rtl8188fu_xmit_tasklet(void *priv)
{
	int ret = _FALSE;
	_adapter *padapter = (_adapter *)priv;
	struct xmit_priv *pxmitpriv = &padapter->xmitpriv;

	while (1) {
		if (RTW_CANNOT_TX(padapter)) {
			DBG_8192C("xmit_tasklet => bDriverStopped or bSurpriseRemoved or bWritePortCancel\n");
			break;
		}

		if (rtw_xmit_ac_blocked(padapter) == _TRUE)
			break;

		ret = rtl8188fu_xmitframe_complete(padapter, pxmitpriv, NULL);

		if (ret == _FALSE)
			break;

	}

}



void rtl8188fu_set_intf_ops(struct _io_ops	*pops)
{
	_func_enter_;

	_rtw_memset((u8 *)pops, 0, sizeof(struct _io_ops));

	pops->_read8 = &usb_read8;
	pops->_read16 = &usb_read16;
	pops->_read32 = &usb_read32;
	pops->_read_mem = &usb_read_mem;
	pops->_read_port = &usb_read_port;

	pops->_write8 = &usb_write8;
	pops->_write16 = &usb_write16;
	pops->_write32 = &usb_write32;
	pops->_writeN = &usb_writeN;

#ifdef CONFIG_USB_SUPPORT_ASYNC_VDN_REQ
	pops->_write8_async = &usb_async_write8;
	pops->_write16_async = &usb_async_write16;
	pops->_write32_async = &usb_async_write32;
#endif
	pops->_write_mem = &usb_write_mem;
	pops->_write_port = &usb_write_port;

	pops->_read_port_cancel = &usb_read_port_cancel;
	pops->_write_port_cancel = &usb_write_port_cancel;

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	pops->_read_interrupt = &usb_read_interrupt;
#endif

	_func_exit_;

}
void rtl8188fu_set_hw_type(struct dvobj_priv *pdvobj)
{
	pdvobj->HardwareType = HARDWARE_TYPE_RTL8188FU;
	DBG_871X("CHIP TYPE: RTL8188FU\n");
}
