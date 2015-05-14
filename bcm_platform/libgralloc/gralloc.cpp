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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <dlfcn.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"
#include "nexus_base_mmap.h"
#include "nexus_platform.h"

// default platform layer to render to nexus
#include "EGL/egl.h"

#include "cutils/properties.h"
#include "nx_ashmem.h"

void __attribute__ ((constructor)) gralloc_explicit_load(void);
void __attribute__ ((destructor)) gralloc_explicit_unload(void);

static void (* dyn_BEGLint_BufferGetRequirements)(BEGL_PixmapInfo *, BEGL_BufferSettings *);
static void * (* dyn_EGL_nexus_join)(char *client_process_name);
static void (* dyn_EGL_nexus_unjoin)(void *nexus_client);
#define LOAD_FN(lib, name) \
if (!(dyn_ ## name = (typeof(dyn_ ## name)) dlsym(lib, #name))) \
   ALOGV("failed resolving '%s'", #name); \
else \
   ALOGV("resolved '%s' to %p", #name, dyn_ ## name);
static void *gl_dyn_lib;
static void *nexus_client = NULL;
static int gralloc_with_mma = 0;
static int gralloc_default_align = 0;
static int gralloc_log_map = 0;
static int gralloc_conv_time = 0;

static pthread_mutex_t moduleLock = PTHREAD_MUTEX_INITIALIZER;
static NEXUS_Graphics2DHandle hGraphics = NULL;
static BKNI_EventHandle hCheckpointEvent = NULL;

#define DATA_PLANE_MAX_WIDTH    1920
#define DATA_PLANE_MAX_HEIGHT   1200

/* default alignment for gralloc buffers:
 *
 *      vc4 - 16 bytes alignment on surfaces, 4K alignment on textures (max).
 *      vc5 - 256/512 bytes alignment on textures (min).
 */
#define GRALLOC_MAX_BUFFER_ALIGNED  4096
#define GRALLOC_MIN_BUFFER_ALIGNED  256

#define NX_MMA                  "ro.nx.mma"
#define NX_GR_LOG_MAP           "ro.gr.log.map"
#define NX_GR_CONV_TIME         "ro.gr.conv.time"

#define NEXUS_JOIN_CLIENT_PROCESS "gralloc"
static void gralloc_load_lib(void)
{
   char value[PROPERTY_VALUE_MAX];
   const char *gl_dyn_lib_path = "/system/lib/egl/libGLES_nexus.so";
   gl_dyn_lib = dlopen(gl_dyn_lib_path, RTLD_LAZY | RTLD_LOCAL);
   if (!gl_dyn_lib) {
      // try legacy path as well.
      const char *gl_dyn_lib_path = "/vendor/lib/egl/libGLES_nexus.so";
      gl_dyn_lib = dlopen(gl_dyn_lib_path, RTLD_LAZY | RTLD_LOCAL);
      if (!gl_dyn_lib) {
         ALOGE("failed loading essential GLES library '%s': <%s>!", gl_dyn_lib_path, dlerror());
      }
   }

   // load wanted functions from the library now.
   LOAD_FN(gl_dyn_lib, BEGLint_BufferGetRequirements);
   LOAD_FN(gl_dyn_lib, EGL_nexus_join);
   LOAD_FN(gl_dyn_lib, EGL_nexus_unjoin);

   if (dyn_EGL_nexus_join) {
      nexus_client = dyn_EGL_nexus_join((char *)NEXUS_JOIN_CLIENT_PROCESS);
      if (nexus_client == NULL) {
         ALOGE("%s: failed joining nexus client '%s'!", __FUNCTION__, NEXUS_JOIN_CLIENT_PROCESS);
      } else {
         ALOGV("%s: joined nexus client '%s'!", __FUNCTION__, NEXUS_JOIN_CLIENT_PROCESS);
      }
   } else {
      ALOGE("%s: dyn_EGL_nexus_join unavailable, something will break!", __FUNCTION__);
   }

   if (property_get(NX_MMA, value, "0")) {
      gralloc_with_mma = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   if (property_get(NX_GR_LOG_MAP, value, "0")) {
      gralloc_log_map = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   if (property_get(NX_GR_CONV_TIME, value, "0")) {
      gralloc_conv_time = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   gralloc_default_align = GRALLOC_MAX_BUFFER_ALIGNED;
   if (!dyn_BEGLint_BufferGetRequirements) {
      gralloc_default_align = GRALLOC_MIN_BUFFER_ALIGNED;
   }
}

#undef LOAD_FN

static void gralloc_checkpoint_callback(void *pParam, int param)
{
    (void)param;
    BKNI_SetEvent((BKNI_EventHandle)pParam);
}

void gralloc_explicit_load(void)
{
   NEXUS_Error rc;
   NEXUS_Graphics2DOpenSettings g2dOpenSettings;

   gralloc_load_lib();

   rc = BKNI_CreateEvent(&hCheckpointEvent);
   if (rc) {
      ALOGE("Unable to create checkpoint event");
      hCheckpointEvent = NULL;
      hGraphics = NULL;
   } else {
      NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOpenSettings);
      g2dOpenSettings.compatibleWithSurfaceCompaction = false;
      hGraphics = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOpenSettings);
      if (!hGraphics) {
         ALOGW("Unable to open Graphics2D.  HW/SW access format conversions will fail...");
      } else {
         NEXUS_Graphics2DSettings gfxSettings;
         NEXUS_Graphics2D_GetSettings(hGraphics, &gfxSettings);
         gfxSettings.pollingCheckpoint = false;
         gfxSettings.checkpointCallback.callback = gralloc_checkpoint_callback;
         gfxSettings.checkpointCallback.context = (void *)hCheckpointEvent;
         rc = NEXUS_Graphics2D_SetSettings(hGraphics, &gfxSettings);
         if ( rc ) {
            ALOGW("Unable to set Graphics2D Settings");
            NEXUS_Graphics2D_Close(hGraphics);
            hGraphics = NULL;
         }
      }
   }
}

void gralloc_explicit_unload(void)
{
   if (nexus_client && dyn_EGL_nexus_unjoin) {
      dyn_EGL_nexus_unjoin(nexus_client);
      nexus_client = NULL;
   }

   dlclose(gl_dyn_lib);

   if (hGraphics) {
      NEXUS_Graphics2D_Close(hGraphics);
      hGraphics = NULL;
   }

   if (hCheckpointEvent) {
      BKNI_DestroyEvent(hCheckpointEvent);
      hCheckpointEvent = NULL;
   }
}

int gralloc_log_mapper(void)
{
   return gralloc_log_map;
}

int gralloc_timestamp_conversion(void)
{
   return gralloc_conv_time;
}

void * gralloc_v3d_get_nexus_client_context(void)
{
   return nexus_client;
}

pthread_mutex_t *gralloc_g2d_lock(void)
{
   return &moduleLock;
}

NEXUS_Graphics2DHandle gralloc_g2d_hdl(void)
{
   return hGraphics;
}

BKNI_EventHandle gralloc_g2d_evt(void)
{
   return hCheckpointEvent;
}

static void gralloc_bzero(int is_mma, unsigned handle, size_t numBytes)
{
    NEXUS_Graphics2DHandle gfx = gralloc_g2d_hdl();
    BKNI_EventHandle event = gralloc_g2d_evt();
    pthread_mutex_t *pMutex = gralloc_g2d_lock();
    bool done=false;
    NEXUS_Addr physAddr;
    void *pMemory = NULL;
    NEXUS_MemoryBlockHandle block_handle = NULL;

    if (is_mma) {
       block_handle = (NEXUS_MemoryBlockHandle) handle;
       NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
       NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
    } else {
       pMemory = NEXUS_OffsetToCachedAddr((NEXUS_Addr)handle);
    }

    if (gfx && event && pMutex) {
        NEXUS_Error errCode;
        size_t roundedSize = (numBytes + 0xFFF) & ~0xFFF;

        if (is_mma) {
           if (pMemory == NULL) {
              goto out_release;
           }
           // TODO: using m2m.
        } else {
           if (pMemory == NULL) {
              return;
           }
           NEXUS_FlushCache(pMemory, roundedSize);
           pthread_mutex_lock(pMutex);
           errCode = NEXUS_Graphics2D_Memset32(gfx, pMemory, 0, roundedSize/4);
           if (0 == errCode) {
               errCode = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
               if (errCode == NEXUS_GRAPHICS2D_QUEUED) {
                   errCode = BKNI_WaitForEvent(event, CHECKPOINT_TIMEOUT);
                   if (errCode) {
                      ALOGW("Timeout zeroing gralloc buffer");
                   }
               }
               if (!errCode) {
                  done = true;
               }
           }
           pthread_mutex_unlock(pMutex);
        }
    }

    if (!done) {
       bzero(pMemory, numBytes);
    } else if (!is_mma) {
       NEXUS_FlushCache(pMemory, numBytes);
    }

out_release:
    if (is_mma && block_handle) {
       NEXUS_MemoryBlock_UnlockOffset(block_handle);
       NEXUS_MemoryBlock_Unlock(block_handle);
    }
}

/*****************************************************************************/

struct gralloc_context_t {
    alloc_device_t  device;
};

static int
gralloc_alloc_buffer(alloc_device_t* dev,int w, int h,
       int format, int usage, buffer_handle_t* pHandle,int *pStride);
/*****************************************************************************/

int fb_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

static int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device);

extern int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        void** vaddr);

extern int gralloc_unlock(gralloc_module_t const* module,
        buffer_handle_t handle);

extern int gralloc_register_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

extern int gralloc_unregister_buffer(gralloc_module_t const* module,
        buffer_handle_t handle);

NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp);

/*****************************************************************************/

static struct hw_module_methods_t gralloc_module_methods = {
        open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM = {
   base: {
      common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: GRALLOC_HARDWARE_MODULE_ID,
         name: "Graphics Memory Allocator Module",
         author: "The Android Open Source Project",
         methods: &gralloc_module_methods,
         dso: NULL,
         reserved: {0}
      },
      registerBuffer: gralloc_register_buffer,
      unregisterBuffer: gralloc_unregister_buffer,
      lock: gralloc_lock,
      unlock: gralloc_unlock,
      perform: NULL,
      lock_ycbcr: NULL,
      lockAsync: NULL,
      unlockAsync: NULL,
      lockAsync_ycbcr: NULL,
      reserved_proc: {0, 0, 0}
   },
};

/*****************************************************************************/

NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt,
                                      int *bpp)
{
   NEXUS_PixelFormat pf;
   int b;
   switch (pixelFmt) {
      case HAL_PIXEL_FORMAT_RGBA_8888:    b = 4;   pf = NEXUS_PixelFormat_eA8_B8_G8_R8;   break;
      case HAL_PIXEL_FORMAT_RGBX_8888:    b = 4;   pf = NEXUS_PixelFormat_eX8_R8_G8_B8;   break;
      case HAL_PIXEL_FORMAT_RGB_888:      b = 4;   pf = NEXUS_PixelFormat_eX8_R8_G8_B8;   break;
      case HAL_PIXEL_FORMAT_RGB_565:      b = 2;   pf = NEXUS_PixelFormat_eR5_G6_B5;      break;
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                                          b = 4;   pf = NEXUS_PixelFormat_eX8_R8_G8_B8;   break;
      // yv12 planar - nexus does not support this natively, instead we return the expected
      //               converted to packed format that eventually we would produce.
      case HAL_PIXEL_FORMAT_YV12:         b = 2;   pf = NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8; break;
      default:                            b = 0;   pf = NEXUS_PixelFormat_eUnknown;
                                          ALOGE("%s %d FORMAT [ %d ] NOT SUPPORTED ",__FUNCTION__,__LINE__,pixelFmt); break;
   }

   *bpp = b;
   return pf;
}

static
unsigned int allocGLSuitableBuffer(private_handle_t * allocContext,
                                   int fd,
                                   int width,
                                   int height,
                                   int bpp,
                                   int format)
{
   BEGL_PixmapInfo bufferRequirements;
   BEGL_BufferSettings bufferConstrainedRequirements;
   unsigned int phyAddr = 0;
   struct nx_ashmem_alloc ashmem_alloc;

   if (!allocContext) {
      return -EINVAL;
   }

   bufferRequirements.width = width;
   bufferRequirements.height = height;

   // WARNING! GL only has three renderable surfaces, premote 888 to X888
   switch (format)
   {
   case HAL_PIXEL_FORMAT_RGBA_8888:  bufferRequirements.format = BEGL_BufferFormat_eA8B8G8R8;     break;
   case HAL_PIXEL_FORMAT_RGBX_8888:  bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8;     break;
   case HAL_PIXEL_FORMAT_RGB_888:    bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8;     break;
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                                     bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8;     break;
   case HAL_PIXEL_FORMAT_RGB_565:    bufferRequirements.format = BEGL_BufferFormat_eR5G6B5;       break;

   default:                          bufferRequirements.format = BEGL_BufferFormat_INVALID;       break;
   }

   //
   // this surface may not necessarily be used by GL,
   // so it can still succeed, it will however fail at eglCreateImageKHR()
   //
   if (bufferRequirements.format != BEGL_BufferFormat_INVALID) {
      int ret;
      // We need to call the V3D driver buffer get requirements function in order to
      // get the parameters of the shadow buffer that we are creating.
      if (gralloc_v3d_get_nexus_client_context() == NULL) {
         ALOGE("%s: no valid client context...", __FUNCTION__);
      }

      // VC4 requires memory with a set stride.
      if (dyn_BEGLint_BufferGetRequirements)
         dyn_BEGLint_BufferGetRequirements(&bufferRequirements, &bufferConstrainedRequirements);
      else {
         // VC5, can use any stride, so just calculate the values (dont need to call the function)
         // push into VC4 return header to simplify the build/code paths
         bufferConstrainedRequirements.pitchBytes = width * bpp;
         bufferConstrainedRequirements.totalByteSize = height * bufferConstrainedRequirements.pitchBytes;
      }

      ashmem_alloc.size = bufferConstrainedRequirements.totalByteSize;
      ashmem_alloc.align = gralloc_default_align;
      ret = ioctl(fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret < 0) {
         return 0;
      };

      phyAddr = (NEXUS_Addr)ioctl(fd, NX_ASHMEM_GETMEM);

      allocContext->oglStride = bufferConstrainedRequirements.pitchBytes;
      allocContext->oglFormat = bufferConstrainedRequirements.format;
      allocContext->oglSize   = bufferConstrainedRequirements.totalByteSize;
   }
   else {
      allocContext->oglStride = 0;
      allocContext->oglFormat = 0;
      allocContext->oglSize   = 0;
   }

   return phyAddr;
}

static void getBufferDataFromFormat(int *alignment, int w, int h, int bpp, int format, int *pStride, int *size, int *extra_size)
{
   /* note: alignment - as 'input' provides an indication of what the platform expects,
            may be changed as 'output' based on the format requirements.
   */

   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      case HAL_PIXEL_FORMAT_RGB_565:
      {
          int bpr = (w*bpp + (*alignment-1)) & ~(*alignment-1);
          *size = bpr * h;
          *pStride = bpr / bpp;
          *extra_size = 0;
      }
      break;

      case HAL_PIXEL_FORMAT_YV12:
      {
          // force alignment according to (android) format definition.
          *alignment = 16;
          // use y-stride: ALIGN(w, 16)
          *pStride = (w + (*alignment-1)) & ~(*alignment-1);
          // size: y-stride * h + 2 * (c-stride * h/2), with c-stride: ALIGN(y-stride/2, 16)
          *size = (*pStride * h) + 2 * ((h/2) * ((*pStride/2 + (*alignment-1)) & ~(*alignment-1)));
          // extra_size: stride * height * bpp
          *extra_size = *pStride * bpp * h;
      }
      break;

      default:
          *pStride = 0;
          *size = 0;
          *extra_size = 0;
      break;
   }
}

