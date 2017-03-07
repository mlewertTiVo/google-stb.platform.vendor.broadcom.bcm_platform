/*
 * Copyright 2016-2017 The Android Open Source Project
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

#define LOG_NDEBUG 0

#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <cutils/properties.h>
#include <cutils/log.h>
#include <inttypes.h>

#include <hardware/gralloc1.h>
#include "gralloc_priv.h"
#include <nx_ashmem.h>
#include <nexus_base_mmap.h>
#include <nexus_platform.h>

#define GR_ASHMEM_PROP "ro.nexus.ashmem.devname"
#define GR_ASHMEM_VAL  "nx_ashmem"
#define GR_LOG_MAP     "ro.gr.log.map"

void __attribute__ ((constructor)) gr1_load(void);
void __attribute__ ((destructor)) gr1_unload(void);

static void * (* dyn_nxwrap_create_client)(void **wrap);
static void (* dyn_nxwrap_destroy_client)(void *wrap);
#define LOAD_FN(lib, name) \
if (!(dyn_ ## name = (typeof(dyn_ ## name)) dlsym(lib, #name))) \
   ALOGV("failed resolving '%s'", #name); \
else \
   ALOGV("resolved '%s' to %p", #name, dyn_ ## name);

static void *gl_dyn_lib;
static void *nxwrap = NULL;
static void *gr1_nx = NULL;
static int gr1_log = 0;
static int gr1_bug_align = 256;
static pthread_mutex_t gr1_lock = PTHREAD_MUTEX_INITIALIZER;
static NEXUS_Graphics2DHandle gr1_gfx = NULL;
static BKNI_EventHandle gr1_event = NULL;

static void gr1_load_lib(
   void) {

   char value[PROPERTY_VALUE_MAX];

   snprintf(value, PROPERTY_VALUE_MAX, "%slibGLES_nexus.so", V3D_DLOPEN_PATH);
   gl_dyn_lib = dlopen(value, RTLD_LAZY | RTLD_LOCAL);
   if (!gl_dyn_lib) {
      snprintf(value, PROPERTY_VALUE_MAX, "/vendor/lib/egl/libGLES_nexus.so");
      gl_dyn_lib = dlopen(value, RTLD_LAZY | RTLD_LOCAL);
      if (!gl_dyn_lib) {
         ALOGE("failed loading essential GLES library '%s': <%s>!", value, dlerror());
      }
   }

   LOAD_FN(gl_dyn_lib, nxwrap_create_client);
   LOAD_FN(gl_dyn_lib, nxwrap_destroy_client);

   if (dyn_nxwrap_create_client) {
      gr1_nx = dyn_nxwrap_create_client(&nxwrap);
   }

   gr1_log = property_get_bool(GR_LOG_MAP, 1);
}

static void gr1_gfx_cb(
   void *pParam,
   int param) {

   (void)param;
   BKNI_SetEvent((BKNI_EventHandle)pParam);
}


void gr1_load(
   void) {
   NEXUS_Error rc;

   gr1_load_lib();

   rc = BKNI_CreateEvent(&gr1_event);
   if (!rc) {
      NEXUS_Graphics2DOpenSettings g2dOpenSettings;
      NEXUS_Graphics2D_GetDefaultOpenSettings(&g2dOpenSettings);
      g2dOpenSettings.compatibleWithSurfaceCompaction = false;
      gr1_gfx = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, &g2dOpenSettings);
      if (gr1_gfx) {
         NEXUS_Graphics2DSettings gfxSettings;
         NEXUS_Graphics2D_GetSettings(gr1_gfx, &gfxSettings);
         gfxSettings.pollingCheckpoint = false;
         gfxSettings.checkpointCallback.callback = gr1_gfx_cb;
         gfxSettings.checkpointCallback.context = (void *)gr1_event;
         rc = NEXUS_Graphics2D_SetSettings(gr1_gfx, &gfxSettings);
         if (rc) {
            NEXUS_Graphics2D_Close(gr1_gfx);
            gr1_gfx = NULL;
            BKNI_DestroyEvent(gr1_event);
            gr1_event = NULL;
         }
      }
   }
}

void gr1_unload(
   void) {

   if (nxwrap && dyn_nxwrap_destroy_client) {
      dyn_nxwrap_destroy_client(nxwrap);
      gr1_nx = NULL;
      nxwrap = NULL;
   }

   dlclose(gl_dyn_lib);

   if (gr1_gfx) {
      NEXUS_Graphics2D_Close(gr1_gfx);
      gr1_gfx = NULL;
   }
   if (gr1_event) {
      BKNI_DestroyEvent(gr1_event);
      gr1_event = NULL;
   }
}

static int32_t gr1_dump(
   gralloc1_device_t* device,
   uint32_t* outSize,
   char* outBuffer) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   if (outSize != NULL) {
      *outSize = 0;
   }

   // TODO: dump local view of gralloc buffers?
   (void)outBuffer;

out:
   return ret;
}

static void gr1_getCaps(
   struct gralloc1_device* device,
   uint32_t* outCount,
   int32_t* /*gralloc1_capability_t*/ outCapabilities) {

   if (device == NULL) {
      return;
   }

   if (outCount != NULL) {
      *outCount = 0;
   }
   if (outCapabilities != NULL) {
      // TODO: outCapabilities[0] = GRALLOC1_CAPABILITY_TEST_ALLOCATE;
   }
   return;
}

