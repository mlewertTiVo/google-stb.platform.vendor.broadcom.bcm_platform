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

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"
#include "nexus_base_mmap.h"
#include "nexus_platform.h"

// default platform layer to render to nexus
#include "EGL/egl.h"

#include "cutils/properties.h"
#include "nx_ashmem.h"

static NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt, int *bpp);
static BM2MC_PACKET_PixelFormat getBm2mcPixelFormat(int pixelFmt);

void __attribute__ ((constructor)) gralloc_explicit_load(void);
void __attribute__ ((destructor)) gralloc_explicit_unload(void);

#if defined(V3D_VARIANT_v3d)
static void (* dyn_BEGLint_BufferGetRequirements)(BEGL_PixmapInfo *, BEGL_BufferSettings *);
#endif
static void * (* dyn_EGL_nexus_join)(char *client_process_name);
static void (* dyn_EGL_nexus_unjoin)(void *nexus_client);
#define LOAD_FN(lib, name) \
if (!(dyn_ ## name = (typeof(dyn_ ## name)) dlsym(lib, #name))) \
   ALOGV("failed resolving '%s'", #name); \
else \
   ALOGV("resolved '%s' to %p", #name, dyn_ ## name);
static void *gl_dyn_lib;
static void *nexus_client = NULL;
static int gralloc_mgmt_mode = -1;
static int gralloc_default_align = 0;
static int gralloc_log_map = 0;
static int gralloc_conv_time = 0;
static int gralloc_boom_chk = 0;

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

#define NX_MMA_MGMT_MODE        "ro.nx.mma.mgmt_mode"
#define NX_GR_LOG_MAP           "ro.gr.log.map"
#define NX_GR_CONV_TIME         "ro.gr.conv.time"
#define NX_GR_BOOM_CHK          "ro.gr.boom.chk"
#define NX_MMA_MGMT_MODE_DEF    "locked"

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
#if defined(V3D_VARIANT_v3d)
   LOAD_FN(gl_dyn_lib, BEGLint_BufferGetRequirements);
#endif
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

   if (property_get(NX_MMA_MGMT_MODE, value, NX_MMA_MGMT_MODE_DEF)) {
      gralloc_mgmt_mode = (strncmp(value, NX_MMA_MGMT_MODE_DEF, sizeof(NX_MMA_MGMT_MODE_DEF)) == 0) ?
         GR_MGMT_MODE_LOCKED : GR_MGMT_MODE_UNLOCKED;
   }

   if (property_get(NX_GR_LOG_MAP, value, "0")) {
      gralloc_log_map = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

   if (property_get(NX_GR_CONV_TIME, value, "0")) {
      gralloc_conv_time = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
   }

#if defined(V3D_VARIANT_v3d)
   gralloc_default_align = GRALLOC_MAX_BUFFER_ALIGNED;
#else
   gralloc_default_align = GRALLOC_MIN_BUFFER_ALIGNED;
#endif

   if (property_get(NX_GR_BOOM_CHK, value, "0")) {
      gralloc_boom_chk = (strtoul(value, NULL, 10) > 0) ? 1 : 0;
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

int gralloc_boom_check(void)
{
   return gralloc_boom_chk;
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

static void gralloc_bzero(PSHARED_DATA pSharedData)
{
    NEXUS_Graphics2DHandle gfx = gralloc_g2d_hdl();
    BKNI_EventHandle event = gralloc_g2d_evt();
    pthread_mutex_t *pMutex = gralloc_g2d_lock();
    bool done=false;
    NEXUS_Addr physAddr;
    void *pMemory = NULL;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    static const BM2MC_PACKET_Blend copyColor = {BM2MC_PACKET_BlendFactor_eSourceColor, BM2MC_PACKET_BlendFactor_eOne, false,
       BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};
    static const BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eSourceAlpha, BM2MC_PACKET_BlendFactor_eOne, false,
       BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};

    block_handle = (NEXUS_MemoryBlockHandle)pSharedData->container.physAddr;
    NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
    NEXUS_MemoryBlock_Lock(block_handle, &pMemory);

    if (pMemory == NULL) {
       goto out_release;
    }

    if (gfx && event && pMutex) {
        NEXUS_Error errCode;
        size_t pktSize;
        void *pktBuffer, *next;

        pthread_mutex_lock(pMutex);
        errCode = NEXUS_Graphics2D_GetPacketBuffer(gfx, &pktBuffer, &pktSize, 1024);
        if (errCode == 0) {
           next = pktBuffer;
           {
              BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
              BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
              pPacket->plane.address = physAddr;
              pPacket->plane.pitch   = pSharedData->container.stride;
              pPacket->plane.format  = getBm2mcPixelFormat(pSharedData->container.format);
              pPacket->plane.width   = pSharedData->container.width;
              pPacket->plane.height  = pSharedData->container.height;
              next = ++pPacket;
           }
           {
              BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
              BM2MC_PACKET_INIT( pPacket, Blend, false );
              pPacket->color_blend   = copyColor;
              pPacket->alpha_blend   = copyAlpha;
              pPacket->color         = 0;
              next = ++pPacket;
           }
           {
              BM2MC_PACKET_PacketSourceColor *pPacket = (BM2MC_PACKET_PacketSourceColor *)next;
              BM2MC_PACKET_INIT(pPacket, SourceColor, false );
              pPacket->color         = 0x00000000;
              next = ++pPacket;
           }
           {
              BM2MC_PACKET_PacketFillBlit *pPacket = (BM2MC_PACKET_PacketFillBlit *)next;
              BM2MC_PACKET_INIT(pPacket, FillBlit, true);
              pPacket->rect.x        = 0;
              pPacket->rect.y        = 0;
              pPacket->rect.width    = pSharedData->container.width;
              pPacket->rect.height   = pSharedData->container.height;
              next = ++pPacket;
           }
           errCode = NEXUS_Graphics2D_PacketWriteComplete(gfx, (uint8_t*)next - (uint8_t*)pktBuffer);
           if (errCode == 0) {
              errCode = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
              if (errCode == NEXUS_GRAPHICS2D_QUEUED) {
                 errCode = BKNI_WaitForEvent(event, CHECKPOINT_TIMEOUT);
                 if (errCode) {
                    ALOGW("Timeout zeroing gralloc buffer");
                 }
              }
           }
           if (!errCode) {
              done = true;
           }
        }
        pthread_mutex_unlock(pMutex);
    }

    if (!done) {
       bzero(pMemory, pSharedData->container.size);
    }

out_release:
    if (block_handle) {
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

NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt, int *bpp)
{
   NEXUS_PixelFormat pf;
   int b;
   switch (pixelFmt) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
         b = 4;
         pf = NEXUS_PixelFormat_eA8_B8_G8_R8;
      break;
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
         b = 4;
         pf = NEXUS_PixelFormat_eX8_R8_G8_B8;
      break;
      case HAL_PIXEL_FORMAT_RGB_565:
         b = 2;
         pf = NEXUS_PixelFormat_eR5_G6_B5;
      break;
      case HAL_PIXEL_FORMAT_YV12:
         /* no native nexus support, return the 'converted for nexus consumption'. */
         b = 2;
         pf = NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      default:
         b = 0;
         pf = NEXUS_PixelFormat_eUnknown;
         ALOGE("%s %d FORMAT [ %d ] NOT SUPPORTED ",__FUNCTION__,__LINE__,pixelFmt);
      break;
   }

   *bpp = b;
   return pf;
}

BM2MC_PACKET_PixelFormat getBm2mcPixelFormat(int pixelFmt)
{
   BM2MC_PACKET_PixelFormat pf;
   switch (pixelFmt) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
         pf = BM2MC_PACKET_PixelFormat_eA8_B8_G8_R8;
      break;
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
         pf = BM2MC_PACKET_PixelFormat_eX8_R8_G8_B8;
      break;
      case HAL_PIXEL_FORMAT_RGB_565:
         pf = BM2MC_PACKET_PixelFormat_eR5_G6_B5;
      break;
      case HAL_PIXEL_FORMAT_YV12:
         /* no native bm2mc support, return the 'converted for bm2mc consumption'. */
         pf = BM2MC_PACKET_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      default:
         pf = BM2MC_PACKET_PixelFormat_eUnknown;
         ALOGE("%s %d FORMAT [ %d ] NOT SUPPORTED ",__FUNCTION__,__LINE__,pixelFmt);
      break;
   }

   return pf;
}

static unsigned int setupGLSuitableBuffer(private_handle_t *hnd, PSHARED_DATA pSharedData)
{
   BEGL_PixmapInfo bufferRequirements;
#if defined(V3D_VARIANT_v3d)
   BEGL_BufferSettings bufferConstrainedRequirements;
#endif
   int rc = -EINVAL;

   memset(&bufferRequirements, 0, sizeof(BEGL_PixmapInfo));
   bufferRequirements.width = pSharedData->container.width;
   bufferRequirements.height = pSharedData->container.height;
   bufferRequirements.format = BEGL_BufferFormat_INVALID;

   switch (pSharedData->container.format) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
         bufferRequirements.format = BEGL_BufferFormat_eA8B8G8R8;
      break;
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
         bufferRequirements.format = BEGL_BufferFormat_eX8B8G8R8;
      break;
      case HAL_PIXEL_FORMAT_RGB_565:
         bufferRequirements.format = BEGL_BufferFormat_eR5G6B5;
      break;
      case HAL_PIXEL_FORMAT_YV12:
#if defined(V3D_VARIANT_v3d)
         bufferRequirements.format = BEGL_BufferFormat_eYV12_Texture;
#else
         bufferRequirements.format = BEGL_BufferFormat_eYV12;
#endif
      break;
      default:
      break;
   }

   switch (bufferRequirements.format) {
      case BEGL_BufferFormat_INVALID:
         hnd->oglStride = 0;
         hnd->oglFormat = 0;
         hnd->oglSize   = 0;
      break;
#if defined(V3D_VARIANT_v3d)
      case BEGL_BufferFormat_eYV12_Texture:
#else
      case BEGL_BufferFormat_eYV12:
#endif
         hnd->oglStride = pSharedData->container.stride;
         hnd->oglFormat = bufferRequirements.format;
         hnd->oglSize   = pSharedData->container.size;
         rc = 0;
      break;
      default:
#if defined(V3D_VARIANT_v3d)
         if (gralloc_v3d_get_nexus_client_context() == NULL) {
            ALOGE("%s: no valid client context...", __FUNCTION__);
         }
         dyn_BEGLint_BufferGetRequirements(&bufferRequirements, &bufferConstrainedRequirements);
         hnd->oglStride = bufferConstrainedRequirements.pitchBytes;
         hnd->oglFormat = bufferConstrainedRequirements.format;
         hnd->oglSize   = bufferConstrainedRequirements.totalByteSize;
#else
         hnd->oglStride = pSharedData->container.width * pSharedData->container.bpp;
         hnd->oglFormat = bufferRequirements.format;
         hnd->oglSize   = pSharedData->container.height * hnd->oglStride;
#endif
         rc = 0;
      break;
   }

out:
   return rc;
}

static void getBufferDataFromFormat(int *alignment, int w, int h, int bpp, int format, int *pStride, int *size)
{
   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      case HAL_PIXEL_FORMAT_RGB_565:
         *pStride = ((w*bpp + (*alignment-1)) & ~(*alignment-1)) / bpp;
         *size = ((w*bpp + (*alignment-1)) & ~(*alignment-1)) * h;
      break;
      case HAL_PIXEL_FORMAT_YV12:
         // force alignment according to (android) format definition.
         *alignment = 16;
         // use y-stride: ALIGN(w, 16)
         *pStride = (w + (*alignment-1)) & ~(*alignment-1);
         // size: y-stride * h + 2 * (c-stride * h/2), with c-stride: ALIGN(y-stride/2, 16)
         *size = (*pStride * h) + 2 * ((h/2) * ((*pStride/2 + (*alignment-1)) & ~(*alignment-1)));
      break;
      default:
         *pStride = 0;
         *size = 0;
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
   int bpp = 0, err = 0, ret = 0, fd = -1, fd2 = -1;
   int size, fmt_align, fmt_set = GR_NONE;
   NEXUS_PixelFormat nxFormat = getNexusPixelFormat(format, &bpp);
   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   void *pMemory;
   struct nx_ashmem_alloc ashmem_alloc;

   private_handle_t *hnd = NULL;
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
      err = -EINVAL;
      goto error;
   }
   fd2 = open(value2, O_RDWR, 0);
   if ((fd2 == -1) || (!fd2)) {
      err = -EINVAL;
      goto error;
   }

   hnd = new private_handle_t(fd, fd2, 0);
   if (hnd == NULL) {
      err = -ENOMEM;
      goto error;
   }

   hnd->mgmt_mode = gralloc_mgmt_mode;
   hnd->alignment = 1;
#if defined(V3D_VARIANT_v3d)
   hnd->alignment = 16;
#endif

   fmt_align = hnd->alignment;
   getBufferDataFromFormat(&fmt_align, w, h, bpp, format, pStride, &size);
   if (fmt_align != hnd->alignment) {
      hnd->alignment = fmt_align;
   }

   memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
   ashmem_alloc.size = sizeof(SHARED_DATA);
   ashmem_alloc.align = GRALLOC_MAX_BUFFER_ALIGNED;
   ret = ioctl(hnd->fd2, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
   if (ret < 0) {
      err = -ENOMEM;
      goto error;
   };

   hnd->sharedData = (NEXUS_Addr)ioctl(hnd->fd2, NX_ASHMEM_GETMEM);
   if (hnd->sharedData == 0) {
      err = -ENOMEM;
      goto error;
   }

   pMemory = NULL;
   block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;

   if (pSharedData == NULL) {
      /* that's pretty bad...  failed to map the allocated memory! */
      err = -ENOMEM;
      goto error;
   }

   memset(pSharedData, 0, sizeof(SHARED_DATA));

   hnd->usage = usage;
   pSharedData->container.width     = w;
   pSharedData->container.height    = h;
   pSharedData->container.bpp       = bpp;
   pSharedData->container.format    = format;
   pSharedData->container.size      = size;
   pSharedData->container.allocSize = size;
   pSharedData->container.stride    = *pStride;

   if (setupGLSuitableBuffer(hnd, pSharedData)) {
      ALOGE("%s: failed to setup GL buffer, aborting...", __FUNCTION__);
      return -EINVAL;
   }

   if (format != HAL_PIXEL_FORMAT_YV12) {
      fmt_set |= GR_STANDARD;
   } else if ((format == HAL_PIXEL_FORMAT_YV12) && !(usage & GRALLOC_USAGE_PRIVATE_0)) {
      fmt_set |= GR_YV12;
   } else if ((format == HAL_PIXEL_FORMAT_YV12) && (usage & GRALLOC_USAGE_PRIVATE_0)) {
      if (usage & GRALLOC_USAGE_SW_READ_OFTEN) {
         // private multimedia buffer, we only need a yv12 plane in case cpu is intending to read
         // the content, eg decode->encode type of scenario; yv12 data is produced during lock.
         fmt_set |= GR_YV12;
      } else {
         fmt_set = GR_NONE;
      }
   }
   memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
   ashmem_alloc.align = gralloc_default_align;
   if ((fmt_set & GR_YV12) == GR_YV12) {
      ashmem_alloc.size = size;
      if (usage & GRALLOC_USAGE_HW_TEXTURE) {
         if ((w <= DATA_PLANE_MAX_WIDTH &&
              h <= DATA_PLANE_MAX_HEIGHT)) {
            fmt_set |= GR_HWTEX;
         }
      }
   } else {
      pSharedData->container.allocSize = hnd->oglSize;
      pSharedData->container.stride = hnd->oglStride;
      ashmem_alloc.size = pSharedData->container.allocSize;
   }
   ret = ioctl(hnd->fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
   if (ret >= 0) {
      pSharedData->container.physAddr =
          (NEXUS_Addr)ioctl(hnd->fd, NX_ASHMEM_GETMEM);
   }
   hnd->fmt_set = fmt_set;

   if (hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) {
      NEXUS_Addr physAddr;
      NEXUS_MemoryBlock_LockOffset((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr, &physAddr);
      hnd->nxSurfacePhysicalAddress = (unsigned)physAddr;
      pMemory = NULL;
      NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr, &pMemory);
      hnd->nxSurfaceAddress = (unsigned)pMemory;
   } else {
      hnd->nxSurfacePhysicalAddress = (unsigned)0;
      hnd->nxSurfaceAddress = (unsigned)pMemory;
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr physAddr;
      unsigned sharedPhysAddr = hnd->sharedData;
      NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
      sharedPhysAddr = (unsigned)physAddr;
      ALOGI("alloc (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::sz:%d::use:0x%x:0x%x::mapped:0x%x",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
            getpid(),
            hnd->sharedData,
            sharedPhysAddr,
            pSharedData->container.physAddr,
            hnd->nxSurfacePhysicalAddress,
            pSharedData->container.size,
            hnd->usage,
            hnd->fmt_set,
            hnd->nxSurfaceAddress);
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
   }

   if ((fmt_set != GR_NONE) && pSharedData->container.physAddr == 0) {
      ALOGE("%s: failed to allocate default plane (%d,%d), size %d", __FUNCTION__, w, h, size);
      err = -ENOMEM;
      goto alloc_failed;
   } else if (pSharedData->container.physAddr) {
       gralloc_bzero(pSharedData);
   }

   *pHandle = hnd;

   if (block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

   return 0;

alloc_failed:
   if (block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

error:
   *pHandle = NULL;
   delete hnd;
   if (fd > 0)
      close(fd);
   if (fd2 > 0)
      close(fd2);
   return err;
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

   void *pMemory;
   block_handle = (NEXUS_MemoryBlockHandle)hnd->sharedData;
   NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;

   if (pSharedData) {
      if (gralloc_log_mapper()) {
         NEXUS_Addr physAddr = 0;
         unsigned sharedPhysAddr = hnd->sharedData;
         unsigned planePhysAddr = pSharedData->container.physAddr;
         unsigned planePhysSize = pSharedData->container.size;
         NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
         sharedPhysAddr = (unsigned)physAddr;
         ALOGI(" free (%s): owner:%d::s-blk:0x%x::s-addr:0x%x::p-blk:0x%x::p-addr:0x%x::sz:%d::use:0x%x:0x%x::mapped:0x%x",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : "ST",
               hnd->pid,
               hnd->sharedData,
               sharedPhysAddr,
               planePhysAddr,
               hnd->nxSurfacePhysicalAddress,
               planePhysSize,
               hnd->usage,
               hnd->fmt_set,
               hnd->nxSurfaceAddress);
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
      }
   }

   if (pSharedData) {
      if (hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) {
         if (pSharedData->container.physAddr) {
            NEXUS_MemoryBlock_UnlockOffset((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr);
            NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlockHandle)pSharedData->container.physAddr);
         }
      }
   }
   if (block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }

   if (hnd->fd >= 0) {
      close(hnd->fd);
   }
   if (hnd->fd2 >= 0) {
      close(hnd->fd2);
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
