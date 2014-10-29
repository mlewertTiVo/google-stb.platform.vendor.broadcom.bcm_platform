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
#include "NexusSurface.h"

/* desktop Linux needs a little help with gettid() */
#if defined(ARCH_X86) && !defined(HAVE_ANDROID_OS)
#define __KERNEL__
# include <linux/unistd.h>
pid_t gettid() { return syscall(__NR_gettid);}
#undef __KERNEL__
#endif


#include "nexus_base_mmap.h"
// add the C code to convert to TFormat
#include "EGL/egl.h"
#include "converter.h"
#include "converter_cr.c"

extern
NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp);

#define NULL_LIST_SIZE 27

static void JobCallbackHandler(void *context, int param)
{
   NEXUS_Graphicsv3dNotification nNot;
   private_handle_t* hnd = (private_handle_t*)context;
   NEXUS_Graphicsv3d_GetNotification((NEXUS_Graphicsv3dHandle)hnd->lockHnd, &nNot);
   BKNI_SetEvent((BKNI_EventHandle)hnd->lockEvent);
}

static pix_format_e translateConversionFormat(unsigned int format, pix_format_e *ltConverterFormat)
{
   pix_format_e converterFormat;

   switch (format) {
      case BEGL_BufferFormat_eA8B8G8R8_TFormat:   converterFormat = ABGR_8888;   *ltConverterFormat = ABGR_8888_LT;  break;
      case BEGL_BufferFormat_eX8B8G8R8_TFormat:   converterFormat = XBGR_8888;   *ltConverterFormat = XBGR_8888_LT;  break;
      case BEGL_BufferFormat_eR5G6B5_TFormat:     converterFormat = RGB_565;     *ltConverterFormat = RGB_565_LT;  break;
      case BEGL_BufferFormat_eR5G5B5A1_TFormat:   converterFormat = RGBA_5551;   *ltConverterFormat = RGBA_5551_LT;  break;
      case BEGL_BufferFormat_eR4G4B4A4_TFormat:   converterFormat = RGBA_4444;   *ltConverterFormat = RGBA_4444_LT;  break;
      default:                                    converterFormat = ABGR_8888;   *ltConverterFormat = ABGR_8888_LT;  break;
   }

   return converterFormat;
}


static int createHwBacking(private_handle_t* hnd)
{
   NEXUS_Graphicsv3dHandle nexusHandle;
   NEXUS_Graphicsv3dCreateSettings settings;
   BKNI_EventHandle jobDone;
   uint32_t xTiles;
   uint32_t yTiles;
   uint32_t numDirtyTiles;
   uint32_t numBytes;
   int stride;
   int align = 16;

   if (BKNI_CreateEvent(&jobDone) != BERR_SUCCESS)
      goto error0;
   hnd->lockEvent = (void *)jobDone;

   NEXUS_Graphicsv3d_GetDefaultCreateSettings(&settings);
   settings.sJobCallback.callback = JobCallbackHandler;
   settings.sJobCallback.context  = (void *)hnd;
   settings.sJobCallback.param    = 0;
   nexusHandle = NEXUS_Graphicsv3d_Create(&settings);

   if (nexusHandle == NULL)
   {
      LOGE("%s:: Unable to open Graphicsv3d module", __FUNCTION__);
      goto error1;
   }

   // save local copy for use in lock/unlock
   hnd->lockHnd = nexusHandle;

   stride = (hnd->width * hnd->bpp + (align-1)) & ~(align-1);
   // TF converter must have 4k alignment on tiles

   NEXUS_MemoryAllocationSettings allocationSettings;
   NEXUS_Memory_GetDefaultAllocationSettings(&allocationSettings);
   allocationSettings.alignment = 4096;
   allocationSettings.heap = NEXUS_Platform_GetFramebufferHeap(NEXUS_OFFSCREEN_SURFACE);
   NEXUS_Memory_Allocate(hnd->height * stride, &allocationSettings, &hnd->lockTmp);
   if (hnd->lockTmp == NULL)
   {
      LOGE("%s:: Unable to allocate nexus memory for raster backing", __FUNCTION__);
      goto error2;
   }

   // allocate enough space for the CLE
   xTiles = (hnd->width  + 63) / 64;
   yTiles = (hnd->height + 63) / 64;

   numDirtyTiles = xTiles * yTiles;

   numBytes = 11 + (numDirtyTiles * 11);
   if (numBytes < NULL_LIST_SIZE)
      numBytes = NULL_LIST_SIZE;    // Must have at least this many bytes in case we need a NULL list

   NEXUS_Memory_Allocate(numBytes, &allocationSettings, &hnd->lockHWCLE);
   if (hnd->lockHWCLE == NULL)
   {
      LOGE("%s:: Unable to allocate nexus memory for HW CLE list", __FUNCTION__);
      goto error3;
   }

   return 0;

error3:
   NEXUS_Memory_Free(hnd->lockTmp);

error2:
   NEXUS_Graphicsv3d_Destroy(nexusHandle);

error1:
   BKNI_DestroyEvent(jobDone);

error0:
   return -1;
}


