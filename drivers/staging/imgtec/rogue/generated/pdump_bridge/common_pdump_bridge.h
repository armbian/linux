/*************************************************************************/ /*!
@File
@Title          Common bridge header for pdump
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures used by both the client
                and server side of the bridge for pdump
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef COMMON_PDUMP_BRIDGE_H
#define COMMON_PDUMP_BRIDGE_H

#include <powervr/mem_types.h>

#include "img_types.h"
#include "pvrsrv_error.h"

#include "devicemem_typedefs.h"
#include "pdumpdefs.h"


#define PVRSRV_BRIDGE_PDUMP_CMD_FIRST			0
#define PVRSRV_BRIDGE_PDUMP_DEVMEMPDUMPBITMAP			PVRSRV_BRIDGE_PDUMP_CMD_FIRST+0
#define PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPCOMMENT			PVRSRV_BRIDGE_PDUMP_CMD_FIRST+1
#define PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSETFRAME			PVRSRV_BRIDGE_PDUMP_CMD_FIRST+2
#define PVRSRV_BRIDGE_PDUMP_CMD_LAST			(PVRSRV_BRIDGE_PDUMP_CMD_FIRST+2)


/*******************************************
            DevmemPDumpBitmap          
 *******************************************/

/* Bridge in structure for DevmemPDumpBitmap */
typedef struct PVRSRV_BRIDGE_IN_DEVMEMPDUMPBITMAP_TAG
{
	IMG_CHAR * puiFileName;
	IMG_UINT32 ui32FileOffset;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32StrideInBytes;
	IMG_DEV_VIRTADDR sDevBaseAddr;
	IMG_HANDLE hDevmemCtx;
	IMG_UINT32 ui32Size;
	PDUMP_PIXEL_FORMAT ePixelFormat;
	IMG_UINT32 ui32AddrMode;
	IMG_UINT32 ui32PDumpFlags;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_DEVMEMPDUMPBITMAP;

/* Bridge out structure for DevmemPDumpBitmap */
typedef struct PVRSRV_BRIDGE_OUT_DEVMEMPDUMPBITMAP_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_DEVMEMPDUMPBITMAP;


/*******************************************
            PVRSRVPDumpComment          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpComment */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT_TAG
{
	IMG_CHAR * puiComment;
	IMG_UINT32 ui32Flags;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT;

/* Bridge out structure for PVRSRVPDumpComment */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT;


/*******************************************
            PVRSRVPDumpSetFrame          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpSetFrame */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME_TAG
{
	IMG_UINT32 ui32Frame;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME;

/* Bridge out structure for PVRSRVPDumpSetFrame */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME;


#endif /* COMMON_PDUMP_BRIDGE_H */
