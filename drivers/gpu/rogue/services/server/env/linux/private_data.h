/*************************************************************************/ /*!
@File
@Title          Linux private data structure
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#if !defined(__INCLUDED_PRIVATE_DATA_H_)
#define __INCLUDED_PRIVATE_DATA_H_

#if defined(SUPPORT_DRM_AUTH_IMPORT)
#include <linux/list.h>
#endif
#if defined(SUPPORT_DRM)
#include <linux/atomic.h>
#include <drm/drmP.h>
#endif

#include <linux/fs.h>

#include "connection_server.h"
#include "env_connection.h"

/* This structure is required in the rare case that a process creates
 * a connection to services, but before closing the file descriptor,
 * does a fork(). This fork() will duplicate the file descriptor in the
 * child process. If the parent process dies before the child, this can
 * cause the PVRSRVRelease() method to be called in a different process
 * context than the original PVRSRVOpen(). This is bad because we need
 * to update the per-process data reference count and/or free the
 * per-process data. So we must keep a record of which PID's per-process
 * data to inspect during ->release().
 */

typedef struct
{
	IMG_PVOID pvConnectionData;

#if defined(PVR_SECURE_FD_EXPORT)
	/* Global kernel MemInfo handle */
	IMG_HANDLE hKernelMemInfo;
#endif /* defined(PVR_SECURE_FD_EXPORT) */

#if defined(SUPPORT_DRM_AUTH_IMPORT)
	struct list_head sDRMAuthListItem;

	IMG_PID uPID;
#endif
}
PVRSRV_FILE_PRIVATE_DATA;

#if defined(SUPPORT_DRM)
CONNECTION_DATA *LinuxConnectionFromFile(struct drm_file *pFile);
#else
CONNECTION_DATA *LinuxConnectionFromFile(struct file *pFile);
#endif

struct file *LinuxFileFromEnvConnection(ENV_CONNECTION_DATA *psEnvConnection);

#endif /* !defined(__INCLUDED_PRIVATE_DATA_H_) */