// not static, as it may need to be called from gralloc itself, if the buffer was locked
// in the same process space.
void destroyHwBacking(private_handle_t* hnd)
{
   NEXUS_Graphicsv3d_Destroy((NEXUS_Graphicsv3dHandle)hnd->lockHnd);
   BKNI_DestroyEvent((BKNI_EventHandle)hnd->lockEvent);

   NEXUS_Memory_Free(hnd->lockTmp);
   hnd->lockTmp = NULL;

   NEXUS_Memory_Free(hnd->lockHWCLE);
   hnd->lockHWCLE = NULL;
}

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

   if (createHwBacking(hnd) != 0)
      return -ENOMEM;

   // Set timestamp at which the buffer was registered...
   pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);
   pSharedData->videoWindow.timestamp = systemTime();

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

   destroyHwBacking(hnd);

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
      int stride;
      void * src, *dst;
      int align = 16;
      pix_format_e converterFormat, ltConverterFormat;
      private_handle_t* hnd = (private_handle_t*)handle;

      // lock comes from the same process space as the alloc.  This happens in the case of the
      // Chrome browser.  In this instance, the 'register' didnt get called, so no backing exists
      // for the raster version.  Create the stuff here.  It will need to be destroyed in free
      if (hnd->lockTmp == NULL)
         if (createHwBacking(hnd) != 0)
            return -ENOMEM;

      stride = (hnd->width * hnd->bpp + (align - 1)) & ~(align - 1);

      src = NEXUS_OffsetToCachedAddr(hnd->nxSurfacePhysicalAddress);
      dst = hnd->lockTmp;

      converterFormat = translateConversionFormat(hnd->oglFormat, &ltConverterFormat);
      if (hnd->bpp == 16)
      {
         if (hnd->width <= 32 || hnd->height <= 32)
            converterFormat = ltConverterFormat;
      }
      else // 32bpp
      {
         if (hnd->width <= 16 || hnd->height <= 16)
            converterFormat = ltConverterFormat;
      }

      if ((src != NULL) && (dst != NULL))
      {
         NEXUS_FlushCache(dst, hnd->height * stride);
         // Dont want CPU evictions to trample HW converter
         NEXUS_FlushCache(src, hnd->oglSize);

         tfconvert_hw(converterFormat, false,
            (uint8_t *)hnd->lockHWCLE,
            (NEXUS_Graphicsv3dHandle)hnd->lockHnd,
            (uint8_t *)dst,
            hnd->width, stride,
            hnd->height, 0, 0,
            hnd->width, hnd->height,
            (uint8_t *)src, hnd->width,
            hnd->oglStride,
            hnd->height, 0, 0);

         // CPU may have pulled some of the lines in, flush again to make sure the copy is good
         NEXUS_FlushCache(dst, hnd->height * stride);

         BKNI_WaitForEvent((BKNI_EventHandle)(hnd->lockEvent), BKNI_INFINITE);
      }

      /* Only update SW read/write usage flags */
      hnd->usage &= ~(GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK);
      hnd->usage |= usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK);

      LOGV("%s : successfully locked", __FUNCTION__);
      *vaddr = hnd->lockTmp;
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

   pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);
   if (pSharedData == NULL)
   {
      LOGE("%s: !!!! FATAL ERROR Shared Data Not Available in LOCK !!!! FATAL ERROR!!!! FATAL ERROR",__FUNCTION__);
      return -EINVAL;
   }

   if (hnd->lockTmp == NULL || hnd->lockHWCLE == NULL || hnd->lockEvent == NULL || hnd->lockHnd == NULL)
   {
      LOGE("%s: !!!! Conversion cant happen due to resource issues LOCK !!!! FATAL ERROR!!!! FATAL ERROR",__FUNCTION__);
      return -EINVAL;
   }

   // only for offscreen surfaces and only if the buffer has been updated
   if ((hnd->usage & GRALLOC_USAGE_SW_WRITE_MASK) && !(hnd->flags & PRIV_FLAGS_FRAMEBUFFER))
   {
      pix_format_e converterFormat, ltConverterFormat;
      int align = 16;
      int stride = (hnd->width * hnd->bpp + (align-1)) & ~(align-1);
      // source version which was locked
      void * src = hnd->lockTmp; // cached
      void * dst = NEXUS_OffsetToCachedAddr(hnd->nxSurfacePhysicalAddress);

      if ((src != NULL) && (dst != NULL))
      {
         converterFormat = translateConversionFormat(hnd->oglFormat, &ltConverterFormat);
         if (hnd->bpp == 16)
         {
            if (hnd->width <= 32 || hnd->height <= 32)
               converterFormat = ltConverterFormat;
         }
         else /* 32bpp */
         {
            if (hnd->width <= 16 || hnd->height <= 16)
               converterFormat = ltConverterFormat;
         }

         BCM_DEBUG_MSG("%s : calling CONVERTER with params: "
                        "fmt:%x, dst:%p, oglStride:%x, createSettings.width:%x, "
                        "createSettings.height:%x, createSettings.pitch:%x, Src:%p",
                        __FUNCTION__,
                        converterFormat,
                        (uint8_t *)dst,
                        hnd->oglStride,
                        hnd->width,
                        hnd->height,
                        stride,
                        (uint8_t *)src);

         NEXUS_FlushCache(src, hnd->height * stride);
         NEXUS_FlushCache(dst, hnd->oglSize);

         tfconvert_hw(converterFormat, true,
            (uint8_t *)hnd->lockHWCLE,
            (NEXUS_Graphicsv3dHandle)hnd->lockHnd,
            (uint8_t *)dst,
            hnd->width, hnd->oglStride,
            hnd->height, 0, 0,
            hnd->width, hnd->height,
            (uint8_t *)src, hnd->width,
            stride,
            hnd->height, 0, 0);

         BKNI_WaitForEvent((BKNI_EventHandle)(hnd->lockEvent), BKNI_INFINITE);

         LOGV("%s : successfully unlocked", __FUNCTION__);
      }
   }

   return 0;
}

