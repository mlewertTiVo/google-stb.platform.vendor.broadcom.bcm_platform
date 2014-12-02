/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/Timers.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"
#include "nexus_base_mmap.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "bkni.h"

extern
NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp);

#define NULL_LIST_SIZE 27

int gralloc_register_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;

   private_handle_t* hnd = (private_handle_t*)handle;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : INVALID HANDLE !!", __FUNCTION__);
      return -EINVAL;
   }

   LOGE("%s : HandlePID:%d procssPID;%d", __FUNCTION__, hnd->pid, getpid());

   if (hnd->pid != getpid())
   {
      NEXUS_PlatformStatus  pstatus;
      NEXUS_Error rc = NEXUS_SUCCESS;

      rc = NEXUS_Platform_GetStatus(&pstatus);
      if (rc != NEXUS_SUCCESS)
      {
         BCM_DEBUG_MSG("BRCM-GRALLOC:: Trying To Join:%d\n",getpid());
         rc = NEXUS_Platform_Join();
         if (rc == NEXUS_SUCCESS)
         {
            BCM_DEBUG_MSG("BRCM-GRALLOC:: Regr buff process %d joined nexus\n",getpid());
         }
         else
         {
            BCM_DEBUG_MSG("BRCM-GRALLOC:: Unable to proceed, nexus join has failed\n");
            return -EINVAL;
         }
      }
   }
   else
   {
      LOGE("%s:: Register buffer from same process :%d", __FUNCTION__, getpid());
   }

   pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

   return 0;
}

int gralloc_unregister_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   private_handle_t* hnd = (private_handle_t*)handle;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : INVALID HANDLE !!", __FUNCTION__);
      return -EINVAL;
   }

   LOGE("%s : HandlePID:%d procssPID:%d", __FUNCTION__, hnd->pid, getpid());

   return 0;
}

/*
 * This implementation does not really change since when we
 * allocate the buffer we fill in the base pointer of the
 * private handle using the surface buffer that was created.
 */
// TODO : lock actually takes width/height of the region you want to lock
// optimize the case for a sub region (if it ever ocurrs)
int gralloc_lock(gralloc_module_t const* module,
   buffer_handle_t handle, int usage,
   int l, int t, int w, int h,
   void** vaddr)
{
   int err = 0;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : returning EINVAL", __FUNCTION__);
      err = -EINVAL;
   }
   else
   {
      private_handle_t* hnd = (private_handle_t*)handle;

      LOGV("%s : successfully locked", __FUNCTION__);
      *vaddr = NEXUS_OffsetToCachedAddr(hnd->nxSurfacePhysicalAddress);
   }

   return err;
}

int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
   // we're done with a software buffer. nothing to do in this
   // implementation. typically this is used to flush the data cache.
   PSHARED_DATA pSharedData;

   private_handle_t *hnd = (private_handle_t *) handle;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : returning EINVAL", __FUNCTION__);
      return -EINVAL;
   }

   if (!hnd->nxSurfacePhysicalAddress)
   {
      LOGE("%s: !!!FATAL ERROR NULL NEXUS SURFACE HANDLE!!!", __FUNCTION__);
      return -EINVAL;
   }

   NEXUS_FlushCache(NEXUS_OffsetToCachedAddr(hnd->nxSurfacePhysicalAddress), hnd->oglSize);

   return 0;
}