static int
gralloc_alloc_buffer(alloc_device_t* dev,
                    int w, int h,
                    int format,
                    int usage,
                    buffer_handle_t* pHandle,
                    int *pStride)
{
   int bpp = 0, fd = -1, fd2 = -1, fd3 = -1, fd4 = -1;
   int size, extra_size, fmt_align;
   NEXUS_PixelFormat nxFormat = getNexusPixelFormat(format, &bpp);
   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   void *pMemory;
   bool needs_yv12 = false;
   bool needs_ycrcb = false;
   bool needs_rgb = false;
   struct nx_ashmem_alloc ashmem_alloc;

   (void)dev;

   if (nxFormat == NEXUS_PixelFormat_eUnknown) {
      ALOGE("%s: unsupported gr->nx format: %d", __FUNCTION__, format);
      return -EINVAL;
   }

   property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
   strcpy(value2, "/dev/");
   strcat(value2, value);

   fd = open(value2, O_RDWR, 0);
   if ((fd == -1) || (!fd)) {
      return -EINVAL;
   }
   fd2 = open(value2, O_RDWR, 0);
   if ((fd2 == -1) || (!fd2)) {
      return -EINVAL;
   }
   fd3 = open(value2, O_RDWR, 0);
   if ((fd3 == -1) || (!fd3)) {
      return -EINVAL;
   }
   fd4 = open(value2, O_RDWR, 0);
   if ((fd4 == -1) || (!fd4)) {
      return -EINVAL;
   }

   private_handle_t *grallocPrivateHandle = new private_handle_t(fd, fd2, fd3, fd4, 0);
   if (grallocPrivateHandle == NULL) {
      *pHandle = NULL;
      return -ENOMEM;
   }

   grallocPrivateHandle->is_mma = gralloc_with_mma;
   grallocPrivateHandle->alignment = 1;
   if (dyn_BEGLint_BufferGetRequirements) {
      grallocPrivateHandle->alignment = 16;
   }

   fmt_align = grallocPrivateHandle->alignment;
   getBufferDataFromFormat(&fmt_align, w, h, bpp, format, pStride, &size, &extra_size);
   if (fmt_align != grallocPrivateHandle->alignment) {
      grallocPrivateHandle->alignment = fmt_align;
   }

   ashmem_alloc.size = sizeof(SHARED_DATA);
   ashmem_alloc.align = GRALLOC_MAX_BUFFER_ALIGNED;
   int ret = ioctl(grallocPrivateHandle->fd2, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
   if (ret < 0) {
      return -ENOMEM;
   };

   grallocPrivateHandle->sharedData = (NEXUS_Addr)ioctl(fd2, NX_ASHMEM_GETMEM);
   if (grallocPrivateHandle->sharedData == 0) {
      *pHandle = NULL;
      delete grallocPrivateHandle;
      return -ENOMEM;
   }

   if (grallocPrivateHandle->is_mma) {
      pMemory = NULL;
      block_handle = (NEXUS_MemoryBlockHandle)grallocPrivateHandle->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(grallocPrivateHandle->sharedData);
   }

   if (pSharedData == NULL) {
      /* that's pretty bad...  failed to map the allocated memory! */
      *pHandle = NULL;
      delete grallocPrivateHandle;
      return -ENOMEM;
   }

   memset(pSharedData, 0, sizeof(SHARED_DATA));

   grallocPrivateHandle->usage = usage;
   pSharedData->planes[DEFAULT_PLANE].width     = w;
   pSharedData->planes[DEFAULT_PLANE].height    = h;
   pSharedData->planes[DEFAULT_PLANE].bpp       = bpp;
   pSharedData->planes[DEFAULT_PLANE].format    = format;
   pSharedData->planes[DEFAULT_PLANE].size      = size;
   pSharedData->planes[DEFAULT_PLANE].allocSize = size;
   pSharedData->planes[DEFAULT_PLANE].stride    = *pStride;

   if (format != HAL_PIXEL_FORMAT_YV12) {
      // standard graphic buffer.
      pSharedData->planes[DEFAULT_PLANE].physAddr =
         allocGLSuitableBuffer(grallocPrivateHandle, grallocPrivateHandle->fd, w, h, bpp, format);
      pSharedData->planes[DEFAULT_PLANE].allocSize = grallocPrivateHandle->oglSize;
      pSharedData->planes[DEFAULT_PLANE].stride = grallocPrivateHandle->oglStride;

      if (grallocPrivateHandle->is_mma) {
         NEXUS_Addr physAddr;
         NEXUS_MemoryBlock_LockOffset((NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr, &physAddr);
         grallocPrivateHandle->nxSurfacePhysicalAddress = (unsigned)physAddr;
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr, &pMemory);
         grallocPrivateHandle->nxSurfaceAddress = (unsigned)pMemory;
      } else {
         grallocPrivateHandle->nxSurfacePhysicalAddress = pSharedData->planes[DEFAULT_PLANE].physAddr;
         grallocPrivateHandle->nxSurfaceAddress = (unsigned)NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
      }

      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr;
         unsigned sharedPhysAddr = grallocPrivateHandle->sharedData;
         if (grallocPrivateHandle->is_mma) {
            NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
            sharedPhysAddr = (unsigned)physAddr;
         }
         ALOGI("alloc (ST): mma:%d::owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::sz:%d::mapped:0x%x",
               grallocPrivateHandle->is_mma,
               getpid(),
               grallocPrivateHandle->sharedData,
               sharedPhysAddr,
               pSharedData->planes[DEFAULT_PLANE].physAddr,
               grallocPrivateHandle->nxSurfacePhysicalAddress,
               pSharedData->planes[DEFAULT_PLANE].size,
               grallocPrivateHandle->nxSurfaceAddress);
         if (grallocPrivateHandle->is_mma) {
            NEXUS_MemoryBlock_UnlockOffset(block_handle);
         }
      }

   } else if ((format == HAL_PIXEL_FORMAT_YV12) && !(usage & GRALLOC_USAGE_PRIVATE_0)) {
      // standard yv12 buffer for multimedia, need also a secondary buffer for
      // planar to packed conversion when going through the hwc/nsc.
      needs_yv12 = true;
      if ((usage & GRALLOC_USAGE_HW_COMPOSER) &&
          (usage & GRALLOC_USAGE_SW_WRITE_MASK) &&
          w <= DATA_PLANE_MAX_WIDTH &&
          h <= DATA_PLANE_MAX_HEIGHT) {
         needs_ycrcb = true;
      }
   } else if ((format == HAL_PIXEL_FORMAT_YV12) && (usage & GRALLOC_USAGE_PRIVATE_0) & (usage & GRALLOC_USAGE_SW_READ_OFTEN)) {
      // private multimedia buffer, we only need a yv12 plane in case cpu is intending to read
      // the content, eg decode->encode type of scenario; yv12 data is produced during lock.
      needs_yv12 = true;
   }

   if (needs_yv12) {
      ashmem_alloc.size = size;
      ashmem_alloc.align = gralloc_default_align;
      ret = ioctl(grallocPrivateHandle->fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret >= 0) {
         pSharedData->planes[DEFAULT_PLANE].physAddr =
             (NEXUS_Addr)ioctl(grallocPrivateHandle->fd, NX_ASHMEM_GETMEM);
      }

      if ((usage & GRALLOC_USAGE_HW_TEXTURE)) {
         // 1) vc5 suports 4k texture and does not require special
         //    alignment considerations.
         // 2) vc4 does only support 2k textures and does require
         //    special alignmnent considerations.
         if (/*!dyn_BEGLint_BufferGetRequirements ||
             (dyn_BEGLint_BufferGetRequirements &&*/
             (w <= DATA_PLANE_MAX_WIDTH &&
              h <= DATA_PLANE_MAX_HEIGHT)) {
            needs_rgb = true;
         }
      }
   }

   if (needs_ycrcb) {
      pSharedData->planes[EXTRA_PLANE].width     = w;
      pSharedData->planes[EXTRA_PLANE].height    = h;
      pSharedData->planes[EXTRA_PLANE].bpp       = bpp;
      pSharedData->planes[EXTRA_PLANE].format    = (int)nxFormat;
      pSharedData->planes[EXTRA_PLANE].size      = extra_size;
      pSharedData->planes[EXTRA_PLANE].allocSize = extra_size;
      pSharedData->planes[EXTRA_PLANE].stride    = bpp * *pStride;
      ashmem_alloc.size = extra_size;
      ashmem_alloc.align = gralloc_default_align;
      ret = ioctl(grallocPrivateHandle->fd3, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret >= 0) {
         pSharedData->planes[EXTRA_PLANE].physAddr =
             (NEXUS_Addr)ioctl(grallocPrivateHandle->fd3, NX_ASHMEM_GETMEM);
      }
   }

   if (needs_rgb) {
      // Create a RGB plane for GL texture as Khronos does not support YUV texturing.
      pSharedData->planes[GL_PLANE].physAddr =
         allocGLSuitableBuffer(grallocPrivateHandle, grallocPrivateHandle->fd4, w, h, 4, HAL_PIXEL_FORMAT_RGBA_8888);
      pSharedData->planes[GL_PLANE].width     = w;
      pSharedData->planes[GL_PLANE].height    = h;
      pSharedData->planes[GL_PLANE].bpp       = 4;
      pSharedData->planes[GL_PLANE].format    = (int)NEXUS_PixelFormat_eA8_B8_G8_R8;
      pSharedData->planes[GL_PLANE].size      = size;
      pSharedData->planes[GL_PLANE].allocSize = grallocPrivateHandle->oglSize;
      pSharedData->planes[GL_PLANE].stride    = grallocPrivateHandle->oglStride;

      if (grallocPrivateHandle->is_mma) {
         NEXUS_Addr physAddr;
         NEXUS_MemoryBlock_LockOffset((NEXUS_MemoryBlockHandle)pSharedData->planes[GL_PLANE].physAddr, &physAddr);
         grallocPrivateHandle->nxSurfacePhysicalAddress = (unsigned)physAddr;
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlockHandle)pSharedData->planes[GL_PLANE].physAddr, &pMemory);
         grallocPrivateHandle->nxSurfaceAddress = (unsigned)pMemory;
      } else {
         grallocPrivateHandle->nxSurfacePhysicalAddress = pSharedData->planes[GL_PLANE].physAddr;
         grallocPrivateHandle->nxSurfaceAddress = (unsigned)NEXUS_OffsetToCachedAddr(pSharedData->planes[GL_PLANE].physAddr);
      }

      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr;
         unsigned sharedPhysAddr = grallocPrivateHandle->sharedData;
         if (grallocPrivateHandle->is_mma) {
            NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
            sharedPhysAddr = (unsigned)physAddr;
         }
         ALOGI("alloc (GL): mma:%d::owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::sz:%d::mapped:0x%x",
               grallocPrivateHandle->is_mma,
               getpid(),
               grallocPrivateHandle->sharedData,
               sharedPhysAddr,
               pSharedData->planes[GL_PLANE].physAddr,
               grallocPrivateHandle->nxSurfacePhysicalAddress,
               pSharedData->planes[GL_PLANE].size,
               grallocPrivateHandle->nxSurfaceAddress);
         if (grallocPrivateHandle->is_mma) {
            NEXUS_MemoryBlock_UnlockOffset(block_handle);
         }
      }
   }

   bool alloc_failed = false;
   if (needs_yv12 && (pSharedData->planes[DEFAULT_PLANE].physAddr == 0)) {
      ALOGE("%s: failed to allocate default yv12 plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      alloc_failed = true;
   }
   if (needs_ycrcb && (pSharedData->planes[EXTRA_PLANE].physAddr == 0)) {
      ALOGE("%s: failed to allocate extra ycrcb plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      alloc_failed = true;
   }
   if (!needs_yv12 && ((!grallocPrivateHandle->is_mma && pSharedData->planes[DEFAULT_PLANE].physAddr == 0) ||
       (grallocPrivateHandle->is_mma && grallocPrivateHandle->nxSurfacePhysicalAddress == 0))) {
      ALOGE("%s: failed to allocate standard plane (%d,%d), size %d", __FUNCTION__, w, h, extra_size);
      alloc_failed = true;
   }
   if (needs_rgb && ((!grallocPrivateHandle->is_mma && pSharedData->planes[GL_PLANE].physAddr == 0) ||
       (grallocPrivateHandle->is_mma && grallocPrivateHandle->nxSurfacePhysicalAddress == 0))) {
      ALOGE("%s: failed to allocate gl plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      alloc_failed = true;
   }

   if (alloc_failed) {
      *pHandle = NULL;
      if (grallocPrivateHandle->is_mma && block_handle) {
         NEXUS_MemoryBlock_Unlock(block_handle);
      }
      delete grallocPrivateHandle;
      close(fd);
      close(fd2);
      close(fd3);
      close(fd4);
      return -ENOMEM;
   } else {
      if (pSharedData->planes[DEFAULT_PLANE].physAddr) {
          gralloc_bzero(grallocPrivateHandle->is_mma, pSharedData->planes[DEFAULT_PLANE].physAddr, pSharedData->planes[DEFAULT_PLANE].allocSize);
      }
      if (pSharedData->planes[EXTRA_PLANE].physAddr) {
          gralloc_bzero(grallocPrivateHandle->is_mma, pSharedData->planes[EXTRA_PLANE].physAddr, pSharedData->planes[EXTRA_PLANE].allocSize);
      }
      if (pSharedData->planes[GL_PLANE].physAddr) {
          gralloc_bzero(grallocPrivateHandle->is_mma, pSharedData->planes[GL_PLANE].physAddr, pSharedData->planes[GL_PLANE].allocSize);
      }
   }


   *pHandle = grallocPrivateHandle;

   if (grallocPrivateHandle->is_mma && block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

   return 0;
}

static int
gralloc_free_buffer(alloc_device_t* dev, private_handle_t *hnd)
{
   (void)dev;

   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;

   if (!hnd) {
      return -EINVAL;
   }

   if (hnd->is_mma) {
      void *pMemory;
      block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedData);
   }

   if (pSharedData) {
      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr;
         unsigned sharedPhysAddr = hnd->sharedData;
         int gl_plane = (pSharedData->planes[GL_PLANE].physAddr ? 1 : 0);
         unsigned planePhysAddr =
            (pSharedData->planes[GL_PLANE].physAddr ? pSharedData->planes[GL_PLANE].physAddr : pSharedData->planes[DEFAULT_PLANE].physAddr);
         unsigned planePhysSize =
            (pSharedData->planes[GL_PLANE].physAddr ? pSharedData->planes[GL_PLANE].size : pSharedData->planes[DEFAULT_PLANE].size);
         if (hnd->is_mma) {
            NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
            sharedPhysAddr = (unsigned)physAddr;
         }
         ALOGI(" free (%s): mma:%d::owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::sz:%d::mapped:0x%x",
               gl_plane ? "GL" : "ST",
               hnd->is_mma,
               hnd->pid,
               hnd->sharedData,
               sharedPhysAddr,
               planePhysAddr,
               hnd->nxSurfacePhysicalAddress,
               planePhysSize,
               hnd->nxSurfaceAddress);
         if (hnd->is_mma) {
            NEXUS_MemoryBlock_UnlockOffset(block_handle);
         }
      }

      if (android_atomic_acquire_load(&pSharedData->hwc.active)) {
         ALOGE("Freeing gralloc buffer %#x used by HWC!  layer %d surface %#x",
               hnd->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface);
      }
   }

   if (hnd->is_mma) {
      if (pSharedData) {
         if (pSharedData->planes[GL_PLANE].physAddr) {
            NEXUS_MemoryBlock_UnlockOffset((NEXUS_MemoryBlockHandle)pSharedData->planes[GL_PLANE].physAddr);
            NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlockHandle)pSharedData->planes[GL_PLANE].physAddr);
         }
         if (pSharedData->planes[DEFAULT_PLANE].physAddr) {
            NEXUS_MemoryBlock_UnlockOffset((NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr);
            NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlockHandle)pSharedData->planes[DEFAULT_PLANE].physAddr);
         }
      }
      if (block_handle) {
         NEXUS_MemoryBlock_Unlock(block_handle);
      }
   }

   if (hnd->fd >= 0) {
      close(hnd->fd);
   }
   if (hnd->fd2 >= 0) {
      close(hnd->fd2);
   }
   if (hnd->fd3 >= 0) {
      close(hnd->fd3);
   }
   if (hnd->fd4 >= 0) {
      close(hnd->fd4);
   }
   delete hnd;

   return 0;
}

