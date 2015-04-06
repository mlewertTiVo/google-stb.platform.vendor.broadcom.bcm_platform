/*
 * Copyright (C) 2014, 2015 Broadcom Canada Ltd.
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

#include "gralloc_destripe.h"
#include <cutils/log.h>

#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "bkni.h"

#define CONVERSION_IS_VERBOSE 0

static NEXUS_SurfaceHandle to_nsc_surface(int width, int height, int stride, NEXUS_PixelFormat format,
                                          int is_mma, unsigned handle, unsigned offset, uint8_t *data)
{
    NEXUS_SurfaceHandle shdl = NULL;
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = format;
    createSettings.width       = width;
    createSettings.height      = height;
    createSettings.pitch       = stride;
    if (!is_mma && data) {
        createSettings.pMemory = data;
    } else if (is_mma && handle) {
        createSettings.pixelMemory = (NEXUS_MemoryBlockHandle) handle;
        createSettings.pixelMemoryOffset = offset;
    }

    shdl = NEXUS_Surface_Create(&createSettings);

    ALOGV("%s: (%d,%d), s:%d, fmt:%d, p:%p, h:%p -> %p",
          __FUNCTION__, width, height, stride, format, data, handle, shdl);
    return shdl;
}

int gralloc_yv12to422p(private_handle_t *handle)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    int stride, cstride;
    uint8_t *yv12, *y_addr, *cr_addr, *cb_addr, *ycrcb422;
    unsigned cr_offset, cb_offset;
    NEXUS_SurfaceHandle srcCb, srcCr, srcY, dst422;
    NEXUS_Graphics2DSettings gfxSettings;
    BM2MC_PACKET_Plane planeY, planeCb, planeCr, planeYCbCr;
    void *buffer, *next, *slock, *pMemory;
    size_t size;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    PSHARED_DATA pSharedData;

    BM2MC_PACKET_Blend combColor = {BM2MC_PACKET_BlendFactor_eSourceColor,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eDestinationColor,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero};
    BM2MC_PACKET_Blend copyAlpha = {BM2MC_PACKET_BlendFactor_eZero,
                                    BM2MC_PACKET_BlendFactor_eOne,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero,
                                    BM2MC_PACKET_BlendFactor_eZero,
                                    false,
                                    BM2MC_PACKET_BlendFactor_eZero};

    if (handle->is_mma) {
       pMemory = NULL;
       block_handle = (NEXUS_MemoryBlockHandle)handle->sharedData;
       NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
       pSharedData = (PSHARED_DATA) pMemory;
    } else {
       pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(handle->sharedData);
    }

    if (pSharedData == NULL) {
        ALOGE("%s: unable to locate shared data, abort conversion\n", __FUNCTION__);
        rc = NEXUS_INVALID_PARAMETER;
        goto out;
    }

    if (gralloc_g2d_hdl() == NULL) {
        ALOGE("%s: no support for conversion\n", __FUNCTION__);
        rc = NEXUS_INVALID_PARAMETER;
        goto out;
    }

    stride = (pSharedData->planes[DEFAULT_PLANE].width + (handle->alignment-1)) & ~(handle->alignment-1);
    cstride = (stride/2 + (handle->alignment-1)) & ~(handle->alignment-1),
    cr_offset = stride * pSharedData->planes[DEFAULT_PLANE].height;
    cb_offset = (pSharedData->planes[DEFAULT_PLANE].height/2) * ((stride/2 + (handle->alignment-1)) & ~(handle->alignment-1));

    if (handle->is_mma) {
       yv12     = NULL;
       ycrcb422 = NULL;
       y_addr   = NULL;
       cr_addr  = NULL;
       cb_addr  = NULL;
    } else {
       yv12 = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
       if (yv12 == NULL) {
          ALOGE("%s: yv12 input address NULL\n", __FUNCTION__);
          rc = NEXUS_INVALID_PARAMETER;
          goto out;
       }
       ycrcb422 = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[EXTRA_PLANE].physAddr);
       if (ycrcb422 == NULL) {
          ALOGE("%s: ycrcb422 output address NULL\n", __FUNCTION__);
          rc = NEXUS_INVALID_PARAMETER;
          goto out;
       }

       y_addr  = yv12;
       cr_addr = (uint8_t *)(y_addr + cr_offset);
       cb_addr = (uint8_t *)(cr_addr + cb_offset);
    }

    if (CONVERSION_IS_VERBOSE) {
       ALOGD("%s: yv12 (%d,%d):%d: cr-off:%u, cb-off:%u\n", __FUNCTION__,
          pSharedData->planes[DEFAULT_PLANE].width, pSharedData->planes[DEFAULT_PLANE].height,
          stride, cr_offset, cb_offset);
    }

    srcY = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width,
                          pSharedData->planes[DEFAULT_PLANE].height,
                          stride,
                          NEXUS_PixelFormat_eY8,
                          handle->is_mma,
                          pSharedData->planes[DEFAULT_PLANE].physAddr,
                          0,
                          y_addr);
    NEXUS_Surface_Lock(srcY, &slock);
    NEXUS_Surface_Flush(srcY);

    srcCr = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                           pSharedData->planes[DEFAULT_PLANE].height/2,
                           cstride,
                           NEXUS_PixelFormat_eCr8,
                           handle->is_mma,
                           pSharedData->planes[DEFAULT_PLANE].physAddr,
                           cr_offset,
                           cr_addr);
    NEXUS_Surface_Lock(srcCr, &slock);
    NEXUS_Surface_Flush(srcCr);

    srcCb = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                           pSharedData->planes[DEFAULT_PLANE].height/2,
                           cstride,
                           NEXUS_PixelFormat_eCb8,
                           handle->is_mma,
                           pSharedData->planes[DEFAULT_PLANE].physAddr,
                           cb_offset,
                           cb_addr);
    NEXUS_Surface_Lock(srcCb, &slock);
    NEXUS_Surface_Flush(srcCb);

    stride = pSharedData->planes[EXTRA_PLANE].width * pSharedData->planes[EXTRA_PLANE].bpp;
    dst422 = to_nsc_surface(pSharedData->planes[EXTRA_PLANE].width,
                            pSharedData->planes[EXTRA_PLANE].height,
                            stride,
                            (NEXUS_PixelFormat)pSharedData->planes[EXTRA_PLANE].format,
                            handle->is_mma,
                            pSharedData->planes[EXTRA_PLANE].physAddr,
                            0,
                            ycrcb422);
    NEXUS_Surface_Lock(dst422, &slock);
    NEXUS_Surface_Flush(dst422);

    if (CONVERSION_IS_VERBOSE) {
       ALOGD("%s: intermediate surfaces: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);
    }
    if (srcY == NULL || srcCr == NULL || srcCb == NULL) {
       ALOGE("%s: at least one null input surface: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);
       rc = NEXUS_INVALID_PARAMETER;
       goto out_cleanup;
    }
    if (dst422 == NULL) {
       ALOGE("%s: null output surface\n", __FUNCTION__);
       rc = NEXUS_INVALID_PARAMETER;
       goto out_cleanup;
    }

    rc = NEXUS_Graphics2D_GetPacketBuffer(gralloc_g2d_hdl(), &buffer, &size, 1024);
    if ((rc != NEXUS_SUCCESS) || !size) {
       ALOGE("%s: failed getting packet buffer from g2d: (num:%d, id:0x%x)\n", __FUNCTION__,

             NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
       goto out_cleanup;
    }

    NEXUS_Surface_InitPlaneAndPaletteOffset(srcY, &planeY, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(srcCb, &planeCb, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(srcCr, &planeCr, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(dst422, &planeYCbCr, NULL);

    next = buffer;
    {
        BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
        BM2MC_PACKET_INIT(pPacket, FilterEnable, false );
        pPacket->enable = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketSourceFeeders *pPacket = (BM2MC_PACKET_PacketSourceFeeders *)next;
        BM2MC_PACKET_INIT(pPacket, SourceFeeders, false );
        pPacket->plane0          = planeCb;
        pPacket->plane1          = planeCr;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketDestinationFeeder *pPacket = (BM2MC_PACKET_PacketDestinationFeeder *)next;
        BM2MC_PACKET_INIT(pPacket, DestinationFeeder, false );
        pPacket->plane           = planeY;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
        BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
        pPacket->plane           = planeYCbCr;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
        BM2MC_PACKET_INIT( pPacket, Blend, false );
        pPacket->color_blend     = combColor;
        pPacket->alpha_blend     = copyAlpha;
        pPacket->color           = 0;
        next = ++pPacket;
    }
    {
        BM2MC_PACKET_PacketScaleBlendBlit *pPacket = (BM2MC_PACKET_PacketScaleBlendBlit *)next;
        BM2MC_PACKET_INIT(pPacket, ScaleBlendBlit, true);
        pPacket->src_rect.x      = 0;
        pPacket->src_rect.y      = 0;
        pPacket->src_rect.width  = planeCb.width;
        pPacket->src_rect.height = planeCb.height;
        pPacket->out_rect.x      = 0;
        pPacket->out_rect.y      = 0;
        pPacket->out_rect.width  = planeYCbCr.width;
        pPacket->out_rect.height = planeYCbCr.height;
        pPacket->dst_point.x     = 0;
        pPacket->dst_point.y     = 0;
        next = ++pPacket;
    }

    rc = NEXUS_Graphics2D_PacketWriteComplete(gralloc_g2d_hdl(), (uint8_t*)next - (uint8_t*)buffer);
    if (rc != NEXUS_SUCCESS) {
       ALOGE("%s: failed writting packet buffer: (num:%d, id:0x%x)\n", __FUNCTION__,
             NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
       goto out_cleanup;
    }

    rc = NEXUS_Graphics2D_Checkpoint(gralloc_g2d_hdl(), NULL);
    switch (rc) {
       case NEXUS_SUCCESS:
       break;
       case NEXUS_GRAPHICS2D_QUEUED:
         rc = BKNI_WaitForEvent(gralloc_g2d_evt(), CHECKPOINT_TIMEOUT);
         if (rc) {
            ALOGW("%s: checkpoint timeout\n", __FUNCTION__);
            goto out_cleanup;
         }
       break;
       default:
         ALOGE("%s: checkpoint error\n", __FUNCTION__);
         goto out_cleanup;
    }

out_cleanup:
    NEXUS_Surface_Destroy(srcCb);
    NEXUS_Surface_Destroy(srcCr);
    NEXUS_Surface_Destroy(srcY);
    NEXUS_Surface_Destroy(dst422);
out:
   if (handle->is_mma && block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }
   return (int)rc;
}

int gralloc_plane_copy(private_handle_t *handle, unsigned src, unsigned dst)
{
   NEXUS_Error errCode;
   NEXUS_SurfaceHandle hSurfaceSrc, hSurfaceDst;
   NEXUS_Graphics2DBlitSettings blitSettings;
   unsigned height, width, strideSrc, strideDst;
   uint8_t *pSrc, *pDst;
   void *slock, *pMemory;
   int rc = -EINVAL;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   PSHARED_DATA pSharedData;


   if (src == dst) {
      goto out;
   }

   if (src >= MAX_NUM_INSTANCES || dst >= MAX_NUM_INSTANCES) {
      ALOGE("Unknown planes src=%u dst=%u", src, dst);
      rc = NEXUS_INVALID_PARAMETER;
      goto err_plane;
   }

   if (NULL == gralloc_g2d_hdl()) {
      ALOGE("Graphics2D Not available.  Cannot access HW decoder data.");
      goto err_gfx2d;
   }

   if (handle->is_mma) {
      pMemory = NULL;
      block_handle = (NEXUS_MemoryBlockHandle)handle->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(handle->sharedData);
   }

   if (NULL == pSharedData) {
      ALOGE("Unable to access shared data");
      goto err_shared_data;
   }

   if (handle->is_mma) {
      pSrc = pDst = NULL;
   } else {
      pSrc = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[src].physAddr);
      if ( NULL == pSrc ) {
         ALOGE("%s: Input address NULL\n", __FUNCTION__);
         rc = NEXUS_INVALID_PARAMETER;
         goto err_src;
      }

      pDst = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[dst].physAddr);
      if ( NULL == pDst ) {
         ALOGE("%s: Output address NULL\n", __FUNCTION__);
         rc = NEXUS_INVALID_PARAMETER;
         goto err_dst;
      }
   }

   height = pSharedData->planes[src].height;
   width = pSharedData->planes[src].width;
   strideSrc = pSharedData->planes[src].stride;
   strideDst = pSharedData->planes[dst].stride;

   hSurfaceSrc =
         to_nsc_surface(width, height, strideSrc, (NEXUS_PixelFormat)pSharedData->planes[src].format,
                        handle->is_mma, pSharedData->planes[src].physAddr, 0, pSrc);
   if (NULL == hSurfaceSrc) {
      ALOGE("Unable to allocate input surface");
      goto err_src_surface;
   }
   NEXUS_Surface_Lock(hSurfaceSrc, &slock);
   NEXUS_Surface_Flush(hSurfaceSrc);

   hSurfaceDst =
         to_nsc_surface(width, height, strideDst, (NEXUS_PixelFormat)pSharedData->planes[dst].format,
                        handle->is_mma, pSharedData->planes[dst].physAddr, 0, pDst);
   if (NULL == hSurfaceDst) {
      ALOGE("Unable to allocate output surface");
      goto err_dst_surface;
   }
   NEXUS_Surface_Lock(hSurfaceDst, &slock);
   NEXUS_Surface_Flush(hSurfaceDst);

   NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);
   blitSettings.source.surface = hSurfaceSrc;
   blitSettings.output.surface = hSurfaceDst;
   errCode = NEXUS_Graphics2D_Blit(gralloc_g2d_hdl(), &blitSettings);
   if ( errCode )
   {
      ALOGE("Unable to copy planes - %d", errCode);
      rc = NEXUS_UNKNOWN;
      goto err_blit;
   }

   // Wait for completion
   errCode = NEXUS_Graphics2D_Checkpoint(gralloc_g2d_hdl(), NULL);
   switch ( errCode )
   {
   case NEXUS_SUCCESS:
      break;
   case NEXUS_GRAPHICS2D_QUEUED:
      errCode = BKNI_WaitForEvent(gralloc_g2d_evt(), CHECKPOINT_TIMEOUT);
      if ( errCode )
      {
         ALOGE("Checkpoint Timeout");
         goto err_checkpoint;
      }
      break;
   default:
      ALOGE("Checkpoint Error");
      goto err_checkpoint;
   }

   // Success
   NEXUS_Surface_Flush(hSurfaceDst);
   rc = 0;

err_checkpoint:
err_blit:
   NEXUS_Surface_Destroy(hSurfaceDst);
err_dst_surface:
   NEXUS_Surface_Destroy(hSurfaceSrc);
err_src_surface:
err_dst:
err_src:
err_shared_data:
   if (handle->is_mma && block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }
err_gfx2d:
err_plane:
out:
   return rc;
}
