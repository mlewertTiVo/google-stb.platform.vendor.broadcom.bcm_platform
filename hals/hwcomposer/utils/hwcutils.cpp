/*
 * Copyright (C) 2014 Broadcom Canada Ltd.
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

#define LOG_TAG "bcm-hwc-utils"

#include <cutils/log.h>
#include <fcntl.h>
#include <string.h>

#include "nexus_surface_client.h"
#include "nxclient.h"
#include "nx_ashmem.h"

extern "C" NEXUS_SurfaceHandle hwc_to_nsc_surface(
    int width, int height, int stride, NEXUS_PixelFormat format,
    NEXUS_MemoryBlockHandle handle, unsigned offset)
{
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat   = format;
    createSettings.width         = width;
    createSettings.height        = height;
    createSettings.pitch         = stride;
    createSettings.managedAccess = false;

    if (handle) {
        createSettings.pixelMemory = (NEXUS_MemoryBlockHandle) handle;
        createSettings.pixelMemoryOffset = offset;
    }

    return NEXUS_Surface_Create(&createSettings);
}

extern "C" NEXUS_SurfaceHandle hwc_surface_create(
   const NEXUS_SurfaceCreateSettings *pCreateSettings,
   bool dynamic_heap)
{
   NEXUS_SurfaceHandle surface = NULL;

   surface = NEXUS_Surface_Create(pCreateSettings);
   if ((surface == NULL) && dynamic_heap) {
      if (NxClient_GrowHeap(NXCLIENT_DYNAMIC_HEAP) == NEXUS_SUCCESS) {
         surface = NEXUS_Surface_Create(pCreateSettings);
      }
   }

   if (surface == NULL) {
      ALOGE("%s: oom (%s) - size:%dx%d, st:%d, fmt:%d", __FUNCTION__,
         dynamic_heap ? "d-cma" : "static",
         pCreateSettings->width, pCreateSettings->height,
         pCreateSettings->pitch, pCreateSettings->pixelFormat);
   }

   return surface;
}

extern "C" NEXUS_MemoryBlockHandle hwc_block_create(
   const NEXUS_SurfaceCreateSettings *pCreateSettings,
   char *mem_if,
   bool dynamic_heap,
   int *mem_blk_fd)
{
   NEXUS_MemoryBlockHandle block_handle = NULL;
   int block_fd = -1;

   block_fd = open(mem_if, O_RDWR, 0);
   if (block_fd >= 0) {
      struct nx_ashmem_alloc ashmem_alloc;
      struct nx_ashmem_getmem ashmem_getmem;
      memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
      memset(&ashmem_getmem, 0, sizeof(struct nx_ashmem_getmem));
      ashmem_alloc.size = pCreateSettings->height * pCreateSettings->pitch;
      ashmem_alloc.align = 4096;
      ashmem_alloc.heap = dynamic_heap ? NX_ASHMEM_HEAP_DCMA : NX_ASHMEM_HEAP_FB;
      int ret = ioctl(block_fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
      if (ret < 0) {
         close(block_fd);
         block_fd = -1;
      } else {
         ret = ioctl(block_fd, NX_ASHMEM_GETMEM, &ashmem_getmem);
         if (ret < 0) {
            close(block_fd);
            block_fd = -1;
         } else {
            block_handle = (NEXUS_MemoryBlockHandle)ashmem_getmem.hdl;
         }
      }
   }

   if (block_fd >= 0) {
      *mem_blk_fd = block_fd;
   }
   return block_handle;
}