/*****************************************************************************/

static int gralloc_alloc(alloc_device_t* dev,
        int w, int h, int format, int usage,
        buffer_handle_t* pHandle, int* pStride)
{
   if (!pHandle || !pStride) {
      ALOGE("%s : condition check failed", __FUNCTION__);
         return -EINVAL;
   }

   int err;
   err = gralloc_alloc_buffer(dev, w, h, format, usage, pHandle, pStride);

   if (usage & GRALLOC_USAGE_HW_FB) {
      ALOGI("alloc: fb::w:%d::h:%d::fmt:%d::usage:0x%08x::hdl:%p", w, h, format, usage, pHandle);
   }

   if (err < 0) {
      ALOGE("alloc: FAILED::w:%d::h:%d::fmt:%d::usage:0x%08x", w, h, format, usage);
   }

   return err;
}

static int gralloc_free(alloc_device_t* dev,
        buffer_handle_t handle)
{
   if (private_handle_t::validate(handle) < 0) {
      ALOGE("%s : attempt to free invalid handle?", __FUNCTION__);
      return -EINVAL;
   }

   private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
   gralloc_free_buffer(dev,const_cast<private_handle_t*>(hnd));

   return 0;
}

/*****************************************************************************/

static int gralloc_close(struct hw_device_t *dev)
{
   gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
   if (ctx) {
      free(ctx);
   }
   return 0;
}

int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
   int status = -EINVAL;

   ALOGD("%s: %s (s-data: %d)", __FUNCTION__, name, sizeof(SHARED_DATA));

   if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
      gralloc_context_t *dev;
      dev = (gralloc_context_t*)malloc(sizeof(*dev));
      /* initialize our state here */
      memset(dev, 0, sizeof(*dev));

      /* initialize the procs */
      dev->device.common.tag = HARDWARE_DEVICE_TAG;
      dev->device.common.version = 0;
      dev->device.common.module = const_cast<hw_module_t*>(module);
      dev->device.common.close = gralloc_close;

      dev->device.alloc   = gralloc_alloc;
      dev->device.free    = gralloc_free;

      private_module_t* m = reinterpret_cast<private_module_t*>(dev->device.common.module);

      *device = &dev->device.common;
      status = 0;
   } else {
      status = fb_device_open(module, name, device);
   }

   return status;
}
