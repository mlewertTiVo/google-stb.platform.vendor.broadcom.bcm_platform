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

#include "gralloc_destripe.h"
#include <cutils/log.h>

#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "bkni.h"

#define CONVERSION_IS_VERBOSE 0

static NEXUS_SurfaceHandle to_nsc_surface(int width, int height, int stride, NEXUS_PixelFormat format, uint8_t *data)
{
    NEXUS_SurfaceHandle shdl = NULL;
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = format;
    createSettings.width       = width;
    createSettings.height      = height;
    createSettings.pitch       = stride;
    if (data) {
        createSettings.pMemory = data;
    }

    shdl = NEXUS_Surface_Create(&createSettings);

    ALOGV("%s: (%d,%d), s:%d, fmt:%d, p:%p -> %p", __FUNCTION__, width, height, stride, format, data, shdl);
    return shdl;
}

int gralloc_yv12to422p(private_handle_t *handle)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    int align, stride;
    uint8_t *yv12, *y_addr, *cr_addr, *cb_addr, *ycrcb422;
    NEXUS_SurfaceHandle srcCb, srcCr, srcY, dst422;
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

    if (gralloc_g2d_hdl() == NULL)
    {
        ALOGE("%s: no support for conversion\n", __FUNCTION__);
        rc = NEXUS_INVALID_PARAMETER;
        goto out;
    }

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

    srcY = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width,
                          pSharedData->planes[DEFAULT_PLANE].height,
                          stride,
                          NEXUS_PixelFormat_eY8,
                          y_addr);
    NEXUS_Surface_Lock(srcY, &slock);
    NEXUS_Surface_Flush(srcY);

    srcCr = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                           pSharedData->planes[DEFAULT_PLANE].height/2,
                           (stride/2 + (align-1)) & ~(align-1),
                           NEXUS_PixelFormat_eCr8,
                           cr_addr);
    NEXUS_Surface_Lock(srcCr, &slock);
    NEXUS_Surface_Flush(srcCr);

    srcCb = to_nsc_surface(pSharedData->planes[DEFAULT_PLANE].width/2,
                           pSharedData->planes[DEFAULT_PLANE].height/2,
                           (stride/2 + (align-1)) & ~(align-1),
                           NEXUS_PixelFormat_eCb8,
                           cb_addr);
    NEXUS_Surface_Lock(srcCb, &slock);
    NEXUS_Surface_Flush(srcCb);

    stride = pSharedData->planes[EXTRA_PLANE].width * pSharedData->planes[EXTRA_PLANE].bpp;
    dst422 = to_nsc_surface(pSharedData->planes[EXTRA_PLANE].width,
                            pSharedData->planes[EXTRA_PLANE].height,
                            stride,
                            (NEXUS_PixelFormat)pSharedData->planes[EXTRA_PLANE].format,
                            ycrcb422);

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
   return (int)rc;
}
