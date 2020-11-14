/*************************************************************************/ /*!
@File
@Title          Services external synchronisation checkpoint interface header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines synchronisation checkpoint structures that are visible
				internally and externally
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

#ifndef _SYNC_CHECKPOINT_EXTERNAL_
#define _SYNC_CHECKPOINT_EXTERNAL_

#define SYNC_CHECKPOINT_MAX_CLASS_NAME_LEN 32

typedef struct _SYNC_CHECKPOINT_CONTEXT *PSYNC_CHECKPOINT_CONTEXT;

typedef struct _SYNC_CHECKPOINT *PSYNC_CHECKPOINT;

/* PVRSRV_SYNC_CHECKPOINT states.
 * The OS native sync implementation should call pfnIsSignalled() to determine if a
 * PVRSRV_SYNC_CHECKPOINT has signalled (which will return an IMG_BOOL), but can set the
 * state for a PVRSRV_SYNC_CHECKPOINT (which is currently in the NOT_SIGNALLED state)
 * where that PVRSRV_SYNC_CHECKPOINT is representing a foreign sync.
 */
typedef enum
{
    PVRSRV_SYNC_CHECKPOINT_NOT_SIGNALLED = 0x0,  /*!< checkpoint has not signalled */
    PVRSRV_SYNC_CHECKPOINT_SIGNALLED     = 0x1,  /*!< checkpoint has signalled */
    PVRSRV_SYNC_CHECKPOINT_ERRORED       = 0x3   /*!< checkpoint has been errored */
} PVRSRV_SYNC_CHECKPOINT_STATE;

#if defined(PVR_USE_SYNC_CHECKPOINTS)
#if defined(__KERNEL__) && defined(LINUX) && !defined(__GENKSYMS__)
#define __pvrsrv_defined_struct_enum__
#include <services_kernel_client.h>
#endif
#endif
#endif /* _SYNC_CHECKPOINT_EXTERNAL_ */
