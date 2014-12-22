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
   LOGE("failed resolving '%s'", #name); \
else \
   LOGI("resolved '%s' to %p", #name, dyn_ ## name);
static void *gl_dyn_lib;
static void *nexus_client = NULL;

static pthread_mutex_t moduleLock = PTHREAD_MUTEX_INITIALIZER;
static NEXUS_Graphics2DHandle hGraphics = NULL;
static BKNI_EventHandle hCheckpointEvent = NULL;

#define NEXUS_JOIN_CLIENT_PROCESS "gralloc"
static void gralloc_load_lib(void)
{
   const char *gl_dyn_lib_path = "/system/lib/egl/libGLES_nexus.so";
   gl_dyn_lib = dlopen(gl_dyn_lib_path, RTLD_LAZY | RTLD_LOCAL);
   if (!gl_dyn_lib) {
      // try legacy path as well.
      const char *gl_dyn_lib_path = "/vendor/lib/egl/libGLES_nexus.so";
      gl_dyn_lib = dlopen(gl_dyn_lib_path, RTLD_LAZY | RTLD_LOCAL);
      if (!gl_dyn_lib) {
         LOGE("failed loading essential GLES library '%s': <%s>!", gl_dyn_lib_path, dlerror());
      }
   }

   // load wanted functions from the library now.
   LOAD_FN(gl_dyn_lib, BEGLint_BufferGetRequirements);
   LOAD_FN(gl_dyn_lib, EGL_nexus_join);
   LOAD_FN(gl_dyn_lib, EGL_nexus_unjoin);

   if (dyn_EGL_nexus_join) {
      nexus_client = dyn_EGL_nexus_join((char *)NEXUS_JOIN_CLIENT_PROCESS);
      if (nexus_client == NULL) {
         LOGE("%s: failed joining nexus client '%s'!", __FUNCTION__, NEXUS_JOIN_CLIENT_PROCESS);
      } else {
         LOGI("%s: joined nexus client '%s'!", __FUNCTION__, NEXUS_JOIN_CLIENT_PROCESS);
      }
   } else {
      LOGE("%s: dyn_EGL_nexus_join unavailable, something will break!", __FUNCTION__);
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
   NEXUS_Error     rc;

   gralloc_load_lib();

   rc = BKNI_CreateEvent(&hCheckpointEvent);
   if (rc) {
      LOGE("Unable to create checkpoint event");
      hCheckpointEvent = NULL;
      hGraphics = NULL;
   } else {
      hGraphics = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
      if (!hGraphics) {
         LOGW("Unable to open Graphics2D.  HW/SW access format conversions will fail...");
      } else {
         NEXUS_Graphics2DSettings gfxSettings;
         NEXUS_Graphics2D_GetSettings(hGraphics, &gfxSettings);
         gfxSettings.pollingCheckpoint = false;
         gfxSettings.checkpointCallback.callback = gralloc_checkpoint_callback;
         gfxSettings.checkpointCallback.context = (void *)hCheckpointEvent;
         rc = NEXUS_Graphics2D_SetSettings(hGraphics, &gfxSettings);
         if ( rc ) {
            LOGW("Unable to set Graphics2D Settings");
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

      default:
         {
               b = 0;   pf = NEXUS_PixelFormat_eUnknown;
               LOGE("%s %d FORMAT [ %d ] NOT SUPPORTED ",__FUNCTION__,__LINE__,pixelFmt);
               break;
         }
   }

   *bpp = b;
   return pf;
}

static
unsigned int allocGLSuitableBuffer(private_handle_t * allocContext,
                                   int width,
                                   int height,
                                   int format)
{
   BEGL_PixmapInfo bufferRequirements;
   BEGL_BufferSettings bufferConstrainedRequirements;
   unsigned int phyAddr = 0;

   if (!allocContext) {
      BCM_DEBUG_ERRMSG("%s : invalid handle", __FUNCTION__);
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
         LOGE("%s: no valid client context...", __FUNCTION__);
      }
      dyn_BEGLint_BufferGetRequirements(&bufferRequirements, &bufferConstrainedRequirements);

      // nx_ashmem always aligns to 4k.  Add an additional 4k block at the start for shared memory.
      // This needs reference counting in the same way as the regular blocks
      ret = ioctl(allocContext->fd, NX_ASHMEM_SET_SIZE, bufferConstrainedRequirements.totalByteSize);
      if (ret < 0) {
         return 0;
      };

      // This is similar to the mmap call in ashmem, but nexus doesnt work in this way, so tunnel
      // via IOCTL
      phyAddr = (NEXUS_Addr)ioctl(allocContext->fd, NX_ASHMEM_GETMEM);

      allocContext->oglStride = bufferConstrainedRequirements.pitchBytes;
      allocContext->oglFormat = bufferConstrainedRequirements.format;
      allocContext->oglSize   = bufferConstrainedRequirements.totalByteSize;
   }
   else {
      BCM_DEBUG_MSG("=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+");
      BCM_DEBUG_MSG("             BRCM-GRALLOC: [OGL-INTEGRATION] ASSIGNING OGL PARAMS=0");
      BCM_DEBUG_MSG("=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+");
      allocContext->oglStride = 0;
      allocContext->oglFormat = 0;
      allocContext->oglSize   = 0;
   }

   return phyAddr;
}

static void getBufferDataFromFormat(int w, int h, int bpp, int format, int *pStride, int *size, int *extra_size)
{
   int align = 16;

   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      case HAL_PIXEL_FORMAT_RGB_565:
      {
          int bpr = (w*bpp + (align-1)) & ~(align-1);
          *size = bpr * h;
          *pStride = bpr / bpp;
          *extra_size = 0;
      }
      break;

      case HAL_PIXEL_FORMAT_YV12:
      {
          // use y-stride: ALIGN(w, 16)
          *pStride = (w + (align-1)) & ~(align-1);
          // size: y-stride * h + 2 * (c-stride * h/2), with c-stride: ALIGN(y-stride/2, 16)
          *size = (*pStride * h) + 2 * ((h/2) * ((*pStride/2 + (align-1)) & ~(align-1)));
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
   int bpp = 0, fd = -1, fd2 = -1, fd3 = -1;
   int size, extra_size;
   NEXUS_PixelFormat nxFormat = getNexusPixelFormat(format,&bpp);

   (void)dev;

   getBufferDataFromFormat(w, h, bpp, format, pStride, &size, &extra_size);

   if (nxFormat == NEXUS_PixelFormat_eUnknown) {
      LOGE("%s %d : UnSupported Format In Gralloc",__FUNCTION__,__LINE__);
      return -EINVAL;
   }

   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
   strcpy(value2, "/dev/");
   strcat(value2, value);

   fd = open(value2, O_RDWR, 0);
   if ((fd == -1) || (!fd)) {
      LOGE("default-plane: open %s failed 0x%x\n", value2, fd);
      return -EINVAL;
   }

   fd2 = open(value2, O_RDWR, 0);
   if ((fd2 == -1) || (!fd2)) {
      LOGE("shared-data: open %s failed 0x%x\n", value2, fd2);
      return -EINVAL;
   }

   fd3 = open(value2, O_RDWR, 0);
   if ((fd3 == -1) || (!fd3)) {
      LOGE("extra-plane: open %s failed 0x%x\n", value2, fd3);
      return -EINVAL;
   }

   private_handle_t *grallocPrivateHandle = new private_handle_t(fd, fd2, fd3, 0);

   int ret = ioctl(fd2, NX_ASHMEM_SET_SIZE, sizeof(SHARED_DATA));
   if (ret < 0) {
      return -ENOMEM;
   };

   grallocPrivateHandle->sharedData = (NEXUS_Addr)ioctl(fd2, NX_ASHMEM_GETMEM);
   if (grallocPrivateHandle->sharedData == 0) {
      LOGE("%s %d: hnd->sharedData == 0", __FUNCTION__, __LINE__);
      return -ENOMEM;
   }

   PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(grallocPrivateHandle->sharedData);
   memset(pSharedData, 0, sizeof(SHARED_DATA));

   bool needs_yv12 = false;
   bool needs_ycrcb = false;

   grallocPrivateHandle->usage = usage;
   pSharedData->planes[DEFAULT_PLANE].width = w;
   pSharedData->planes[DEFAULT_PLANE].height = h;
   pSharedData->planes[DEFAULT_PLANE].bpp = bpp;
   pSharedData->planes[DEFAULT_PLANE].format = format;
   pSharedData->planes[DEFAULT_PLANE].size = size;

   if (format != HAL_PIXEL_FORMAT_YV12) {
      // standard graphic buffer.
      pSharedData->planes[DEFAULT_PLANE].physAddr =
         allocGLSuitableBuffer(grallocPrivateHandle, w, h, format);
      grallocPrivateHandle->nxSurfacePhysicalAddress =
         pSharedData->planes[DEFAULT_PLANE].physAddr;
   } else if ((format == HAL_PIXEL_FORMAT_YV12) && !(usage & GRALLOC_USAGE_PRIVATE_0)) {
      // standard yv12 buffer for multimedia, need also a secondary buffer for
      // planar to packed conversion when going through the hwc/nsc.
      needs_yv12 = true;
      needs_ycrcb = true;
   } else if ((format == HAL_PIXEL_FORMAT_YV12) && (usage & GRALLOC_USAGE_PRIVATE_0) & (usage & GRALLOC_USAGE_SW_READ_OFTEN)) {
      // private multimedia buffer, we only need a yv12 plane in case cpu is intending to read
      // the content, eg decode->encode type of scenario; yv12 data is produced during lock.
      needs_yv12 = true;
   }

   if (needs_yv12) {
      ret = ioctl(fd, NX_ASHMEM_SET_SIZE, size);
      if (ret >= 0) {
         pSharedData->planes[DEFAULT_PLANE].physAddr =
             (NEXUS_Addr)ioctl(fd, NX_ASHMEM_GETMEM);
      }
   }

   if (needs_ycrcb) {
      pSharedData->planes[EXTRA_PLANE].width = w;
      pSharedData->planes[EXTRA_PLANE].height = h;
      pSharedData->planes[EXTRA_PLANE].bpp = bpp;
      pSharedData->planes[EXTRA_PLANE].format = (int)nxFormat;
      pSharedData->planes[EXTRA_PLANE].size = extra_size;
      ret = ioctl(fd3, NX_ASHMEM_SET_SIZE, extra_size);
      if (ret >= 0) {
         pSharedData->planes[EXTRA_PLANE].physAddr =
             (NEXUS_Addr)ioctl(fd3, NX_ASHMEM_GETMEM);
      }
   }

   bool alloc_failed = false;
   if (needs_yv12 && (pSharedData->planes[DEFAULT_PLANE].physAddr == 0)) {
      LOGE("%s: failed to allocate default yv12 plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      alloc_failed = true;
   }
   if (needs_ycrcb && (pSharedData->planes[EXTRA_PLANE].physAddr == 0)) {
      LOGE("%s: failed to allocate extra ycrcb plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      alloc_failed = true;
   }
   if (!needs_yv12 && (pSharedData->planes[DEFAULT_PLANE].physAddr == 0)) {
      LOGE("%s: failed to allocate standard plane (%d,%d), size %d", __FUNCTION__, w, h, extra_size);
      alloc_failed = true;
   }

   if (alloc_failed) {
      *pHandle = NULL;
      delete grallocPrivateHandle;
      close(fd);
      close(fd2);
      close(fd3);
      return -EINVAL;
   }

   *pHandle = grallocPrivateHandle;

   return 0;
}

static int
grallocFreeHandle(private_handle_t *handleToFree)
{
   if (!handleToFree) {
      LOGE("%s:!!!! Invalid Arguments, Cannot Process Free!!", __FUNCTION__);
      return -1;
   }

   PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(handleToFree->sharedData);
   if ( pSharedData )
   {
     if ( android_atomic_acquire_load(&pSharedData->hwc.active) )
     {
       ALOGE("Freeing gralloc buffer %#x used by HWC!  layer %d surface %#x", handleToFree->sharedData, pSharedData->hwc.layer, pSharedData->hwc.surface);
     }
   }

   if (handleToFree->fd >= 0)
      close(handleToFree->fd);
   if (handleToFree->fd2 >= 0)
      close(handleToFree->fd2);
   if (handleToFree->fd3 >= 0)
      close(handleToFree->fd3);
   delete handleToFree;
   return 0;
}

static int
gralloc_free_buffer(alloc_device_t* dev, private_handle_t *hnd)
{
   (void)dev;
   grallocFreeHandle(hnd);
   return 0;
}


/*****************************************************************************/

static int gralloc_alloc(alloc_device_t* dev,
        int w, int h, int format, int usage,
        buffer_handle_t* pHandle, int* pStride)
{
   if (!pHandle || !pStride) {
      LOGE("%s : condition check failed", __FUNCTION__);
         return -EINVAL;
   }

   int err;
   err = gralloc_alloc_buffer(dev, w, h, format, usage, pHandle, pStride);

   if (usage & GRALLOC_USAGE_HW_FB) {
      LOGI("%s : allocated framebuffer w=%d, h=%d, format=%d, usage=0x%08x, %p", __FUNCTION__, w, h, format, usage, pHandle);
   }

   if (err < 0) {
      LOGE("%s : w=%d, h=%d, format=%d, usage=0x%08x", __FUNCTION__, w, h, format, usage);
      LOGE("%s : alloc returning error", __FUNCTION__);
   }

   return err;
}

static int gralloc_free(alloc_device_t* dev,
        buffer_handle_t handle)
{
   if (private_handle_t::validate(handle) < 0)
   {
      LOGE("gralloc_free Handle Validation Failed \n");
      return -EINVAL;
   }

   private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
   gralloc_free_buffer(dev,const_cast<private_handle_t*>(hnd));

   return 0;
}

/*****************************************************************************/

static int gralloc_close(struct hw_device_t *dev)
{
   LOGD("%s [%d]: Vector Graphics Allocator [L] Build Date[%s Time:%s]\n",
               __FUNCTION__,__LINE__,
               __DATE__,
               __TIME__);

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

   LOGD("%s[%d]: Vector Graphics Allocator [L] Build Date[%s Time:%s]\n",
               __FUNCTION__,__LINE__,
               __DATE__,
               __TIME__);

   if (!strcmp(name, GRALLOC_HARDWARE_GPU0))
   {
      LOGI("Using Hardware GPU type device\n");
      void *alloced=NULL;

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
