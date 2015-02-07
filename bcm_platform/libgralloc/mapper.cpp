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
#include "gralloc_destripe.h"

extern
NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp);

#define NULL_LIST_SIZE 27

int gralloc_register_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   private_handle_t* hnd = (private_handle_t*)handle;

   (void)module;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : INVALID HANDLE !!", __FUNCTION__);
      return -EINVAL;
   }

   pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   LOGI("%s: parent:%d, registrant:%d, addr:0x%x", __FUNCTION__,
        hnd->pid, getpid(), pSharedData->planes[DEFAULT_PLANE].physAddr);

   return 0;
}

int gralloc_unregister_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   private_handle_t* hnd = (private_handle_t*)handle;

   (void)module;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : INVALID HANDLE !!", __FUNCTION__);
      return -EINVAL;
   }

   pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   LOGI("%s: parent:%d, registrant:%d, addr:0x%x", __FUNCTION__,
        hnd->pid, getpid(), pSharedData->planes[DEFAULT_PLANE].physAddr);

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
   bool hwConverted=false;

   private_module_t* pModule = (private_module_t *)module;
   (void)l;
   (void)t;
   (void)w;
   (void)h;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : returning EINVAL", __FUNCTION__);
      err = -EINVAL;
   }
   else
   {
      private_handle_t* hnd = (private_handle_t*)handle;

      PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
      *vaddr = NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);

      if ( android_atomic_acquire_load(&pSharedData->hwc.active) && (usage & GRALLOC_USAGE_SW_WRITE_MASK) )
      {
         ALOGE("Locking gralloc buffer %#x inuse by HWC!  HWC Layer %d NEXUS_SurfaceHandle %#x", hnd->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface);
      }

      // See if we're most likely dealing with video data
      if ( pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12 )
      {
         // Serialize for graphics checkpoint
         pthread_mutex_lock(gralloc_g2d_lock());
         // Lock video frame data
         err = private_handle_t::lock_video_frame(hnd, 250);
         if ( err )
         {
            ALOGE("Unable to lock video frame data (timeout)");
            pthread_mutex_unlock(gralloc_g2d_lock());
            return err;
         }
         // Is there a HW decoded frame present?
         if ( pSharedData->videoFrame.hStripedSurface )
         {
            // SW cannot write this data
            if ( usage & GRALLOC_USAGE_SW_WRITE_MASK )
            {
               ALOGE("Cannot lock HW video decoder buffers for SW_WRITE");
               private_handle_t::unlock_video_frame(hnd);
               pthread_mutex_unlock(gralloc_g2d_lock());
               return -EINVAL;
            }
            // If SW wants to read we may need to destripe first
            if ( usage & GRALLOC_USAGE_SW_READ_MASK )
            {
               if ( !pSharedData->videoFrame.destripeComplete )
               {
                  err = gralloc_destripe_yv12(hnd, pSharedData->videoFrame.hStripedSurface);
                  if ( err )
                  {
                     private_handle_t::unlock_video_frame(hnd);
                     pthread_mutex_unlock(gralloc_g2d_lock());
                     return err;
                  }
                  pSharedData->videoFrame.destripeComplete = true;
                  hwConverted = true;
                  LOGV("%s: Destripe Complete", __FUNCTION__);
               }
            }
         }
         private_handle_t::unlock_video_frame(hnd);
         pthread_mutex_unlock(gralloc_g2d_lock());
      }

      LOGV("%s : successfully locked", __FUNCTION__);
      if ( (usage & (GRALLOC_USAGE_SW_READ_MASK|GRALLOC_USAGE_SW_WRITE_MASK)) && !hwConverted )
      {
           NEXUS_FlushCache(NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr), pSharedData->planes[DEFAULT_PLANE].allocSize);
      }
   }

   return err;
}

int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
   NEXUS_Error rc;

   private_handle_t *hnd = (private_handle_t *) handle;
   private_module_t* pModule = (private_module_t *)module;

   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("%s : returning EINVAL", __FUNCTION__);
      return -EINVAL;
   }

   /* produce a packed version of the buffer that will be used for composition.
    */
   if (hnd->usage & GRALLOC_USAGE_SW_WRITE_MASK)
   {
      bool flushed = false;
      PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
      if (!pSharedData->planes[DEFAULT_PLANE].physAddr)
      {
         LOGE("%s: !!!FATAL ERROR NULL NEXUS SURFACE HANDLE!!!", __FUNCTION__);
         return -EINVAL;
      }

      if (pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12)
      {
         if (pSharedData->planes[EXTRA_PLANE].physAddr != 0)
         {
            pthread_mutex_lock(gralloc_g2d_lock());
            rc = gralloc_yv12to422p(hnd);
            if (rc)
            {
               /* TODO: marked as invalid so we drop it */
            }
            else
            {
               flushed = true;
            }

            /* TODO: YV12->RBA conversion */
            if (!rc && pSharedData->planes[GL_PLANE].physAddr != 0)
            {
               rc = gralloc_plane_copy(hnd, EXTRA_PLANE, GL_PLANE);
               if (rc)
               {
                  LOGE("%s: Error converting from 422p to RGB - %d", __FUNCTION__, rc);
               }
            }
            pthread_mutex_unlock(gralloc_g2d_lock());
         }
      }

      if (!flushed)
      {
         NEXUS_FlushCache(NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr),
                          pSharedData->planes[DEFAULT_PLANE].allocSize);
      }
   }

   return 0;
}
