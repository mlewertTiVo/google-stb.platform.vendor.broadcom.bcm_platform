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

extern int gralloc_log_mapper();

int gralloc_register_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t* hnd = (private_handle_t*)handle;
   /* by default, use the default plane holding the buffer used by the gl stack, this
    * may be overwritten if a gl_plane exists which would mean we want to use that value
    * instead. */
   int plane = DEFAULT_PLANE;
   void *pMemory;

   (void)module;

   if (private_handle_t::validate(handle) < 0) {
      return -EINVAL;
   }

   if (hnd->is_mma) {
      pMemory = NULL;
      block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   }

   if (pSharedData != NULL) {
      if (pSharedData->planes[GL_PLANE].physAddr) {
         plane = GL_PLANE;
      }
      if (hnd->is_mma) {
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlockHandle)pSharedData->planes[plane].physAddr, &pMemory);
         hnd->nxSurfaceAddress = (unsigned)pMemory;
      } else {
         hnd->nxSurfaceAddress = (unsigned)NEXUS_OffsetToCachedAddr(pSharedData->planes[plane].physAddr);
      }

      if (gralloc_log_mapper()) {
         ALOGI("  reg: owner:%d::s-blk:0x%x::p:%d::p-blk:0x%x::p-addr:0x%x::mapped:0x%x::act:%d",
               hnd->pid,
               hnd->sharedData,
               plane,
               pSharedData->planes[plane].physAddr,
               hnd->nxSurfacePhysicalAddress,
               hnd->nxSurfaceAddress,
               getpid());
      }

      if (hnd->is_mma) {
         NEXUS_MemoryBlock_Unlock(block_handle);
      }
   }

   return 0;
}

int gralloc_unregister_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t* hnd = (private_handle_t*)handle;
   int plane = DEFAULT_PLANE;
   void *pMemory;

   (void)module;

   if (private_handle_t::validate(handle) < 0) {
      return -EINVAL;
   }

   if (hnd->is_mma) {
      pMemory = NULL;
      block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   }

   if (pSharedData != NULL) {
      if (pSharedData->planes[GL_PLANE].physAddr) {
         plane = GL_PLANE;
      }

      if (gralloc_log_mapper()) {
         ALOGI("unreg: owner:%d::s-blk:0x%x::p:%d::p-blk:0x%x::p-addr:0x%x::mapped:0x%x::act:%d",
               hnd->pid,
               hnd->sharedData,
               plane,
               pSharedData->planes[plane].physAddr,
               hnd->nxSurfacePhysicalAddress,
               hnd->nxSurfaceAddress,
               getpid());
      }

      if (hnd->is_mma) {
         NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlockHandle)pSharedData->planes[plane].physAddr);
         NEXUS_MemoryBlock_Unlock(block_handle);
      }
   }

   return 0;
}

