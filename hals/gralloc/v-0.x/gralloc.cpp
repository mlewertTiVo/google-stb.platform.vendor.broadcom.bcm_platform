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

#include <cutils/atomic.h>
#include "gralloc_priv.h"
#include "nexus_base_mmap.h"
#include "nexus_platform.h"

// default platform layer to render to nexus
#include "EGL/egl.h"

#include "cutils/properties.h"
#include "nx_ashmem.h"
#include <inttypes.h>

static NEXUS_PixelFormat getNexusPixelFormat(int pixelFmt, int *bpp);
static BM2MC_PACKET_PixelFormat getBm2mcPixelFormat(int pixelFmt);

void __attribute__ ((constructor)) gralloc_explicit_load(void);
void __attribute__ ((destructor)) gralloc_explicit_unload(void);

extern "C" void *nxwrap_create_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);
extern "C" void nxwrap_rmlmk(void *wrap);

#if defined(V3D_VARIANT_v3d)
static void *gl_dyn_lib;
static void (* dyn_BEGLint_BufferGetRequirements)(BEGL_PixmapInfoEXT *, BEGL_BufferSettings *);
#endif
#define LOAD_FN(lib, name) \
if (!(dyn_ ## name = (typeof(dyn_ ## name)) dlsym(lib, #name))) \
   ALOGV("failed resolving '%s'", #name); \
else \
   ALOGV("resolved '%s' to %p", #name, dyn_ ## name);
static void *nxwrap = NULL;
static void *nexus_client = NULL;
static int gralloc_mgmt_mode = -1;
static int gralloc_default_align = 0;
static int gralloc_log_map = 0;
static int gralloc_conv_time = 0;
static int gralloc_boom_chk = 0;

static pthread_mutex_t moduleLock = PTHREAD_MUTEX_INITIALIZER;
static NEXUS_Graphics2DHandle hGraphics = NULL;
static BKNI_EventHandle hCheckpointEvent = NULL;

/* yt360: qHD minimum; cert: qHD and above. */
#if defined(V3D_VARIANT_v3d)
#define DATA_PLANE_MAX_WIDTH    2048 /*TODO*/
#define DATA_PLANE_MAX_HEIGHT   1440
#else
#define DATA_PLANE_MAX_WIDTH    4096
#define DATA_PLANE_MAX_HEIGHT   2160
#endif

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

#if defined(V3D_VARIANT_v3d)
   snprintf(value, PROPERTY_VALUE_MAX, "%slibGLES_nexus.so", V3D_DLOPEN_PATH);
   gl_dyn_lib = dlopen(value, RTLD_LAZY | RTLD_LOCAL);
   if (!gl_dyn_lib) {
      // last resort legacy path.
      snprintf(value, PROPERTY_VALUE_MAX, "/vendor/lib/egl/libGLES_nexus.so");
      gl_dyn_lib = dlopen(value, RTLD_LAZY | RTLD_LOCAL);
      if (!gl_dyn_lib) {
         ALOGE("failed loading essential GLES library '%s': <%s>!", value, dlerror());
      }
   }
   // load wanted functions from the library now.
   LOAD_FN(gl_dyn_lib, BEGLint_BufferGetRequirements);
#endif

   nexus_client = nxwrap_create_client(&nxwrap);
   if (nexus_client == NULL) {
      ALOGE("%s: failed joining nexus client '%s'!", __FUNCTION__, NEXUS_JOIN_CLIENT_PROCESS);
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

   gralloc_load_lib();

   rc = BKNI_CreateEvent(&hCheckpointEvent);
   if (rc) {
      hCheckpointEvent = NULL;
      hGraphics = NULL;
   }
}

void gralloc_g2d_hdl_end(void)
{
   if (hGraphics == NULL) {
      return;
   } else {
      NEXUS_Graphics2D_Close(hGraphics);
      hGraphics = NULL;
   }
}

void gralloc_explicit_unload(void)
{
   if (nxwrap) {
      nxwrap_destroy_client(nxwrap);
      nexus_client = NULL;
      nxwrap = NULL;
   }

#if defined(V3D_VARIANT_v3d)
   dlclose(gl_dyn_lib);
#endif

   if (hCheckpointEvent) {
      BKNI_DestroyEvent(hCheckpointEvent);
      hCheckpointEvent = NULL;
   }
   gralloc_g2d_hdl_end();
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

int gralloc_align(void)
{
   return gralloc_default_align;
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
   NEXUS_Error rc;
   NEXUS_Graphics2DOpenSettings g2dOpenSettings;

   if (hCheckpointEvent == NULL) {
      hGraphics = NULL;
      return NULL;
   }
   if (hGraphics != NULL) {
      return hGraphics;
   } else {
      NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOpenSettings);
      g2dOpenSettings.compatibleWithSurfaceCompaction = false;
      hGraphics = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOpenSettings);
      if (hGraphics) {
         NEXUS_Graphics2DSettings gfxSettings;
         NEXUS_Graphics2D_GetSettings(hGraphics, &gfxSettings);
         gfxSettings.pollingCheckpoint = false;
         gfxSettings.checkpointCallback.callback = gralloc_checkpoint_callback;
         gfxSettings.checkpointCallback.context = (void *)hCheckpointEvent;
         rc = NEXUS_Graphics2D_SetSettings(hGraphics, &gfxSettings);
         if (rc) {
            NEXUS_Graphics2D_Close(hGraphics);
            hGraphics = NULL;
         }
      }
   }
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
    NEXUS_Error lrco, lrc;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    static const BM2MC_PACKET_Blend copyColor = {BM2MC_PACKET_BlendFactor_eSourceColor, BM2MC_PACKET_BlendFactor_eOne, false,
       BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};
    static const BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eSourceAlpha, BM2MC_PACKET_BlendFactor_eOne, false,
       BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};

    block_handle = pSharedData->container.block;
    lrco = NEXUS_MemoryBlock_LockOffset(block_handle, &physAddr);
    lrc  = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);

    if (lrc || lrco || pMemory == NULL) {
       if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
       goto out;
    }

    if (gfx && event && pMutex) {
        NEXUS_Error errCode;
        size_t pktSize;
        void *pktBuffer, *next;

        pthread_mutex_lock(pMutex);
        switch (pSharedData->container.format) {
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_BLOB:
        case HAL_PIXEL_FORMAT_RGBA_FP16:
        case HAL_PIXEL_FORMAT_RGBA_1010102:
           errCode = 0;
        break;
        default:
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
              }{
                 BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
                 BM2MC_PACKET_INIT( pPacket, Blend, false );
                 pPacket->color_blend   = copyColor;
                 pPacket->alpha_blend   = copyAlpha;
                 pPacket->color         = 0;
                 next = ++pPacket;
              }{
                 BM2MC_PACKET_PacketSourceColor *pPacket = (BM2MC_PACKET_PacketSourceColor *)next;
                 BM2MC_PACKET_INIT(pPacket, SourceColor, false );
                 pPacket->color         = 0x00000000;
                 next = ++pPacket;
              }{
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
                       ALOGW("timeout null'ing gralloc buffer");
                    }
                 }
              }
           }
           break;
        }
        pthread_mutex_unlock(pMutex);
        if (!errCode) {
           done = true;
        }
    }

    if (!done) {
       bzero(pMemory, pSharedData->container.size);
    }