static int32_t gr1_debDesc(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t* outDescriptor) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   int rc = -1;
   int sdata = -1;
   NEXUS_Error lrc;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   struct nx_ashmem_alloc ashmem_alloc;
   struct nx_ashmem_getmem ashmem_getmem;
   void *pMemory;

   ALOGV("-> %s \n",
      __FUNCTION__);

   if (device == NULL || outDescriptor == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   property_get(GR_ASHMEM_PROP, value, GR_ASHMEM_VAL);
   strcpy(value2, "/dev/");
   strcat(value2, value);
   sdata = open(value2, O_RDWR, 0);
   if (sdata < 0) {
      goto error;
   }
   memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
   ashmem_alloc.size  = sizeof(SHARED_DATA);
   ashmem_alloc.align = 4096;
   rc = ioctl(sdata, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
   if (rc < 0) {
      goto error;
   }
   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(sdata, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      goto error;
   } else {
      dstore_hdl = (NEXUS_MemoryBlockHandle)ashmem_getmem.hdl;
   }
   pMemory = NULL;
   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (lrc || pSharedData == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      goto error;
   }
   memset(pSharedData, 0, sizeof(SHARED_DATA));
   pSharedData->container.dhdl = sdata;
   if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);

   *outDescriptor = (gralloc1_buffer_descriptor_t)(intptr_t)dstore_hdl;

   if (gr1_log) {
      NEXUS_Addr dstorePhys;
      NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
      ALOGI("cds: o:%d::s-blk:%p::s-addr:%" PRIu64 "",
            getpid(),
            dstore_hdl,
            dstorePhys);
      NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
   }
   goto out;

error:
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }
   if (sdata >= 0) {
      close(sdata);
   }
   ret = GRALLOC1_ERROR_NO_RESOURCES;
out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, *outDescriptor, ret);
   return ret;
}

static int32_t gr1_finDesc(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      if (gr1_log) {
         NEXUS_Addr dstorePhys;
         NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
         ALOGI("kds: o:%d::s-blk:%p::s-addr:%" PRIu64 "",
               getpid(),
               dstore_hdl,
               dstorePhys);
         NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
      }
      if (pSharedData->container.dhdl >= 0) {
          close(pSharedData->container.dhdl);
      }
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      dstore_hdl = NULL;
   } else {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_setDesc(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor,
   uint64_t /*gralloc1_consumer_usage_t*/ usage) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      pSharedData->container.cUsage = usage;
   } else {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_setDims(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor,
   uint32_t width,
   uint32_t height) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      pSharedData->container.width = width;
      pSharedData->container.height = height;
   } else {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_fmtSupp(
   int32_t format) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;

   switch(format) {
   case HAL_PIXEL_FORMAT_RGBA_8888:
   case HAL_PIXEL_FORMAT_RGBX_8888:
   case HAL_PIXEL_FORMAT_RGB_888:
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
   case HAL_PIXEL_FORMAT_RGB_565:
   case HAL_PIXEL_FORMAT_YV12:
   case HAL_PIXEL_FORMAT_YCbCr_420_888:
   break;
   default:
      ALOGE("unsupported descriptor format: %" PRId32 "", format);
      ret = GRALLOC1_ERROR_BAD_VALUE;
   }
   return ret;
}

static void gr1_fmt2PrivFmt(
   int32_t fmt,
   NEXUS_PixelFormat *nxfmt,
   int *bpp)
{
   switch (fmt) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
         *bpp = 4;
         *nxfmt = NEXUS_PixelFormat_eA8_B8_G8_R8;
      break;
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
         *bpp = 4;
         *nxfmt = NEXUS_PixelFormat_eX8_R8_G8_B8;
      break;
      case HAL_PIXEL_FORMAT_RGB_565:
         *bpp = 2;
         *nxfmt = NEXUS_PixelFormat_eR5_G6_B5;
      break;
      case HAL_PIXEL_FORMAT_YV12:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         *bpp = 2;
         *nxfmt = NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      /* hmm, we would have already rejected the descriptor earlier on. */
      default:
         *bpp = 0;
         *nxfmt = NEXUS_PixelFormat_eUnknown;
         ALOGE("unsupported format: %" PRId32 "", fmt);
      break;
   }
}

