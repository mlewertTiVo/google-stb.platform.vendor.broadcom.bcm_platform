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

#define LOG_TAG "bcm-yv12-422p"

#include <cutils/log.h>
#include "gralloc_priv.h"

#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "nexus_surface_cursor.h"
#include "nxclient.h"
#include "bkni.h"

#include "hwcutils.h"

#define CONVERSION_IS_VERBOSE 0

extern "C" NEXUS_Error yv12_to_422planar(private_handle_t *handle, NEXUS_SurfaceHandle out,
                                         NEXUS_Graphics2DHandle gfx)
{
    NEXUS_Error rc;

    int align, stride;
    uint8_t *yv12, *y_addr, *cr_addr, *cb_addr;
    NEXUS_SurfaceHandle srcCb, srcCr, srcY;
    NEXUS_Graphics2DSettings gfxSettings;
    BM2MC_PACKET_Plane planeY, planeCb, planeCr, planeYCbCr;
    void *buffer, *next, *slock;
    size_t size;

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

    PSHARED_DATA pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(handle->sharedData);

    yv12 = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
    if (yv12 == NULL) {
       ALOGE("%s: yv12 input address NULL\n", __FUNCTION__);
       rc = NEXUS_INVALID_PARAMETER;
       goto out;
    }

    align = 16;
    stride = (pSharedData->planes[DEFAULT_PLANE].width + (align-1)) & ~(align-1);
    y_addr = yv12;
    cr_addr = (uint8_t *)(y_addr + (stride * pSharedData->planes[DEFAULT_PLANE].height));
    cb_addr = (uint8_t *)(cr_addr + ((pSharedData->planes[DEFAULT_PLANE].height/2) * ((stride/2 + (align-1)) & ~(align-1))));

    if (CONVERSION_IS_VERBOSE) {
       ALOGD("%s: yv12 (%d,%d):%d: y:%p, cr:%p, cb:%p\n", __FUNCTION__,
          pSharedData->planes[DEFAULT_PLANE].width, pSharedData->planes[DEFAULT_PLANE].height,
          stride, y_addr, cr_addr, cb_addr);
    }

    srcY = hwc_to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width,
                              pSharedData->planes[DEFAULT_PLANE].height,
                              stride,
                              NEXUS_PixelFormat_eY8,
                              y_addr);
    NEXUS_Surface_Lock(srcY, &slock);
    NEXUS_Surface_Flush(srcY);

    srcCr = hwc_to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                               pSharedData->planes[DEFAULT_PLANE].height/2,
                               (stride/2 + (align-1)) & ~(align-1),
                               NEXUS_PixelFormat_eCr8,
                               cr_addr);
    NEXUS_Surface_Lock(srcCr, &slock);
    NEXUS_Surface_Flush(srcCr);

    srcCb = hwc_to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                               pSharedData->planes[DEFAULT_PLANE].height/2,
                               (stride/2 + (align-1)) & ~(align-1),
                               NEXUS_PixelFormat_eCb8,
                               cb_addr);
    NEXUS_Surface_Lock(srcCb, &slock);
    NEXUS_Surface_Flush(srcCb);

    if (CONVERSION_IS_VERBOSE) {
       ALOGD("%s: intermediate surfaces: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);
    }
    if (srcY == NULL || srcCr == NULL || srcCb == NULL) {
       ALOGE("%s: at least one null input surface: y:%p, cr:%p, cb:%p\n", __FUNCTION__, srcY, srcCr, srcCb);
       rc = NEXUS_INVALID_PARAMETER;
       goto out_cleanup;
    }

    NEXUS_Graphics2D_GetSettings(gfx, &gfxSettings);
    gfxSettings.pollingCheckpoint = true;
    NEXUS_Graphics2D_SetSettings(gfx, &gfxSettings);
    rc = NEXUS_Graphics2D_GetPacketBuffer(gfx, &buffer, &size, 1024);
    if ((rc != NEXUS_SUCCESS) || !size) {
       ALOGE("%s: failed getting packet buffer from g2d: (num:%d, id:0x%x)\n", __FUNCTION__,
             NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
       goto out_cleanup;
    }

    NEXUS_Surface_InitPlaneAndPaletteOffset(srcY, &planeY, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(srcCb, &planeCb, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(srcCr, &planeCr, NULL);
    NEXUS_Surface_InitPlaneAndPaletteOffset(out, &planeYCbCr, NULL);

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

    rc = NEXUS_Graphics2D_PacketWriteComplete(gfx, (uint8_t*)next - (uint8_t*)buffer);
    if (rc != NEXUS_SUCCESS) {
       ALOGE("%s: failed writting packet buffer: (num:%d, id:0x%x)\n", __FUNCTION__,
             NEXUS_GET_ERR_NUM(rc), NEXUS_GET_ERR_ID(rc));
       goto out_cleanup;
    }

    // Wait for blits to complete
    {
      int timeout = 10;
      while ( timeout-- > 0 && NEXUS_GRAPHICS2D_BUSY == NEXUS_Graphics2D_Checkpoint(gfx, NULL) )
      {
        BKNI_Sleep(1);
      }
      if ( timeout <= 0 )
      {
        ALOGW("422 Conversion timeout");
      }
    }

out_cleanup:
    NEXUS_Surface_Destroy(srcCb);
    NEXUS_Surface_Destroy(srcCr);
    NEXUS_Surface_Destroy(srcY);
out:
   return rc;
}
