/*
 * Copyright (C) 2014-2016 Broadcom Canada Ltd.
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

#include <cutils/atomic.h>
#include "gralloc_priv.h"
#include "nexus_base_mmap.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "bkni.h"
#include "gralloc_destripe.h"
#include <inttypes.h>

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

   ALOGI("conv: %s (%dx%d) -> %.3f msecs (%" PRId64 ")",
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
   private_handle_t::get_block_handles(hnd, &block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   if (lrc) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
      ALOGE("%s : invalid lock for %p", __FUNCTION__, block_handle);
      return -EINVAL;
   }
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData != NULL) {
      if ((hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) && pSharedData->container.block) {
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock(pSharedData->container.block, &pMemory);
         hnd->nxSurfaceAddress = (uint64_t)pMemory;
      }

      if (gralloc_log_mapper()) {
         NEXUS_Addr sPhysAddr, pPhysAddr;
         NEXUS_MemoryBlock_LockOffset(block_handle, &sPhysAddr);
         if (pSharedData->container.block) {NEXUS_MemoryBlock_LockOffset(pSharedData->container.block, &pPhysAddr);}
         else {pPhysAddr = 0;}
         ALOGI("  reg (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x::act:%d",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
               hnd,
               hnd->pid,
               block_handle,
               sPhysAddr,
               pSharedData->container.block,
               pPhysAddr,
               pSharedData->container.width,
               pSharedData->container.height,
               pSharedData->container.size,
               hnd->usage,
               pSharedData->container.format,
               getpid());
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
         if (pSharedData->container.block) {NEXUS_MemoryBlock_UnlockOffset(pSharedData->container.block);}
      }
   }
   if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   /* mark the block as valid. */
   hnd->magic = 0x4F77656E;

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
   private_handle_t::get_block_handles(hnd, &block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   if (lrc) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
      ALOGE("%s : invalid lock for %p", __FUNCTION__, block_handle);
      return -EINVAL;
   }
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData != NULL) {
      if (gralloc_log_mapper()) {
         NEXUS_Addr sPhysAddr, pPhysAddr;
         NEXUS_MemoryBlock_LockOffset(block_handle, &sPhysAddr);
         if (pSharedData->container.block) {NEXUS_MemoryBlock_LockOffset(pSharedData->container.block, &pPhysAddr);}
         else {pPhysAddr = 0;}
         ALOGI("unreg (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x::act:%d",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
               hnd,
               hnd->pid,
               block_handle,
               sPhysAddr,
               pSharedData->container.block,
               pPhysAddr,
               pSharedData->container.width,
               pSharedData->container.height,
               pSharedData->container.size,
               hnd->usage,
               pSharedData->container.format,
               getpid());
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
         if (pSharedData->container.block) {NEXUS_MemoryBlock_UnlockOffset(pSharedData->container.block);}
      }

      if ((hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) && pSharedData->container.block) {
         NEXUS_MemoryBlock_Unlock(pSharedData->container.block);
         if (gralloc_boom_check()) {
            NEXUS_MemoryBlock_CheckIfLocked(pSharedData->container.block);
         }
      }
   }

   if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   if (gralloc_boom_check()) {
      NEXUS_MemoryBlock_CheckIfLocked(block_handle);
   }
   /* prevent use after un-registration. */
   hnd->magic = 0;
   return 0;
}

int gralloc_lock_ycbcr(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        struct android_ycbcr *ycbcr)
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
   private_handle_t::get_block_handles(hnd, &shared_block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (lrc || pSharedData == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(shared_block_handle);
      ALOGE("%s : invalid private buffer %p, freed?", __FUNCTION__, shared_block_handle);
      return -EINVAL;
   }
   // native HAL_PIXEL_FORMAT_YCbCr_420_888 flex-yuv buffer and native HAL_PIXEL_FORMAT_YV12
   // multimedia only can be locked using the lock_ycbcr interface.  they are morevover the
   // same internal format in our integration.
   if (!((pSharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888) ||
         (pSharedData->container.format == HAL_PIXEL_FORMAT_YV12))) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(shared_block_handle);
      ALOGE("%s : invalid call for NON flex-YUV buffer (0x%x)", __FUNCTION__, pSharedData->container.format);
      return -EINVAL;
   }

   memset(ycbcr, 0, sizeof(struct android_ycbcr));
   ycbcr->ystride = pSharedData->container.stride;
   ycbcr->cstride = (pSharedData->container.stride/2 + (hnd->alignment-1)) & ~(hnd->alignment-1);
   ycbcr->chroma_step = 1;

   block_handle = pSharedData->container.block;
   if (block_handle) {
      NEXUS_MemoryBlock_Lock(block_handle, &ycbcr->y);
      hnd->nxSurfaceAddress = (uint64_t)&ycbcr->y;
      if (hnd->mgmt_mode != GR_MGMT_MODE_LOCKED) {
         NEXUS_Addr physAddr;
         NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
         hnd->nxSurfacePhysicalAddress = (uint64_t)physAddr;
      }
      ycbcr->cr = (void *) ((uint8_t *)ycbcr->y + (pSharedData->container.stride * pSharedData->container.height));
      ycbcr->cb = (void *) ((uint8_t *)ycbcr->cr +
                            ((pSharedData->container.height/2) *
                             ((pSharedData->container.stride/2 + (hnd->alignment-1)) & ~(hnd->alignment-1))));
   } else {
      ALOGE("no default plane on s-blk:%p", shared_block_handle);
   }

   if (pSharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
      // locking a flexible YUV means we are doing mainly SW decode since our
      // HW decoder reports YV12 for native format.
   } else if (pSharedData->container.format == HAL_PIXEL_FORMAT_YV12) {
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
      }
      private_handle_t::unlock_video_frame(hnd);
      pthread_mutex_unlock(gralloc_g2d_lock());
   }