// TODO : lock actually takes width/height of the region you want to lock
// optimize the case for a sub region (if it ever ocurrs)
int gralloc_lock(gralloc_module_t const* module,
   buffer_handle_t handle, int usage,
   int l, int t, int w, int h,
   void** vaddr)
{
   int err = 0;
   bool hwConverted=false;
   NEXUS_MemoryBlockHandle block_handle = NULL, shared_block_handle = NULL;
   PSHARED_DATA pSharedData = NULL;
   private_module_t* pModule = (private_module_t *)module;
   (void)l;
   (void)t;
   (void)w;
   (void)h;

   if (private_handle_t::validate(handle) < 0) {
      return -EINVAL;
   }

   private_handle_t* hnd = (private_handle_t*)handle;

   if (hnd->is_mma) {
      void *pMemory;
      shared_block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
      NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
      block_handle = (NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr;
      NEXUS_MemoryBlock_Lock(block_handle, vaddr);
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
      *vaddr = NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
   }

   if (android_atomic_acquire_load(&pSharedData->hwc.active) && (usage & GRALLOC_USAGE_SW_WRITE_MASK)) {
      ALOGE("Locking gralloc buffer %#x inuse by HWC!  HWC Layer %d NEXUS_SurfaceHandle %#x",
            hnd->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface);
   }

   if (pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12) {
      pthread_mutex_lock(gralloc_g2d_lock());
      err = private_handle_t::lock_video_frame(hnd, 250);
      if (err) {
         ALOGE("Unable to lock video frame data (timeout)");
         pthread_mutex_unlock(gralloc_g2d_lock());
         goto out;
      }
      if (pSharedData->videoFrame.hStripedSurface) {
         if (usage & GRALLOC_USAGE_SW_WRITE_MASK) {
            ALOGE("Cannot lock HW video decoder buffers for SW_WRITE");
            private_handle_t::unlock_video_frame(hnd);
            pthread_mutex_unlock(gralloc_g2d_lock());
            err = -EINVAL;
            goto out;
         }
         if (usage & GRALLOC_USAGE_SW_READ_MASK) {
            if (!pSharedData->videoFrame.destripeComplete) {
               err = gralloc_destripe_yv12(hnd, pSharedData->videoFrame.hStripedSurface);
               if (err) {
                  private_handle_t::unlock_video_frame(hnd);
                  pthread_mutex_unlock(gralloc_g2d_lock());
                  goto out;
               }
               pSharedData->videoFrame.destripeComplete = true;
               hwConverted = true;
               ALOGV("%s: Destripe Complete", __FUNCTION__);
            }
         }
      }
      private_handle_t::unlock_video_frame(hnd);
      pthread_mutex_unlock(gralloc_g2d_lock());
   }

   if ((usage & (GRALLOC_USAGE_SW_READ_MASK|GRALLOC_USAGE_SW_WRITE_MASK)) && !hwConverted) {
      NEXUS_FlushCache(*vaddr, pSharedData->planes[DEFAULT_PLANE].allocSize);
   }

   if (gralloc_log_mapper()) {
      ALOGI(" lock: owner:%d::s-blk:0x%x::p:%d::p-blk:0x%x::p-addr:0x%x::mapped:0x%x::vaddr:0x%x::act:%d",
            hnd->pid,
            hnd->sharedData,
            DEFAULT_PLANE,
            pSharedData->planes[DEFAULT_PLANE].physAddr,
            hnd->nxSurfacePhysicalAddress,
            hnd->nxSurfaceAddress,
            *vaddr,
            getpid());
   }

out:
   if (hnd->is_mma && shared_block_handle) {
      NEXUS_MemoryBlock_Unlock(shared_block_handle);
   }
   return err;
}

int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
   NEXUS_Error rc;

   NEXUS_MemoryBlockHandle block_handle = NULL, shared_block_handle = NULL;
   private_handle_t *hnd = (private_handle_t *) handle;
   private_module_t* pModule = (private_module_t *)module;

   if (private_handle_t::validate(handle) < 0) {
      return -EINVAL;
   }

   PSHARED_DATA pSharedData = NULL;
   if (hnd->is_mma) {
      void *pMemory;
      shared_block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
      NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
      block_handle = (NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   }

   if (hnd->usage & GRALLOC_USAGE_SW_WRITE_MASK) {
      bool flushed = false;

      if (!pSharedData->planes[DEFAULT_PLANE].physAddr) {
         return -EINVAL;
      }

      if (pSharedData->planes[DEFAULT_PLANE].format == HAL_PIXEL_FORMAT_YV12) {
         if (pSharedData->planes[EXTRA_PLANE].physAddr != 0) {
            pthread_mutex_lock(gralloc_g2d_lock());
            rc = gralloc_yv12to422p(hnd);
            if (!rc) {
               flushed = true;
            }

            /* TODO: YV12->RBA conversion */
            if (!rc && pSharedData->planes[GL_PLANE].physAddr != 0) {
               rc = gralloc_plane_copy(hnd, EXTRA_PLANE, GL_PLANE);
               if (rc) {
                  ALOGE("%s: Error converting from 422p to RGB - %d", __FUNCTION__, rc);
               }
            }
            pthread_mutex_unlock(gralloc_g2d_lock());
         }
      }

      if (!flushed) {
         void *vaddr;
         if (hnd->is_mma) {
            NEXUS_MemoryBlock_Lock(block_handle, &vaddr);
         } else {
            vaddr = NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
         }
         NEXUS_FlushCache(vaddr, pSharedData->planes[DEFAULT_PLANE].allocSize);
      }
   }

   if (gralloc_log_mapper()) {
      ALOGI("ulock: owner:%d::s-blk:0x%x::p:%d::p-blk:0x%x::p-addr:0x%x::mapped:0x%x::act:%d",
            hnd->pid,
            hnd->sharedData,
            DEFAULT_PLANE,
            pSharedData->planes[DEFAULT_PLANE].physAddr,
            hnd->nxSurfacePhysicalAddress,
            hnd->nxSurfaceAddress,
            getpid());
   }

   if (hnd->is_mma) {
      if (block_handle) {
         NEXUS_MemoryBlock_Unlock(block_handle);
      }
      if (shared_block_handle) {
         NEXUS_MemoryBlock_Unlock(shared_block_handle);
      }
   }

   return 0;
}