static void gr1_getLayout(
   uint32_t width,
   uint32_t height,
   int32_t format,
   int bpp,
   uint32_t *alignment,
   uint32_t *stride,
   uint32_t *size)
{
   switch (format) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      case HAL_PIXEL_FORMAT_RGB_565:
         *alignment = 1;
         *stride = ((width*bpp + (*alignment-1)) & ~(*alignment-1));
         *size = ((width*bpp + (*alignment-1)) & ~(*alignment-1)) * height;
      break;
      case HAL_PIXEL_FORMAT_YV12:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         *alignment = 16;
         *stride = (width + (*alignment-1)) & ~(*alignment-1);
         *size = (*stride * height) + 2 * ((height/2) * ((*stride/2 + (*alignment-1)) & ~(*alignment-1)));
      break;
      default:
      break;
   }

   ALOGV("[layout]:%ux%u:%d:%d => %u:%u:%u", width, height, bpp, format, *alignment, *stride, *size);
}

static int32_t gr1_setFmt(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor,
   int32_t format) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   if (gr1_fmtSupp(format) != GRALLOC1_ERROR_NONE) {
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      pSharedData->container.format = format;
   } else {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_setProd(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor,
   uint64_t /*gralloc1_producer_usage_t*/ usage) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      pSharedData->container.pUsage = usage;
   } else {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_getBack(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   gralloc1_backing_store_t* outStore) {
   int rc;

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle pstore_hdl = NULL;
   gr1_priv_t const *hnd = NULL;
   struct nx_ashmem_getmem ashmem_getmem;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;

   if (pstore_hdl == NULL) {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   } else {
      *outStore = (gralloc1_backing_store_t)pstore_hdl;
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_getCons(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint64_t* /*gralloc1_consumer_usage_t*/ outUsage) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outUsage = pSharedData->container.cUsage;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_getDims(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint32_t* outWidth,
   uint32_t* outHeight) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outWidth = pSharedData->container.width;
      *outHeight = pSharedData->container.height;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_getFmt(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   int32_t* outFormat) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outFormat = pSharedData->container.format;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_getProd(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint64_t* /*gralloc1_producer_usage_t*/ outUsage) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outUsage = pSharedData->container.pUsage;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_getStrd(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint32_t* outStride) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outStride = pSharedData->container.stride;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static bool gr1_needPstore(
   int32_t format,
   uint64_t cUsage,
   uint64_t pUsage) {

   bool ret = true;

   if (pUsage & GRALLOC1_PRODUCER_USAGE_PROTECTED) {
      ret = false;
   }

   if (((format == HAL_PIXEL_FORMAT_YV12) ||
        (format == HAL_PIXEL_FORMAT_YCbCr_420_888)) &&
        (cUsage & GRALLOC1_CONSUMER_USAGE_PRIVATE_0)) {
      ret = false;
      if ((cUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN) ||
          (cUsage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE)) {
         ret = true;
      }
   }

   return ret;
}

static BM2MC_PACKET_PixelFormat gr1_m2mcFmt(
   int gr1Fmt) {
   BM2MC_PACKET_PixelFormat m2mcFmt;
   switch (gr1Fmt) {
      case HAL_PIXEL_FORMAT_RGBA_8888:
         m2mcFmt = BM2MC_PACKET_PixelFormat_eA8_B8_G8_R8;
      break;
      case HAL_PIXEL_FORMAT_RGBX_8888:
      case HAL_PIXEL_FORMAT_RGB_888:
      case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
         m2mcFmt = BM2MC_PACKET_PixelFormat_eX8_R8_G8_B8;
      break;
      case HAL_PIXEL_FORMAT_RGB_565:
         m2mcFmt = BM2MC_PACKET_PixelFormat_eR5_G6_B5;
      break;
      case HAL_PIXEL_FORMAT_YV12:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         m2mcFmt = BM2MC_PACKET_PixelFormat_eY08_Cb8_Y18_Cr8;
      break;
      default:
         m2mcFmt = BM2MC_PACKET_PixelFormat_eUnknown;
      break;
   }
   return m2mcFmt;
}

static void gr1_bzero(
   PSHARED_DATA pSharedData,
   NEXUS_MemoryBlockHandle pstore_hdl) {

   bool done = false;
   NEXUS_Addr physAddr;
   void *pMemory = NULL;
   NEXUS_Error lrco, lrc;
   static const BM2MC_PACKET_Blend copyColor = {BM2MC_PACKET_BlendFactor_eSourceColor, BM2MC_PACKET_BlendFactor_eOne, false,
      BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};
   static const BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eSourceAlpha, BM2MC_PACKET_BlendFactor_eOne, false,
      BM2MC_PACKET_BlendFactor_eZero, BM2MC_PACKET_BlendFactor_eZero, false, BM2MC_PACKET_BlendFactor_eZero};

   lrco = NEXUS_MemoryBlock_LockOffset(pstore_hdl, &physAddr);
   lrc  = NEXUS_MemoryBlock_Lock(pstore_hdl, &pMemory);

   if (lrc || lrco || pMemory == NULL) {
      if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(pstore_hdl);
      goto out;
   }
   if (gr1_gfx && gr1_event) {
      NEXUS_Error errCode;
      size_t pktSize;
      void *pktBuffer, *next;

      pthread_mutex_lock(&gr1_lock);
      switch (pSharedData->container.format) {
      case HAL_PIXEL_FORMAT_YV12:
      case HAL_PIXEL_FORMAT_YCbCr_420_888:
         errCode = 0;
      break;
      default:
         errCode = NEXUS_Graphics2D_GetPacketBuffer(gr1_gfx, &pktBuffer, &pktSize, 1024);
         if (errCode == 0) {
            next = pktBuffer;
            {
               BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
               BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
               pPacket->plane.address = physAddr;
               pPacket->plane.pitch   = pSharedData->container.stride;
               pPacket->plane.format  = gr1_m2mcFmt(pSharedData->container.format);
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
            errCode = NEXUS_Graphics2D_PacketWriteComplete(gr1_gfx, (uint8_t*)next - (uint8_t*)pktBuffer);
            if (errCode == 0) {
               errCode = NEXUS_Graphics2D_Checkpoint(gr1_gfx, NULL);
               if (errCode == NEXUS_GRAPHICS2D_QUEUED) {
                  BKNI_WaitForEvent(gr1_event, CHECKPOINT_TIMEOUT);
               }
            }
         }
         break;
      }
      pthread_mutex_unlock(&gr1_lock);
      if (!errCode) {
         done = true;
      }
   }

   if (!done) {
      bzero(pMemory, pSharedData->container.size);
   }

   if (!lrco) NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   if (!lrc)  NEXUS_MemoryBlock_Unlock(pstore_hdl);

out:
    return;
}

static buffer_handle_t gr1_allocOneBuf(
   const gralloc1_buffer_descriptor_t descriptor,
   gralloc1_error_t *ret) {

   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   NEXUS_MemoryBlockHandle pstore_hdl = NULL;
   gr1_priv_t *hnd = NULL;
   char value[PROPERTY_VALUE_MAX];
   char value2[PROPERTY_VALUE_MAX];
   struct nx_ashmem_alloc ashmem_alloc;
   struct nx_ashmem_getmem ashmem_getmem;
   struct nx_ashmem_refcnt ashmem_refcnt;
   int pdata, rc;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   NEXUS_Addr physAddr;
   buffer_handle_t buffer = NULL;

   *ret = GRALLOC1_ERROR_NONE;

   hnd = new gr1_priv_t(descriptor);
   if (hnd == NULL) {
      *ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   property_get(GR_ASHMEM_PROP, value, GR_ASHMEM_VAL);
   strcpy(value2, "/dev/");
   strcat(value2, value);
   pdata = open(value2, O_RDWR, 0);
   if (pdata < 0) {
      *ret = GRALLOC1_ERROR_NO_RESOURCES;
      goto error;
   }
   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      NEXUS_PixelFormat nxfmt;
      gr1_fmt2PrivFmt(pSharedData->container.format, &nxfmt, &pSharedData->container.bpp);
      if (nxfmt == NEXUS_PixelFormat_eUnknown) {
         if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
         *ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
         goto error;
      }
      gr1_getLayout(pSharedData->container.width, pSharedData->container.height,
                    pSharedData->container.format, pSharedData->container.bpp,
                    &pSharedData->container.alignment, &pSharedData->container.stride,
                    &pSharedData->container.size);
      if (!pSharedData->container.stride || !pSharedData->container.size) {
         if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
         *ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
         goto error;
      }
      if (gr1_needPstore(pSharedData->container.format,
                         pSharedData->container.cUsage,
                         pSharedData->container.pUsage )) {
         memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
         ashmem_alloc.align = gr1_bug_align;
         ashmem_alloc.size  = pSharedData->container.size;
         rc = ioctl(pdata, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
         if (rc >= 0) {
            memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
            rc = ioctl(pdata, NX_ASHMEM_GETMEM, &ashmem_getmem);
         }
         if (rc >= 0) {
            memset(&ashmem_refcnt, 0, sizeof(struct nx_ashmem_refcnt));
            ashmem_refcnt.hdl = ashmem_getmem.hdl;
            ashmem_refcnt.cnt = NX_ASHMEM_REFCNT_ADD;
            rc = ioctl(pdata, NX_ASHMEM_REFCNT, &ashmem_refcnt);
         }
         if (rc < 0) {
            *ret = GRALLOC1_ERROR_NO_RESOURCES;
            goto error;
         }

         pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;
         pSharedData->container.block = pstore_hdl;
         gr1_bzero(pSharedData, pstore_hdl);

         hnd->pfhdl     = pdata;
         hnd->sfhdl     = pSharedData->container.dhdl;
         pSharedData->container.dhdl = -1;
         hnd->oglStride = pSharedData->container.stride;
         hnd->oglSize   = pSharedData->container.size;
         /* gpu cache - default lock. */
         NEXUS_MemoryBlock_LockOffset(pstore_hdl, &physAddr);
         hnd->nxSurfacePhysicalAddress = (uint64_t)physAddr;
         pMemory = NULL;
         NEXUS_MemoryBlock_Lock(pstore_hdl, &pMemory);
         hnd->nxSurfaceAddress = (uint64_t)pMemory;

      }
   } else {
      *ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }

   if (gr1_log) {
      NEXUS_Addr dstorePhys, pstorePhys;
      NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
      NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
      ALOGI("add: o:%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "::%dx%d::sz:%d::f:0x%x",
            getpid(),
            dstore_hdl,
            dstorePhys,
            pstore_hdl,
            pstorePhys,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            pSharedData->container.format);
      NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   }

   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

   if (*ret == GRALLOC1_ERROR_NONE) {
      buffer = (buffer_handle_t)(intptr_t)hnd;
   } else {
      buffer = NULL;
   }
   goto out;

error:
   if (hnd) {
      delete hnd;
   }
   if (pdata >= 0) {
      close(pdata);
   }
out:
   return buffer;
}

static int32_t gr1_allocBuf(
   gralloc1_device_t* device,
   uint32_t numDescriptors,
   const gralloc1_buffer_descriptor_t* descriptors,
   buffer_handle_t* outBuffers) {

   uint32_t i, j;
   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   buffer_handle_t buffer = NULL;

   ALOGV("-> %s\n",
      __FUNCTION__);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }
   if (outBuffers == NULL) {
      ret = GRALLOC1_ERROR_NOT_SHARED;
      goto out;
   }

   for (i = 0 ; i < numDescriptors ; i++) {
      buffer = gr1_allocOneBuf(*(descriptors+i), &ret);
      if (buffer == NULL) {
         for (j = 0 ; j < i ; j++) {
            // TODO gr1_relOneBuf(outBuffers[j]);
         }
         goto out;
      } else {
         *(outBuffers+i) = buffer;
         ALOGI("abuf: desc:%" PRIu64 ":alloc:%p",
               *(descriptors+i), *(outBuffers+i));
      }
   }

   if (numDescriptors > 0) {
      ret = GRALLOC1_ERROR_NOT_SHARED;
   }

out:
   ALOGE_IF(!((ret==GRALLOC1_ERROR_NONE)||(ret==GRALLOC1_ERROR_NOT_SHARED)),
            "<- %s: (%d)\n", __FUNCTION__, ret);
   return ret;
}

static int32_t gr1_retBuf(
   gralloc1_device_t* device,
   buffer_handle_t buffer) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   int rc;
   struct nx_ashmem_getmem ashmem_getmem;
   struct nx_ashmem_refcnt ashmem_refcnt;
   NEXUS_MemoryBlockHandle dstore_hdl, pstore_hdl;
   gr1_priv_t *hnd = NULL;
   void *pMemory = NULL;
   NEXUS_Addr physAddr;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = (gr1_priv_t *)buffer;

   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   memset(&ashmem_refcnt, 0, sizeof(struct nx_ashmem_refcnt));
   ashmem_refcnt.hdl = ashmem_getmem.hdl;
   ashmem_refcnt.cnt = NX_ASHMEM_REFCNT_ADD;
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_REFCNT, &ashmem_refcnt);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_NO_RESOURCES;
      goto out;
   }
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;
   pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;

   /* gpu cache - reinit lock (may be different process). */
   NEXUS_MemoryBlock_LockOffset(pstore_hdl, &physAddr);
   hnd->nxSurfacePhysicalAddress = (uint64_t)physAddr;
   NEXUS_MemoryBlock_Lock(pstore_hdl, &pMemory);
   hnd->nxSurfaceAddress = (uint64_t)pMemory;

   if (gr1_log) {
      NEXUS_Addr dstorePhys, pstorePhys;
      NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
      NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
      ALOGI("ret: o:%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "",
            getpid(),
            dstore_hdl,
            dstorePhys,
            pstore_hdl,
            pstorePhys);
      NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_relBuf(
   gralloc1_device_t* device,
   buffer_handle_t buffer) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl, pstore_hdl;
   int rc;
   struct nx_ashmem_getmem ashmem_getmem;
   struct nx_ashmem_refcnt ashmem_refcnt;
   gr1_priv_t *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = (gr1_priv_t *)buffer;

   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   memset(&ashmem_refcnt, 0, sizeof(struct nx_ashmem_refcnt));
   ashmem_refcnt.hdl = ashmem_getmem.hdl;
   ashmem_refcnt.cnt = NX_ASHMEM_REFCNT_REM;
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_REFCNT, &ashmem_refcnt);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;
   pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;

   if (ashmem_refcnt.rel) {
      if (gr1_log) {
         NEXUS_Addr dstorePhys, pstorePhys;
         NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
         NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
         ALOGI("rem: o:%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "",
               getpid(),
               dstore_hdl,
               dstorePhys,
               pstore_hdl,
               pstorePhys);
         NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
         NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
      }
      NEXUS_MemoryBlock_Unlock(pstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
      NEXUS_MemoryBlock_CheckIfLocked(pstore_hdl);
      close(hnd->pfhdl);
      close(hnd->sfhdl);
      delete hnd;
   } else {
      if (gr1_log) {
         NEXUS_Addr dstorePhys, pstorePhys;
         NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
         NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
         ALOGI("rel: o:%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "",
               getpid(),
               dstore_hdl,
               dstorePhys,
               pstore_hdl,
               pstorePhys);
         NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
         NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
      }
      NEXUS_MemoryBlock_Unlock(pstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_numPlanes(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint32_t* outNumPlanes) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   gr1_priv_t const *hnd = NULL;
   NEXUS_MemoryBlockHandle dstore_hdl;
   void *pMemory = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error dlrc;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);

   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;
   dlrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (dlrc || pSharedData == NULL) {
      if (dlrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }

	if (pSharedData->container.format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
      *outNumPlanes = 3;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }

   if (dstore_hdl) {
      if (!dlrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_lckBuf(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint64_t /*gralloc1_producer_usage_t*/ producerUsage,
   uint64_t /*gralloc1_consumer_usage_t*/ consumerUsage,
   const gralloc1_rect_t* accessRegion,
   void** outData,
   int32_t acquireFence) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_Error dlrc, plrc;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL, pstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   void *pMemory;
   struct nx_ashmem_getmem ashmem_getmem;
   int rc;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);

   if ((producerUsage == GRALLOC1_PRODUCER_USAGE_NONE) &&
       (consumerUsage == GRALLOC1_CONSUMER_USAGE_NONE)) {
      ALOGE("invalid usage lock combination");
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((producerUsage == GRALLOC1_PRODUCER_USAGE_NONE) &&
        !(consumerUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN)) {
      ALOGE("invalid consumer usage lock combination %" PRIx64 "", consumerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((consumerUsage == GRALLOC1_CONSUMER_USAGE_NONE) &&
        !(producerUsage & (GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN|GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN))) {
      ALOGE("invalid producer usage lock combination %" PRIx64 "", producerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((consumerUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN) &&
       (producerUsage & (GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN|GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN))) {
      ALOGE("invalid consumer/producer usage lock combination %" PRIx64 ":%" PRIx64 "", consumerUsage, producerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }

   /* TODO: make use of those. */
   (void)accessRegion;
   (void)acquireFence;

   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;
   dlrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (dlrc || pSharedData == NULL) {
      if (dlrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;

   if (producerUsage & GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN) {
      pSharedData->container.wlock = 1;
   }

   plrc = NEXUS_MemoryBlock_Lock(pstore_hdl, outData);
   if (plrc || *outData == NULL) {
      if (!dlrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      ret = GRALLOC1_ERROR_NO_RESOURCES;
      goto out;
   }
   NEXUS_FlushCache(*outData, pSharedData->container.size);

   if (gr1_log) {
      NEXUS_Addr dstorePhys, pstorePhys;
      NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
      NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
      ALOGI("lck: o::%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "::%dx%d::sz:%d::f:0x%x::out:%p::act:%d",
            hnd->pid,
            dstore_hdl,
            dstorePhys,
            pstore_hdl,
            pstorePhys,
            pSharedData->container.width,
            pSharedData->container.height,
            pSharedData->container.size,
            pSharedData->container.format,
            *outData,
            getpid());
      NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   }

   if (!dlrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_lckFlex(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint64_t /*gralloc1_producer_usage_t*/ producerUsage,
   uint64_t /*gralloc1_consumer_usage_t*/ consumerUsage,
   const gralloc1_rect_t* accessRegion,
   struct android_flex_layout* outFlexLayout,
   int32_t acquireFence) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);

   if ((producerUsage == GRALLOC1_PRODUCER_USAGE_NONE) &&
       (consumerUsage == GRALLOC1_CONSUMER_USAGE_NONE)) {
      ALOGE("invalid usage lock-flex combination");
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((producerUsage == GRALLOC1_PRODUCER_USAGE_NONE) &&
        !(consumerUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN)) {
      ALOGE("invalid consumer usage lock-flex combination %" PRIx64 "", consumerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((consumerUsage == GRALLOC1_CONSUMER_USAGE_NONE) &&
        !(producerUsage & (GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN|GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN))) {
      ALOGE("invalid producer usage lock-flex combination %" PRIx64 "", producerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }
   if ((consumerUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN) &&
       (producerUsage & (GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN|GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN))) {
      ALOGE("invalid consumer/producer usage lock-flex combination %" PRIx64 ":%" PRIx64 "", consumerUsage, producerUsage);
      ret = GRALLOC1_ERROR_BAD_VALUE;
      goto out;
   }

   /* need definition of android_flex_layout. */
   ret = GRALLOC1_ERROR_UNSUPPORTED;
   /* TODO: make use of those. */
   (void)accessRegion;
   (void)outFlexLayout;
   (void)acquireFence;

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_ulckBuf(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   int32_t* outReleaseFence) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   gr1_priv_t const *hnd = NULL;
   NEXUS_Error dlrc, plrc;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL, pstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   void *pMemory;
   struct nx_ashmem_getmem ashmem_getmem;
   int rc;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);

   /* TODO: consider outReleaseFence. */
   (void)outReleaseFence;

   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;
   dlrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   pSharedData = (PSHARED_DATA) pMemory;
   if (dlrc || pSharedData == NULL) {
      if (dlrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
   rc = ioctl(hnd->pfhdl, NX_ASHMEM_GETMEM, &ashmem_getmem);
   if (rc < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   pstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)ashmem_getmem.hdl;

   if ((pSharedData->container.pUsage & GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN) &&
       pSharedData->container.wlock) {
      plrc = NEXUS_MemoryBlock_Lock(pstore_hdl, &pMemory);
      if (plrc || pMemory == NULL) {
         if (plrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(pstore_hdl);
      } else {
         NEXUS_FlushCache(pMemory, pSharedData->container.size);
         NEXUS_MemoryBlock_Unlock(pstore_hdl);
      }
      pSharedData->container.wlock = 0;
   }

   if (gr1_log) {
      NEXUS_Addr dstorePhys, pstorePhys;
      NEXUS_MemoryBlock_LockOffset(dstore_hdl, &dstorePhys);
      NEXUS_MemoryBlock_LockOffset(pstore_hdl, &pstorePhys);
      ALOGI("ulk: o::%d::s-blk:%p::s-addr:%" PRIx64 "::p-blk:%p::p-addr:%" PRIx64 "",
            hnd->pid,
            dstore_hdl,
            dstorePhys,
            pstore_hdl,
            pstorePhys);
      NEXUS_MemoryBlock_UnlockOffset(dstore_hdl);
      NEXUS_MemoryBlock_UnlockOffset(pstore_hdl);
   }

   NEXUS_MemoryBlock_Unlock(pstore_hdl);
   if (!dlrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

static int32_t gr1_setLyrCnt(
   gralloc1_device_t* device,
   gralloc1_buffer_descriptor_t descriptor,
   uint32_t layerCount) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = (NEXUS_MemoryBlockHandle)descriptor;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, descriptor);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
      goto out;
   }

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (!pSharedData) {
      ret = GRALLOC1_ERROR_BAD_DESCRIPTOR;
   }
   if (layerCount != GR1_SINGLE_LAYER_DESCRIPTOR) {
      ret = GRALLOC1_ERROR_BAD_VALUE;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, descriptor, ret);
   return ret;
}

static int32_t gr1_getLyrCnt(
   gralloc1_device_t* device,
   buffer_handle_t buffer,
   uint32_t* outLayerCount) {

   gralloc1_error_t ret = GRALLOC1_ERROR_NONE;
   NEXUS_MemoryBlockHandle dstore_hdl = NULL;
   PSHARED_DATA pSharedData = NULL;
   NEXUS_Error lrc;
   void *pMemory;
   gr1_priv_t const *hnd = NULL;

   ALOGV("-> %s:%" PRIu64 "\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer);

   if (device == NULL) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   if (gr1_priv_t::validate(buffer) < 0) {
      ret = GRALLOC1_ERROR_BAD_HANDLE;
      goto out;
   }
   hnd = reinterpret_cast<gr1_priv_t const*>(buffer);
   dstore_hdl = (NEXUS_MemoryBlockHandle)(intptr_t)hnd->descriptor;

   lrc = NEXUS_MemoryBlock_Lock(dstore_hdl, &pMemory);
   if (lrc == BERR_NOT_SUPPORTED) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   pSharedData = (PSHARED_DATA) pMemory;
   if (pSharedData) {
      *outLayerCount = GR1_SINGLE_LAYER_DESCRIPTOR;
   } else {
      ret = GRALLOC1_ERROR_UNSUPPORTED;
   }
   if (dstore_hdl) {
      if (!lrc) NEXUS_MemoryBlock_Unlock(dstore_hdl);
   }

out:
   ALOGE_IF((ret!=GRALLOC1_ERROR_NONE),"<- %s:%" PRIu64 " (%d)\n",
      __FUNCTION__, (uint64_t)(intptr_t)buffer, ret);
   return ret;
}

typedef struct gr1_fncElem {
   gralloc1_function_descriptor_t desc;
   gralloc1_function_pointer_t func;
} gr1_fncElem_t;

static gr1_fncElem_t gr1_fncsMap[] = {
   {GRALLOC1_FUNCTION_DUMP,                (gralloc1_function_pointer_t)&gr1_dump},
   {GRALLOC1_FUNCTION_CREATE_DESCRIPTOR,   (gralloc1_function_pointer_t)&gr1_debDesc},
   {GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR,  (gralloc1_function_pointer_t)&gr1_finDesc},
   {GRALLOC1_FUNCTION_SET_CONSUMER_USAGE,  (gralloc1_function_pointer_t)&gr1_setDesc},
   {GRALLOC1_FUNCTION_SET_DIMENSIONS,      (gralloc1_function_pointer_t)&gr1_setDims},
   {GRALLOC1_FUNCTION_SET_FORMAT,          (gralloc1_function_pointer_t)&gr1_setFmt},
   {GRALLOC1_FUNCTION_SET_PRODUCER_USAGE,  (gralloc1_function_pointer_t)&gr1_setProd},
   {GRALLOC1_FUNCTION_GET_BACKING_STORE,   (gralloc1_function_pointer_t)&gr1_getBack},
   {GRALLOC1_FUNCTION_GET_CONSUMER_USAGE,  (gralloc1_function_pointer_t)&gr1_getCons},
   {GRALLOC1_FUNCTION_GET_DIMENSIONS,      (gralloc1_function_pointer_t)&gr1_getDims},
   {GRALLOC1_FUNCTION_GET_FORMAT,          (gralloc1_function_pointer_t)&gr1_getFmt},
   {GRALLOC1_FUNCTION_GET_PRODUCER_USAGE,  (gralloc1_function_pointer_t)&gr1_getProd},
   {GRALLOC1_FUNCTION_GET_STRIDE,          (gralloc1_function_pointer_t)&gr1_getStrd},
   {GRALLOC1_FUNCTION_ALLOCATE,            (gralloc1_function_pointer_t)&gr1_allocBuf},
   {GRALLOC1_FUNCTION_RETAIN,              (gralloc1_function_pointer_t)&gr1_retBuf},
   {GRALLOC1_FUNCTION_RELEASE,             (gralloc1_function_pointer_t)&gr1_relBuf},
   {GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES, (gralloc1_function_pointer_t)&gr1_numPlanes},
   {GRALLOC1_FUNCTION_LOCK,                (gralloc1_function_pointer_t)&gr1_lckBuf},
   {GRALLOC1_FUNCTION_LOCK_FLEX,           (gralloc1_function_pointer_t)&gr1_lckFlex},
   {GRALLOC1_FUNCTION_UNLOCK,              (gralloc1_function_pointer_t)&gr1_ulckBuf},
   {GRALLOC1_FUNCTION_SET_LAYER_COUNT,     (gralloc1_function_pointer_t)&gr1_setLyrCnt},
   {GRALLOC1_FUNCTION_GET_LAYER_COUNT,     (gralloc1_function_pointer_t)&gr1_getLyrCnt},

   {GRALLOC1_FUNCTION_INVALID,             (gralloc1_function_pointer_t)NULL},
};

static gralloc1_function_pointer_t gr1_getFncs(
   struct gralloc1_device* device,
   int32_t /*gralloc1_function_descriptor_t*/ descriptor) {

   int32_t i = 0;

   if (device == NULL) {
      return NULL;
   }

   while (gr1_fncsMap[i].desc != GRALLOC1_FUNCTION_INVALID) {
      if (gr1_fncsMap[i].desc == descriptor) {
         return gr1_fncsMap[i].func;
      }
      i++;
   }

   return NULL;
}

static int gr1_close(
   struct hw_device_t *dev) {
   gralloc1_device_t* device = reinterpret_cast<gralloc1_device_t *>(dev);
   if (device) {
      free(device);
   }
   return 0;
}

static int gr1_open(
   const struct hw_module_t* module,
   const char* name,
   struct hw_device_t** device) {
   if (!strcmp(name, GRALLOC_HARDWARE_MODULE_ID)) {
      gralloc1_device_t *dev;
      dev = (gralloc1_device_t*)malloc(sizeof(*dev));
      memset(dev, 0, sizeof(*dev));

      dev->common.tag      = HARDWARE_DEVICE_TAG;
      dev->common.version  = 0;
      dev->common.module   = const_cast<hw_module_t*>(module);
      dev->common.close    = gr1_close;
      dev->getCapabilities = gr1_getCaps;
      dev->getFunction     = gr1_getFncs;

      *device = &dev->common;
      return 0;
   } else {
      return -EINVAL;
   }
}

static struct hw_module_methods_t gr1_mod_fncs = {
   .open  = gr1_open,
};

typedef struct gr1_module {
   struct hw_module_t common;
} gr1_module_t;

gr1_module_t HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = GRALLOC_MODULE_API_VERSION_1_0,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = GRALLOC_HARDWARE_MODULE_ID,
      .name               = "gralloc1 for set-top-box platforms",
      .author             = "Broadcom",
      .methods            = &gr1_mod_fncs,
      .dso                = 0,
      .reserved           = {0}
   }
};