out_video_failed:
   if ((usage & (GRALLOC_USAGE_SW_READ_MASK|GRALLOC_USAGE_SW_WRITE_MASK)) && !hwConverted) {
      NEXUS_FlushCache(ycbcr->y, pSharedData->container.allocSize);
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr sPhysAddr, pPhysAddr;
      NEXUS_MemoryBlock_LockOffset(shared_block_handle, &sPhysAddr);
      NEXUS_MemoryBlock_LockOffset(block_handle, &pPhysAddr);
      ALOGI(" lock_ycbcr (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x::vaddr:%p::act:%d",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
            hnd,
            hnd->pid,
            shared_block_handle,
            sPhysAddr,
            pSharedData->container.block,
            pPhysAddr,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            pSharedData->container.format,
            ycbcr->y,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(shared_block_handle);
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
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
   if (err != 0) {
      memset(ycbcr, 0, sizeof(struct android_ycbcr));
   }
   return err;
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
   private_handle_t::get_block_handles(hnd, &shared_block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (lrc || pSharedData == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(shared_block_handle);
      ALOGE("%s : invalid private buffer %p, freed?", __FUNCTION__, shared_block_handle);
      return -EINVAL;
   }
   if (pSharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(shared_block_handle);
      ALOGE("%s : invalid call for HAL_PIXEL_FORMAT_YCbCr_420_888 buffer", __FUNCTION__);
      return -EINVAL;
   }
   block_handle = pSharedData->container.block;
   if (block_handle) {
      NEXUS_MemoryBlock_Lock(block_handle, vaddr);
      hnd->nxSurfaceAddress = (uint64_t)vaddr;
      if (hnd->mgmt_mode != GR_MGMT_MODE_LOCKED) {
         NEXUS_Addr physAddr;
         NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
         hnd->nxSurfacePhysicalAddress = (uint64_t)physAddr;
      }
   } else {
      ALOGE("no default plane on s-blk:%p", shared_block_handle);
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
      NEXUS_Addr sPhysAddr, pPhysAddr;
      NEXUS_MemoryBlock_LockOffset(shared_block_handle, &sPhysAddr);
      NEXUS_MemoryBlock_LockOffset(block_handle, &pPhysAddr);
      ALOGI(" lock (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x::vaddr:%p::act:%d",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
            hnd,
            hnd->pid,
            shared_block_handle,
            sPhysAddr,
            pSharedData->container.block,
            pPhysAddr,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            pSharedData->container.format,
            *vaddr,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(shared_block_handle);
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
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
   private_handle_t::get_block_handles(hnd, &shared_block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(shared_block_handle, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(shared_block_handle);
   pSharedData = (PSHARED_DATA) pMemory;
   block_handle = pSharedData->container.block;

   if (hnd->usage & GRALLOC_USAGE_SW_WRITE_MASK) {
      void *vaddr;
      if (!block_handle) {
         if (!lrc) NEXUS_MemoryBlock_Unlock(shared_block_handle);
         return -EINVAL;
      }

      NEXUS_MemoryBlock_Lock(block_handle, &vaddr);
      NEXUS_FlushCache(vaddr, pSharedData->container.allocSize);
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr sPhysAddr, pPhysAddr;
      NEXUS_MemoryBlock_LockOffset(shared_block_handle, &sPhysAddr);
      NEXUS_MemoryBlock_LockOffset(block_handle, &pPhysAddr);
      ALOGI("ulock (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x::act:%d",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
            hnd,
            hnd->pid,
            shared_block_handle,
            sPhysAddr,
            pSharedData->container.block,
            pPhysAddr,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            pSharedData->container.format,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(shared_block_handle);
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
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