out_release:
    if (block_handle) {
       if (!lrco) NEXUS_MemoryBlock_UnlockOffset(block_handle);
       if (!lrc)  NEXUS_MemoryBlock_Unlock(block_handle);
    }
out:
    if (gfx != NULL) {
       gralloc_g2d_hdl_end();
    }
    return;
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

extern int gralloc_lock_ycbcr(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        struct android_ycbcr *ycbcr);

/*****************************************************************************/

static struct hw_module_methods_t gralloc_module_methods = {
        .open = gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM = {
   .base = {
      .common = {
         .tag                = HARDWARE_MODULE_TAG,
         .module_api_version = GRALLOC_MODULE_API_VERSION_0_2,
         .hal_api_version    = HARDWARE_HAL_API_VERSION,
         .id                 = GRALLOC_HARDWARE_MODULE_ID,
         .name               = "gralloc for set-top-box platforms",
         .author             = "Broadcom",
         .methods            = &gralloc_module_methods,
         .dso                = 0,
         .reserved           = {0}
      },
      .registerBuffer        = gralloc_register_buffer,
      .unregisterBuffer      = gralloc_unregister_buffer,
      .lock                  = gralloc_lock,
      .unlock                = gralloc_unlock,
      .perform               = NULL,
      .lock_ycbcr            = gralloc_lock_ycbcr,
      .lockAsync             = NULL,
      .unlockAsync           = NULL,
      .lockAsync_ycbcr       = NULL,
      .reserved_proc         = {0, 0, 0}
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
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         /* no native nexus support, return the 'converted for nexus consumption'. */
         b = 2;
         pf = NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      case HAL_PIXEL_FORMAT_BLOB:
         b = 1;
         pf = NEXUS_PixelFormat_ePalette1;
      break;
      case HAL_PIXEL_FORMAT_RGBA_FP16:
         b = 8;
         /* no better nexus format. */
         pf = NEXUS_PixelFormat_eA8_B8_G8_R8;
      break;
      case HAL_PIXEL_FORMAT_RGBA_1010102:
         b = 4;
         pf = NEXUS_PixelFormat_eA8_B8_G8_R8;
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
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         /* no native bm2mc support, return the 'converted for bm2mc consumption'. */
         pf = BM2MC_PACKET_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      case HAL_PIXEL_FORMAT_BLOB:
         pf = BM2MC_PACKET_PixelFormat_eP1;
      break;
      case HAL_PIXEL_FORMAT_RGBA_1010102:
      case HAL_PIXEL_FORMAT_RGBA_FP16:
         /* no native bm2mc support, return something valid. */
         pf = BM2MC_PACKET_PixelFormat_eX8_R8_G8_B8;
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
   BEGL_PixmapInfoEXT bufferRequirements;
#if defined(V3D_VARIANT_v3d)
   BEGL_BufferSettings bufferConstrainedRequirements;
#endif
   int rc = -EINVAL;

   memset(&bufferRequirements, 0, sizeof(BEGL_PixmapInfoEXT));
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
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         bufferRequirements.format = BEGL_BufferFormat_eYV12;
      break;
      case HAL_PIXEL_FORMAT_BLOB:
      case HAL_PIXEL_FORMAT_RGBA_1010102:
      case HAL_PIXEL_FORMAT_RGBA_FP16:
         bufferRequirements.format = BEGL_BufferFormat_INVALID;
      break;
      default:
      break;
   }

   switch (bufferRequirements.format) {
      case BEGL_BufferFormat_INVALID:
         hnd->oglStride = 0;
         hnd->oglFormat = 0;
         hnd->oglSize   = 0;
         if (pSharedData->container.format == HAL_PIXEL_FORMAT_BLOB ||
             pSharedData->container.format == HAL_PIXEL_FORMAT_RGBA_1010102 ||
             pSharedData->container.format == HAL_PIXEL_FORMAT_RGBA_FP16) {
            rc = 0;
         }
      break;
      case BEGL_BufferFormat_eYV12:
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
      case HAL_PIXEL_FORMAT_RGBA_1010102:
      case HAL_PIXEL_FORMAT_RGBA_FP16:
         *pStride = ((w*bpp + (*alignment-1)) & ~(*alignment-1)) / bpp;
         *size = ((w*bpp + (*alignment-1)) & ~(*alignment-1)) * h;
      break;
      case HAL_PIXEL_FORMAT_YV12:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         // force alignment according to (android) format definition.
         *alignment = 16;
         // use y-stride: ALIGN(w, 16)
         *pStride = (w + (*alignment-1)) & ~(*alignment-1);
         // size: y-stride * h + 2 * (c-stride * h/2), with c-stride: ALIGN(y-stride/2, 16)
         *size = (*pStride * h) + 2 * ((h/2) * ((*pStride/2 + (*alignment-1)) & ~(*alignment-1)));
      break;
      case HAL_PIXEL_FORMAT_BLOB:
         *pStride = 1;
         *size = w;
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
   int bpp = 0, err = 0, ret = 0, pdata = -1, sdata = -1;
   NEXUS_Error lrc;
   int size, fmt_align, fmt_set = GR_NONE;
   NEXUS_PixelFormat nxFormat = getNexusPixelFormat(format, &bpp);
   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   void *pMemory;
   struct nx_ashmem_alloc ashmem_alloc;
   struct nx_ashmem_getmem ashmem_getmem;

   private_handle_t *hnd = NULL;
   (void)dev;

   if (nxFormat == NEXUS_PixelFormat_eUnknown) {
      ALOGE("%s: unsupported gr->nx format: %d", __FUNCTION__, format);
      return -EINVAL;
   }

   property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
   strcpy(value2, "/dev/");
   strcat(value2, value);

   pdata = open(value2, O_RDWR, 0);
   if (pdata < 0) {
      err = -EINVAL;
      goto error;
   }
   sdata = open(value2, O_RDWR, 0);
   if (sdata < 0) {
      err = -EINVAL;
      goto error;
   }

   hnd = new private_handle_t(pdata, sdata, 0);
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
   ret = ioctl(hnd->sdata, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
   if (ret < 0) {
      err = -ENOMEM;
      goto error;
   };
   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   ret = ioctl(hnd->sdata, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (ret < 0) {
      err = -ENOMEM;
      goto error;
   } else {
      block_handle = (NEXUS_MemoryBlockHandle)ashmem_getmem.hdl;
   }
   pMemory = NULL;
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (lrc || pSharedData == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
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
      err = -EINVAL;
      goto alloc_failed;
   }

   if (format != HAL_PIXEL_FORMAT_YV12 && format != HAL_PIXEL_FORMAT_YCbCr_420_888) {
      fmt_set |= GR_STANDARD;
      if (format == HAL_PIXEL_FORMAT_BLOB) {
         fmt_set |= GR_BLOB;
      }
      if (format == HAL_PIXEL_FORMAT_RGBA_1010102 || format == HAL_PIXEL_FORMAT_RGBA_FP16) {
         fmt_set |= GR_FP;
      }
   } else if (usage & GRALLOC_USAGE_PROTECTED) {
      fmt_set |= GR_NONE;
   } else if (((format == HAL_PIXEL_FORMAT_YV12) || (format == HAL_PIXEL_FORMAT_YCbCr_420_888))
              && !(usage & GRALLOC_USAGE_PRIVATE_0)) {
      fmt_set |= (GR_YV12|GR_YV12_SW);
   } else if (((format == HAL_PIXEL_FORMAT_YV12) || (format == HAL_PIXEL_FORMAT_YCbCr_420_888))
              && (usage & GRALLOC_USAGE_PRIVATE_0)) {
      if (usage & GRALLOC_USAGE_SW_READ_OFTEN) {
         // cpu will read content, need backing buffer; yv12 plane produced during lock.
         fmt_set |= (GR_YV12|GR_YV12_SW);
      } else if (usage & GRALLOC_USAGE_HW_TEXTURE) {
         fmt_set |= GR_YV12;
      } else {
         fmt_set = GR_NONE;
      }
   }
   if (fmt_set != GR_NONE) {
      memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
      ashmem_alloc.align = gralloc_default_align;
      if ((fmt_set & GR_YV12) == GR_YV12) {
         bool skip_alloc = true;
         if ((fmt_set & GR_YV12_SW) == GR_YV12_SW) {
            if (w <= DATA_PLANE_MAX_WIDTH && h <= DATA_PLANE_MAX_HEIGHT) {
               skip_alloc = false;
            }
         }
         if (usage & GRALLOC_USAGE_HW_TEXTURE) {
            fmt_set |= GR_HWTEX;
            if (skip_alloc) {
               ashmem_alloc.size = 0; // do not allocate yet.
            } else {
               ashmem_alloc.size = pSharedData->container.allocSize;
            }
         } else if ((usage & GRALLOC_USAGE_SW_READ_OFTEN) &&
                    (usage & GRALLOC_USAGE_SW_WRITE_OFTEN)) {
            // note that we do not allow pure sw planes larger than max data planes because
            // we still want to prevent performance bottleneck on composition hw.
            if (!skip_alloc) {
               ashmem_alloc.size = pSharedData->container.allocSize;
            }
         }
      } else {
         if (!((fmt_set & GR_BLOB) || (fmt_set & GR_FP))) {
            pSharedData->container.allocSize = hnd->oglSize;
            pSharedData->container.stride = hnd->oglStride;
         }
         ashmem_alloc.size = pSharedData->container.allocSize;
      }
      if (ashmem_alloc.size > 0) {
         ret = ioctl(hnd->pdata, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
         if (ret >= 0) {
            memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
            ret = ioctl(hnd->pdata, NX_ASHMEM_GETMEM, &ashmem_getmem);
            if (ret < 0) {
               /* give another try after attempting a round of rmlmk, if
                * rmlmk fails (e.g. not enough memory could be freed up),
                * that's game over.
                */
               nxwrap_rmlmk(nxwrap);
               BKNI_Sleep(5); /* give settling time for rmlmk. */
               ret = ioctl(hnd->pdata, NX_ASHMEM_GETMEM, &ashmem_getmem);
            }
            if (ret < 0) {
               err = -ENOMEM;
               goto alloc_failed;
            } else {
               pSharedData->container.block = (NEXUS_MemoryBlockHandle)ashmem_getmem.hdl;
            }
         }
      }
   }
   hnd->fmt_set = fmt_set;

   if ((hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) && pSharedData->container.block) {
      NEXUS_Addr physAddr;
      NEXUS_MemoryBlock_LockOffset(pSharedData->container.block, &physAddr);
      hnd->nxSurfacePhysicalAddress = (uint64_t)physAddr;
      pMemory = NULL;
      NEXUS_MemoryBlock_Lock(pSharedData->container.block, &pMemory);
      hnd->nxSurfaceAddress = (uint64_t)pMemory;
   } else {
      hnd->nxSurfacePhysicalAddress = 0;
      hnd->nxSurfaceAddress = 0;
   }

   if (gralloc_log_mapper()) {
      NEXUS_Addr sPhysAddr, pPhysAddr;
      NEXUS_MemoryBlock_LockOffset(block_handle, &sPhysAddr);
      if (pSharedData->container.block) {NEXUS_MemoryBlock_LockOffset(pSharedData->container.block, &pPhysAddr);}
      else {pPhysAddr = 0;}
      ALOGI("alloc (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x",
            (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
            hnd,
            getpid(),
            block_handle,
            sPhysAddr,
            pSharedData->container.block,
            pPhysAddr,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            hnd->usage,
            pSharedData->container.format);
      NEXUS_MemoryBlock_UnlockOffset(block_handle);
      if (pSharedData->container.block) {NEXUS_MemoryBlock_UnlockOffset(pSharedData->container.block);}
   }

   if ((fmt_set != GR_NONE) && pSharedData->container.block == 0) {
      if (((hnd->fmt_set & GR_YV12) == GR_YV12) &&
          ((hnd->fmt_set & GR_HWTEX) == GR_HWTEX) &&
          !(hnd->fmt_set & GR_YV12_SW)) {
         ALOGI("%s: dropping data plane in favor of hw-texture (%dx%d:%x), size %d", __FUNCTION__,
            w, h, hnd->fmt_set, size);
         if (!(w <= DATA_PLANE_MAX_WIDTH && h <= DATA_PLANE_MAX_HEIGHT)) {
            pSharedData->container.size = 0;
            pSharedData->container.allocSize = 0;
         }
      } else {
         ALOGE("%s: failed to allocate plane (%d,%d), size %d", __FUNCTION__, w, h, size);
         err = -ENOMEM;
         goto alloc_failed;
      }
   } else if (pSharedData->container.block) {
       gralloc_bzero(pSharedData);
   }

   *pHandle = hnd;

   if (block_handle) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   }
   return 0;

alloc_failed:
   if (block_handle) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   }
error:
   *pHandle = NULL;
   if (pdata >= 0) { close(pdata); hnd->pdata = -1; }
   if (sdata >= 0) { close(sdata); hnd->sdata = -1; }
   delete hnd;
   return err;
}

static int
gralloc_free_buffer(alloc_device_t* dev, private_handle_t *hnd)
{
   (void)dev;

   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   NEXUS_Error lrc;

   if (!hnd) {
      return -EINVAL;
   }

   void *pMemory;
   private_handle_t::get_block_handles(hnd, &block_handle, NULL);
   lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(block_handle);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      if (gralloc_log_mapper()) {
         NEXUS_Addr sPhysAddr = 0, pPhysAddr = 0;
         NEXUS_MemoryBlockHandle planeHandle = pSharedData->container.block;
         unsigned planePhysSize = pSharedData->container.size;
         unsigned planeWidth = pSharedData->container.width;
         unsigned planeHeight = pSharedData->container.height;
         unsigned format = pSharedData->container.format;
         NEXUS_MemoryBlock_LockOffset(block_handle, &sPhysAddr);
         if (planeHandle) {NEXUS_MemoryBlock_LockOffset(planeHandle, &pPhysAddr);}
         else {pPhysAddr = 0;}
         ALOGI(" free (%s:%p): owner:%d::s-blk:%p::s-addr:%" PRIu64 "::p-blk:%p::p-addr:%" PRIu64 "::%dx%d::sz:%d::use:0x%x:0x%x",
               (hnd->fmt_set & GR_YV12) == GR_YV12 ? "MM" : (hnd->fmt_set & GR_BLOB) ? "BL" : "ST",
               hnd,
               hnd->pid,
               block_handle,
               sPhysAddr,
               planeHandle,
               pPhysAddr,
               planeWidth,
               planeHeight,
               planePhysSize,
               hnd->usage,
               format);
         NEXUS_MemoryBlock_UnlockOffset(block_handle);
         if (planeHandle) {NEXUS_MemoryBlock_UnlockOffset(planeHandle);}
      }
   }

   if (pSharedData) {
      if (hnd->mgmt_mode == GR_MGMT_MODE_LOCKED) {
         if (pSharedData->container.block) {
            NEXUS_MemoryBlock_UnlockOffset(pSharedData->container.block);
            NEXUS_MemoryBlock_Unlock(pSharedData->container.block);
         }
      }
   }
   if (block_handle) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
   }

   if (hnd->pdata >= 0) { close(hnd->pdata); hnd->pdata = -1; }
   if (hnd->sdata >= 0) { close(hnd->sdata); hnd->sdata = -1; }
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
      ALOGI("alloc: fb::w:%d::h:%d::fmt:%d::usage:0x%08x::hdl:%p", w, h, format, usage, *pHandle);
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

   ALOGD("%s: %s (s-data: %zu)", __FUNCTION__, name, sizeof(SHARED_DATA));

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
