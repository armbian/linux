/** @file Mrvl_sha256_crypto.h
 *
 *  @brief This file contains the define for sha256
 *
 * Copyright (C) 2014-2017, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

/******************************************************
Change log:
    03/07/2014: Initial version
******************************************************/

#ifndef _MRVL_SHA256_CRYPTO_H
#define _MRVL_SHA256_CRYPTO_H

extern void mrvl_sha256_crypto_vector(void *priv, size_t num_elem,
				      UINT8 *addr[], size_t * len, UINT8 *mac);

extern void mrvl_sha256_crypto_kdf(void *priv, UINT8 *pKey,
				   UINT8 key_len,
				   char *label,
				   UINT8 label_len,
				   UINT8 *pContext,
				   UINT16 context_len,
				   UINT8 *pOutput, UINT16 output_len);

#endif
