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
extern int gralloc_boom_check();
extern int gralloc_timestamp_conversion();

static int64_t gralloc_tick(void)
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

enum {
   CONV_NONE = 0,
   CONV_DESTRIPE,
};

static const char *conv_type[] =
{
   "invalid-conv",
   "destripe-yuv",
};

static void gralloc_conv_print(int w, int h, int type, int64_t start, int64_t end)
{
   const double nsec_to_msec = 1.0 / 1000000.0;

   ALOGI("conv: %s (%dx%d) -> %.3f msecs (%lld)",
         conv_type[type],
         w,
         h,
         nsec_to_msec * (end - start),
         (end - start));
}

int gralloc_register_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t* hnd = (private_handle_t*)handle;
   void *pMemory;
   NEXUS_Error lrc;

   (void)module;

   if (private_handle_t::validate(handle) < 0) {
      ALOGE("%s : invalid handle, freed?", __FUNCTION__);
      return -EINVAL;
   }

   pMemory = NULL;
   block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   if (lrc) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
      ALOGE("%s : invalid lock for 0x%x", __FUNCTION__, block_handle);
      return -EINVAL;
   }
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData != NULL) {
      if ((hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) && pSharedData->container.physAddr) {
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr, &pMemory);
         hnd->nxSurfaceAddress = (unsigned)pMemory;
      }

      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr;
         unsigned sharedPhysAddr = hnd->sharedData;
         NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
         sharedPhysAddr = (unsigned)physAddr;
         ALOGI("  reg (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::%dx%d::sz:%d::use:0x%x:0x%x::mapped:0x%x::act:%d",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
               hnd->pid,
               hnd->sharedData,
               sharedPhysAddr,
               pSharedData->container.physAddr,
               hnd->nxSurfacePhysicalAddress,
               pSharedData->container.width,
               pSharedData->container.height,
               pSharedData->container.size,
               hnd->usage,
               hnd->fmt_set,
               hnd->nxSurfaceAddress,
               getpid());
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
      }
   }
   if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);

   return 0;
}

int gralloc_unregister_buffer(gralloc_module_t const* module,
   buffer_handle_t handle)
{
   PSHARED_DATA pSharedData;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   private_handle_t* hnd = (private_handle_t*)handle;
   void *pMemory;
   NEXUS_Error lrc;

   (void)module;

   if (private_handle_t::validate(handle) < 0) {
      ALOGE("%s : invalid handle, freed?", __FUNCTION__);
      return -EINVAL;
   }

   pMemory = NULL;
   block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   if (lrc) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
      ALOGE("%s : invalid lock for 0x%x", __FUNCTION__, block_handle);
      return -EINVAL;
   }
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData != NULL) {
      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr;
         unsigned sharedPhysAddr = hnd->sharedData;
         NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
         sharedPhysAddr = (unsigned)physAddr;
         ALOGI("unreg (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::%dx%d::sz:%d::use:0x%x:0x%x::mapped:0x%x::act:%d",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
               hnd->pid,
               hnd->sharedData,
               sharedPhysAddr,
               pSharedData->container.physAddr,
               hnd->nxSurfacePhysicalAddress,
               pSharedData->container.width,
               pSharedData->container.height,
               pSharedData->container.size,
               hnd->usage,
               hnd->fmt_set,
               hnd->nxSurfaceAddress,
               getpid());
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
      }

      if ((hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) && pSharedData->container.physAddr) {
         NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr);
         if (gralloc_boom_check()) {
            NEXUS_MemoryBlock_CheckIfLocked((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr);
         }
      }
   }

   if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   if (gralloc_boom_check()) {
      NEXUS_MemoryBlock_CheckIfLocked(block_handle);
   }

   return 0;
}

int gralloc_lock(gralloc_module_t const* module,
   buffer_handle_t handle, int usage,
   int l, int t, int w, int h,
   void** vaddr)
{
   int64_t tick_start, tick_end;
   int err = 0;
   NEXUS_Error lrc;
   bool hwConverted=false;
   NEXUS_MemoryBlockHandle block_handle = NULL, shared_block_handle = NULL;
   PSHARED_DATA pSharedData = NULL;
   private_module_t* pModule = (private_module_t *)module;

   (void)l;
   (void)t;
   (void)w;
   (void)h;

   if (private_handle_t::validate(handle) < 0) {
      ALOGE("%s : invalid handle, freed?", __FUNCTION__);
      return -EINVAL;
   }

   private_handle_t* hnd = (private_handle_t*)handle;

   void *pMemory;
   shared_block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   lrc = NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (lrc || pSharedData == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(shared_block_handle);
      ALOGE("%s : invalid private buffer %p, freed?", __FUNCTION__, shared_block_handle);
      return -EINVAL;
   }
   block_handle = (NEXUS_MemoryBlockHandle)pSharedData->container.physAddr;
   if (block_handle) {
      NEXUS_MemoryBlock_Lock(block_handle, vaddr);
      if (hnd->mgmt_mode != GR_MGMT_MODE_LOCKED) {
         NEXUS_Addr physAddr;
         hnd->nxSurfaceAddress = (unsigned)vaddr;
         NEXUS_MemoryBlock_LockOffset((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr, &physAddr);
         hnd->nxSurfacePhysicalAddress = (unsigned)physAddr;
      }
   } else {
      ALOGE("no default plane on s-blk:0x%x", shared_block_handle);
      *vaddr = NULL;
   }

   if (pSharedData->container.format == HAL_PIXEL_FORMAT_YV12) {
      pthread_mutex_lock(gralloc_g2d_lock());
      err = private_handle_t::lock_video_frame(hnd, 250);
      if (err) {
         ALOGE("Unable to lock video frame data (timeout)");
         pthread_mutex_unlock(gralloc_g2d_lock());
         goto out_video_failed;
      }
      if (pSharedData->videoFrame.hStripedSurface) {
         if (usage & GRALLOC_USAGE_SW_WRITE_MASK) {
            ALOGE("invalid lock of stripped buffers for SW_WRITE");
            private_handle_t::unlock_video_frame(hnd);
            pthread_mutex_unlock(gralloc_g2d_lock());
            err = -EINVAL;
            goto out_video_failed;
         }
         if (usage & GRALLOC_USAGE_SW_READ_MASK) {
            if (!pSharedData->videoFrame.destripeComplete) {
               if (gralloc_timestamp_conversion()) {
                  tick_start = gralloc_tick();
               }
               err = gralloc_destripe_yv12(hnd, pSharedData->videoFrame.hStripedSurface);
               if (err) {
                  private_handle_t::unlock_video_frame(hnd);
                  pthread_mutex_unlock(gralloc_g2d_lock());
                  goto out_video_failed;
               }
               if (gralloc_timestamp_conversion()) {
                  tick_end = gralloc_tick();
               }
               pSharedData->videoFrame.destripeComplete = true;
               hwConverted = true;
            }
         }
      } else if ((hnd->fmt_set & GR_HWTEX) == GR_HWTEX) {
         /* TODO: add behavior for metadata buffer mode processing of stripped
          *       surface as needed...
          */
      }
      private_handle_t::unlock_video_frame(hnd);
      pthread_mutex_unlock(gralloc_g2d_lock());
   }

out_video_failed:
   if ((usage & (GRALLOC_USAGE_SW_READ_MASK|GRALLOC_USAGE_SW_WRITE_MASK)) && !hwConverted) {
      NEXUS_FlushCache(*vaddr, pSharedData->container.allocSize);
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr physAddr;
      unsigned sharedPhysAddr = hnd->sharedData;
      NEXUS_MemoryBlock_LockOffset(shared_block_handle, &physAddr);
      sharedPhysAddr = (unsigned)physAddr;
      ALOGI(" lock (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::%dx%d::sz:%d::use:0x%x:0x%x::mapped:0x%x::vaddr:0x%x::act:%d",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
            hnd->pid,
            hnd->sharedData,
            sharedPhysAddr,
            pSharedData->container.physAddr,
            hnd->nxSurfacePhysicalAddress,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            hnd->fmt_set,
            hnd->nxSurfaceAddress,
            *vaddr,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(shared_block_handle);
   }

   if (hwConverted && gralloc_timestamp_conversion()) {
      gralloc_conv_print(pSharedData->container.width,
                         pSharedData->container.height,
                         CONV_DESTRIPE, tick_start, tick_end);
   }

out:
   if (shared_block_handle) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(shared_block_handle);
   }
   return err;
}

int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
   NEXUS_Error rc;
   NEXUS_Error lrc;
   NEXUS_MemoryBlockHandle block_handle = NULL, shared_block_handle = NULL;
   private_handle_t *hnd = (private_handle_t *) handle;
   private_module_t* pModule = (private_module_t *)module;

   if (private_handle_t::validate(handle) < 0) {
      ALOGE("%s : invalid handle, freed?", __FUNCTION__);
      return -EINVAL;
   }

   PSHARED_DATA pSharedData = NULL;
   void *pMemory = NULL;
   shared_block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   lrc = NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(shared_block_handle);
   pSharedData = (PSHARED_DATA) pMemory;
   block_handle = (NEXUS_MemoryBlockHandle)pSharedData->container.physAddr;

   if (hnd->usage & GRALLOC_USAGE_SW_WRITE_MASK) {
      void *vaddr;
      if (!pSharedData->container.physAddr) {
         if (shared_block_handle) {
            NEXUS_MemoryBlock_Unlock(shared_block_handle);
         }
         return -EINVAL;
      }

      NEXUS_MemoryBlock_Lock(block_handle, &vaddr);
      NEXUS_FlushCache(vaddr, pSharedData->container.allocSize);
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr physAddr;
      unsigned sharedPhysAddr = hnd->sharedData;
      NEXUS_MemoryBlock_LockOffset(shared_block_handle, &physAddr);
      sharedPhysAddr = (unsigned)physAddr;
      ALOGI("ulock (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::%dx%d::sz:%d::use:0x%x:0x%x::mapped:0x%x::act:%d",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
            hnd->pid,
            hnd->sharedData,
            sharedPhysAddr,
            pSharedData->container.physAddr,
            hnd->nxSurfacePhysicalAddress,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            hnd->fmt_set,
            hnd->nxSurfaceAddress,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(shared_block_handle);
   }

   if (block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
      if (hnd->mgmt_mode != GR_MGMT_MODE_LOCKED) {
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
      }
   }
   if (shared_block_handle) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(shared_block_handle);
   }

   return 0;
}
