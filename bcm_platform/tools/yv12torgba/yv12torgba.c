/***************************************************************************
 *     (c)2015 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 **************************************************************************/
#ifndef BSTD_CPU_ENDIAN
#define BSTD_CPU_ENDIAN BSTD_ENDIAN_LITTLE
#endif

#define LOG_TAG "yv12torgba"

#include <cutils/log.h>
#include "nxclient.h"
#include "nexus_base_os.h"
#include "nexus_surface.h"
#include "nexus_surface_client.h"
#include "nexus_graphics2d.h"
#include "bm2mc_packet.h"
#include "nexus_core_utils.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

BDBG_MODULE(YV12toRGBA);

#define W_1080P  1920
#define H_1080P  1080
#define YUV_BPP  2
#define RGBA_BPP 4

enum BLENDIND_TYPE {
    BLENDIND_TYPE_SRC,
    BLENDIND_TYPE_SRC_OVER,
    BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED,
    BLENDIND_TYPE_LAST
} BLENDIND_TYPE;

const NEXUS_BlendEquation nexusColorBlendingEquation[BLENDIND_TYPE_LAST] = {
    { /* BLENDIND_TYPE_SRC */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eZero,
        NEXUS_BlendFactor_eZero,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationColor,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED */
        NEXUS_BlendFactor_eSourceColor,
        NEXUS_BlendFactor_eSourceAlpha,
        false,
        NEXUS_BlendFactor_eDestinationColor,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    }
};

const NEXUS_BlendEquation nexusAlphaBlendingEquation[BLENDIND_TYPE_LAST] = {
    { /* BLENDIND_TYPE_SRC */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eZero,
        NEXUS_BlendFactor_eZero,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationAlpha,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
    { /* BLENDIND_TYPE_SRC_OVER_NON_PREMULTIPLIED */
        NEXUS_BlendFactor_eSourceAlpha,
        NEXUS_BlendFactor_eOne,
        false,
        NEXUS_BlendFactor_eDestinationAlpha,
        NEXUS_BlendFactor_eInverseSourceAlpha,
        false,
        NEXUS_BlendFactor_eZero
    },
};

#define SHIFT_FACTOR 10
static NEXUS_Graphics2DColorMatrix g_hwc_ai32_Matrix_YCbCrtoRGB =
{
   SHIFT_FACTOR,
   {
      (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for R */
      (int32_t) 0,                                 /* Cb factor for R */
      (int32_t) ( 1.596f * (1 << SHIFT_FACTOR)),   /* Cr factor for R */
      (int32_t) 0,                                 /*  A factor for R */
      (int32_t) (-223 * (1 << SHIFT_FACTOR)),      /* Increment for R */
      (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for G */
      (int32_t) (-0.391f * (1 << SHIFT_FACTOR)),   /* Cb factor for G */
      (int32_t) (-0.813f * (1 << SHIFT_FACTOR)),   /* Cr factor for G */
      (int32_t) 0,                                 /*  A factor for G */
      (int32_t) (134 * (1 << SHIFT_FACTOR)),       /* Increment for G */
      (int32_t) ( 1.164f * (1 << SHIFT_FACTOR)),   /*  Y factor for B */
      (int32_t) ( 2.018f * (1 << SHIFT_FACTOR)),   /* Cb factor for B */
      (int32_t) 0,                                 /* Cr factor for B */
      (int32_t) 0,                                 /*  A factor for B */
      (int32_t) (-277 * (1 << SHIFT_FACTOR)),      /* Increment for B */
      (int32_t) 0,                                 /*  Y factor for A */
      (int32_t) 0,                                 /* Cb factor for A */
      (int32_t) 0,                                 /* Cr factor for A */
      (int32_t) (1 << SHIFT_FACTOR),               /*  A factor for A */
      (int32_t) 0,                                 /* Increment for A */
   }
};

NEXUS_SurfaceHandle create_nexus_surface(int width, int height, int stride, NEXUS_PixelFormat format,
                                         NEXUS_MemoryBlockHandle handle, size_t offset)
{
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat   = format;
    createSettings.width         = width;
    createSettings.height        = height;
    createSettings.pitch         = stride;
    createSettings.managedAccess = false;

    if (handle != NULL) {
        createSettings.pixelMemory = handle;
        createSettings.pixelMemoryOffset = (unsigned)offset;
    }

    return NEXUS_Surface_Create(&createSettings);
}

void complete(void *data, int unused)
{
    BSTD_UNUSED(unused);
    BKNI_SetEvent((BKNI_EventHandle)data);
}

/* mimics the android composition of a yv12 input buffer into the rgba output surface which is then posted to
 * the surface compositor.
 *
 * - final rgba is 1080p buffer,
 * - input is what it is (embedded in the input file name typically),
 * - adjustement is used to scale input into the rgba buffer.
 *
 * packet blit in two phases, first convert the yv12 into a yuv422, then color convert and scale the yuv422
 * into rgba.
 */
int main(int argc, char **argv)
{
    NxClient_JoinSettings joinSettings;
    bool saved = false;
    NEXUS_Error rc;
    NEXUS_SurfaceComposition comp;
    NEXUS_Rect clipped;
    NEXUS_SurfaceHandle srcCb, srcCr, srcY, dstYUV, finalRgba;
    void *buffer, *next, *slock, *vaddr;
    BM2MC_PACKET_Plane planeY, planeCb, planeCr, planeYCbCr, planeRgba;
    size_t size, stride, alignment, sread, pkt_size, cstride, cr_offset, cb_offset;
    FILE *fp = NULL;
    int curarg = 1;
    int width = -1, height = -1;
    char fname[256];
    NEXUS_MemoryBlockHandle srcData;
    NEXUS_Graphics2DHandle gfx;
    BKNI_EventHandle checkpointEvent, spaceAvailableEvent;
    NEXUS_Rect outAdj;
    NEXUS_Graphics2DFillSettings fillSettings;
    NEXUS_Graphics2DSettings gfxSettings;
    NEXUS_SurfaceMemory outMem;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_SurfaceClientHandle blit_client;

    BM2MC_PACKET_Blend combColor = {
        BM2MC_PACKET_BlendFactor_eSourceColor,
        BM2MC_PACKET_BlendFactor_eOne,
        false,
        BM2MC_PACKET_BlendFactor_eDestinationColor,
        BM2MC_PACKET_BlendFactor_eOne,
        false,
        BM2MC_PACKET_BlendFactor_eZero};
    BM2MC_PACKET_Blend copyAlpha = {
        BM2MC_PACKET_BlendFactor_eSourceAlpha,
        BM2MC_PACKET_BlendFactor_eOne,
        false,
        BM2MC_PACKET_BlendFactor_eZero,
        BM2MC_PACKET_BlendFactor_eZero,
        false,
        BM2MC_PACKET_BlendFactor_eZero};
    BM2MC_PACKET_Blend copyColor = {
        BM2MC_PACKET_BlendFactor_eSourceColor,
        BM2MC_PACKET_BlendFactor_eOne,
        false,
        BM2MC_PACKET_BlendFactor_eZero,
        BM2MC_PACKET_BlendFactor_eZero,
        false,
        BM2MC_PACKET_BlendFactor_eZero};


    outAdj.x = 0;
    outAdj.y = 0;
    outAdj.width = W_1080P;
    outAdj.height = H_1080P;

    memset(fname, 0, sizeof(fname));
    while (argc > curarg) {
        if (!strcmp(argv[curarg], "-w") && argc>curarg+1) {
            width = strtoul(argv[++curarg], NULL, 10);
            ALOGI("%s: width %d", __FUNCTION__, width);
        } else if (!strcmp(argv[curarg], "-h") && argc>curarg+1) {
            height = strtoul(argv[++curarg], NULL, 10);
            ALOGI("%s: height %d", __FUNCTION__, height);
        } else if (!strcmp(argv[curarg], "-file") && argc>curarg+1) {
            strncpy(fname, (const char *)argv[++curarg], sizeof(fname));
            ALOGI("%s: input data \'%s\'", __FUNCTION__, fname);
        } else if (!strcmp(argv[curarg], "-ax") && argc>curarg+1) {
            outAdj.x = strtoul(argv[++curarg], NULL, 10);
        } else if (!strcmp(argv[curarg], "-ay") && argc>curarg+1) {
            outAdj.y = strtoul(argv[++curarg], NULL, 10);
        } else if (!strcmp(argv[curarg], "-aw") && argc>curarg+1) {
            outAdj.width = strtoul(argv[++curarg], NULL, 10);
        } else if (!strcmp(argv[curarg], "-ah") && argc>curarg+1) {
            outAdj.height = strtoul(argv[++curarg], NULL, 10);
        } else if (!strcmp(argv[curarg], "-save") && argc>curarg+1) {
            saved = true;
        }
        curarg++;
    };

    fp = fopen(fname, "rb");
    ALOGI("%s: read from \'%s\', input %dx%d, fp 0x%x (%s)", __FUNCTION__,
          fname, width, height, (unsigned)fp,
          (fp == NULL) ? strerror(errno) : "okay");
    if (width <= 0 || height <= 0 || fp == NULL) return -1;
    ALOGI("%s: ouput {%d,%d, %dx%d}", __FUNCTION__, outAdj.x, outAdj.y, outAdj.width, outAdj.height);

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "yv12torgba");
    joinSettings.ignoreStandbyRequest = true;
    joinSettings.timeout = 60;
    rc = NxClient_Join(&joinSettings);
    if (rc) {ALOGE("%s: failed nexus join.", __FUNCTION__); return -1;}

    BKNI_CreateEvent(&checkpointEvent);
    BKNI_CreateEvent(&spaceAvailableEvent);
    gfx = NEXUS_Graphics2D_Open(0, NULL);
    NEXUS_Graphics2D_GetSettings(gfx, &gfxSettings);
    gfxSettings.checkpointCallback.callback = complete;
    gfxSettings.checkpointCallback.context = checkpointEvent;
    gfxSettings.packetSpaceAvailable.callback = complete;
    gfxSettings.packetSpaceAvailable.context = spaceAvailableEvent;
    NEXUS_Graphics2D_SetSettings(gfx, &gfxSettings);

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.surfaceClient = 1;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) {ALOGE("%s: failed blit client allocation.", __FUNCTION__); return BERR_TRACE(rc);}
    blit_client = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);

    alignment = 16;
    stride = ((size_t)width + (alignment-1)) & ~(alignment-1);
    size = (stride * height) + 2 * ((height/2) * ((stride/2 + (alignment-1)) & ~(alignment-1)));
    cstride = (stride/2 + (alignment-1)) & ~(alignment-1);
    cr_offset = stride * height;
    cb_offset = cr_offset + (cstride * (height/2));
    ALOGI("%s: input: %dx%d, s/cs: %d/%d, cr:%d, cb:%d, size: %d", __FUNCTION__, width, height, stride, cstride, cr_offset, cb_offset, size);

    srcData = NEXUS_MemoryBlock_Allocate(NULL, size, 0, NULL);
    if (srcData == NULL) {ALOGE("%s: failed allocating nexus block.", __FUNCTION__); return -1;}

    NEXUS_MemoryBlock_Lock(srcData, &vaddr);
    if (vaddr == NULL) return -1;
    sread = fread(vaddr, 1, size, fp);
    NEXUS_FlushCache(vaddr, size);
    NEXUS_MemoryBlock_Unlock(srcData);
    fclose(fp);

    srcY = create_nexus_surface(width, height, stride, NEXUS_PixelFormat_eY8, srcData, 0);
    NEXUS_Surface_Lock(srcY, &slock);
    NEXUS_Surface_Flush(srcY);

    srcCr = create_nexus_surface(width/2, height/2, cstride, NEXUS_PixelFormat_eCr8, srcData, cr_offset);
    NEXUS_Surface_Lock(srcCr, &slock);
    NEXUS_Surface_Flush(srcCr);

    srcCb = create_nexus_surface(width/2, height/2, cstride, NEXUS_PixelFormat_eCb8, srcData, cb_offset);
    NEXUS_Surface_Lock(srcCb, &slock);
    NEXUS_Surface_Flush(srcCb);

    dstYUV = create_nexus_surface(width, height, width * YUV_BPP, NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8, NULL, 0);
    NEXUS_Surface_Lock(dstYUV, &slock);
    NEXUS_Surface_Flush(dstYUV);

    finalRgba = create_nexus_surface(W_1080P, H_1080P, W_1080P*RGBA_BPP, NEXUS_PixelFormat_eA8_B8_G8_R8, NULL, 0);
    NEXUS_Surface_Lock(finalRgba, &slock);

    if (srcY == NULL || srcCr == NULL || srcCb == NULL || dstYUV == NULL || finalRgba == NULL) {ALOGE("%s: failed allocating first pass nexus surfaces.", __FUNCTION__); return -1;}

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = finalRgba;
    fillSettings.color = 0xFF00FF00; /* green background */
    rc = NEXUS_Graphics2D_Fill(gfx, &fillSettings);
    if(rc!=NEXUS_SUCCESS) {return BERR_TRACE(rc);}
    rc = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
    if (rc == NEXUS_GRAPHICS2D_QUEUED) {
        BKNI_WaitForEvent(checkpointEvent, BKNI_INFINITE);
    } else if(rc!=NEXUS_SUCCESS) {
        return BERR_TRACE(rc);
    }

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, finalRgba);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = 0;
    comp.position.y            = 0;
    comp.position.width        = W_1080P;
    comp.position.height       = H_1080P;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: {%d,%d} background first pass fill'ed surface: {%d,%d - %d,%d}, press 'ENTER' to continue...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    rc = NEXUS_Graphics2D_GetPacketBuffer(gfx, &buffer, &pkt_size, 1024);
    BDBG_ASSERT(!rc);

    NEXUS_Surface_LockPlaneAndPalette(srcY, &planeY, NULL);
    NEXUS_Surface_LockPlaneAndPalette(srcCb, &planeCb, NULL);
    NEXUS_Surface_LockPlaneAndPalette(srcCr, &planeCr, NULL);
    NEXUS_Surface_LockPlaneAndPalette(dstYUV, &planeYCbCr, NULL);
    NEXUS_Surface_LockPlaneAndPalette(finalRgba, &planeRgba, NULL);

    /* first pass at this.  decode yv12 to yuv422, then convert+scale the yuv422 to rgba.
     * dipslayed outcome is 1) yuv422 and 2) rgba final scaled.
     */
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
    {
       BM2MC_PACKET_PacketSourceFeeder *pPacket = (BM2MC_PACKET_PacketSourceFeeder *)next;
       BM2MC_PACKET_INIT(pPacket, SourceFeeder, false );
       pPacket->plane           = planeYCbCr;
       pPacket->color           = 0xFF000000; /* add alpha, opaque. */
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceControl *pPacket = (BM2MC_PACKET_PacketSourceControl *)next;
       BM2MC_PACKET_INIT(pPacket, SourceControl, false);
       pPacket->chroma_filter   = true;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketDestinationNone *pPacket = (BM2MC_PACKET_PacketDestinationNone *)next;
       BM2MC_PACKET_INIT(pPacket, DestinationNone, false);
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
       BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
       pPacket->plane           = planeRgba;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketOutputControl *pPacket = (BM2MC_PACKET_PacketOutputControl *)next;
       BM2MC_PACKET_INIT(pPacket, OutputControl, false);
       pPacket->chroma_filter    = true;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
       BM2MC_PACKET_INIT( pPacket, Blend, false );
       pPacket->color_blend     = copyColor;
       pPacket->alpha_blend     = copyAlpha;
       pPacket->color           = 0;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
       BM2MC_PACKET_INIT(pPacket, FilterEnable, false);
       pPacket->enable          = 1;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
       pPacket->enable          = 1;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrix *pPacket = (BM2MC_PACKET_PacketSourceColorMatrix *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrix, false);
       NEXUS_Graphics2D_ConvertColorMatrix(&g_hwc_ai32_Matrix_YCbCrtoRGB, &pPacket->matrix);
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketScaleBlit *pPacket = (BM2MC_PACKET_PacketScaleBlit *)next;
       BM2MC_PACKET_INIT(pPacket, ScaleBlit, true);
       pPacket->src_rect.x       = 0;
       pPacket->src_rect.y       = 0;
       pPacket->src_rect.width   = planeYCbCr.width;
       pPacket->src_rect.height  = planeYCbCr.height;
       pPacket->out_rect.x       = outAdj.x;
       pPacket->out_rect.y       = outAdj.y;
       pPacket->out_rect.width   = outAdj.width;
       pPacket->out_rect.height  = outAdj.height;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
       pPacket->enable          = 0;
       next = ++pPacket;
    }

    rc = NEXUS_Graphics2D_PacketWriteComplete(gfx, (uint8_t*)next - (uint8_t*)buffer);
    if(rc!=NEXUS_SUCCESS) {return BERR_TRACE(rc);}
    rc = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
    if (rc == NEXUS_GRAPHICS2D_QUEUED) {
        BKNI_WaitForEvent(checkpointEvent, BKNI_INFINITE);
    } else if(rc!=NEXUS_SUCCESS) {
        return BERR_TRACE(rc);
    }

    if (saved) {
       NEXUS_Surface_GetMemory(dstYUV, &outMem);
       sprintf(fname, "/sdcard/conv_yv12_yuv422_w%d_h%d.yuv", width, height);
       fp = fopen(fname, "wb");
       ALOGI("%s: writing intermediate output to \'%s\', fp 0x%x", __FUNCTION__, fname, (unsigned)fp);
       if (fp != NULL) {
          fwrite(outMem.buffer, 1,  height * width * YUV_BPP, fp);
          fclose(fp);
       }
       NEXUS_Surface_GetMemory(finalRgba, &outMem);
       sprintf(fname, "/sdcard/conv_yv12_rgba_w%d_h%d.rgb", width, height);
       fp = fopen(fname, "wb");
       ALOGI("%s: writing final output to \'%s\', fp 0x%x", __FUNCTION__, fname, (unsigned)fp);
       if (fp != NULL) {
          fwrite(outMem.buffer, 1,  H_1080P*W_1080P*RGBA_BPP, fp);
          fclose(fp);
       }
    }

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, dstYUV);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = (W_1080P-planeYCbCr.width)/2;
    comp.position.y            = (H_1080P-planeYCbCr.height)/2;
    comp.position.width        = planeYCbCr.width;
    comp.position.height       = planeYCbCr.height;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: {%d,%d} ycbcr intermediate first pass: {%d,%d - %d,%d}, press 'ENTER' to continue...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, finalRgba);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = 0;
    comp.position.y            = 0;
    comp.position.width        = W_1080P;
    comp.position.height       = H_1080P;
    comp.clipRect.x            = outAdj.x;
    comp.clipRect.y            = outAdj.y;
    comp.clipRect.width        = outAdj.width;
    comp.clipRect.height       = outAdj.height;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: rgba final first pass {%dx%d} -> {%d,%d - %dx%d} -> clip {%d,%d - %dx%d}, press 'ENTER' to continue...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height,
          comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    getchar();

    NEXUS_Surface_UnlockPlaneAndPalette(dstYUV);
    NEXUS_Surface_Unlock(dstYUV);
    NEXUS_Surface_Destroy(dstYUV);
    NEXUS_Surface_UnlockPlaneAndPalette(finalRgba);
    NEXUS_Surface_Unlock(finalRgba);
    NEXUS_Surface_Destroy(finalRgba);

    finalRgba = create_nexus_surface(width, height, width * RGBA_BPP, NEXUS_PixelFormat_eA8_B8_G8_R8, NULL, 0);
    NEXUS_Surface_Lock(finalRgba, &slock);

    dstYUV = create_nexus_surface(width, height, width * YUV_BPP, NEXUS_PixelFormat_eY08_Cb8_Y18_Cr8, NULL, 0);
    NEXUS_Surface_Lock(dstYUV, &slock);

    if (finalRgba == NULL || dstYUV == NULL) {ALOGE("%s: failed allocating second pass nexus surfaces.", __FUNCTION__); return -1;}

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = finalRgba;
    fillSettings.color = 0xFF0000FF; /* blue background */
    rc = NEXUS_Graphics2D_Fill(gfx, &fillSettings);
    if(rc!=NEXUS_SUCCESS) {return BERR_TRACE(rc);}
    rc = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
    if (rc == NEXUS_GRAPHICS2D_QUEUED) {
        BKNI_WaitForEvent(checkpointEvent, BKNI_INFINITE);
    } else if(rc!=NEXUS_SUCCESS) {
        return BERR_TRACE(rc);
    }

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, finalRgba);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = 0;
    comp.position.y            = 0;
    comp.position.width        = W_1080P;
    comp.position.height       = H_1080P;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: {%d,%d} background second pass fill'ed surface: {%d,%d - %d,%d}, press 'ENTER' to continue...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    rc = NEXUS_Graphics2D_GetPacketBuffer(gfx, &buffer, &pkt_size, 1024);
    BDBG_ASSERT(!rc);

    NEXUS_Surface_LockPlaneAndPalette(dstYUV, &planeYCbCr, NULL);
    NEXUS_Surface_LockPlaneAndPalette(finalRgba, &planeRgba, NULL);

    /* second pass at this.  decode yv12 to rgba, no scaling.
     * dipslayed outcome is 1) rgba final (un)scaled.
     */
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
    {
       BM2MC_PACKET_PacketSourceFeeder *pPacket = (BM2MC_PACKET_PacketSourceFeeder *)next;
       BM2MC_PACKET_INIT(pPacket, SourceFeeder, false );
       pPacket->plane           = planeYCbCr;
       pPacket->color           = 0xFF000000; /* add alpha, opaque. */
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceControl *pPacket = (BM2MC_PACKET_PacketSourceControl *)next;
       BM2MC_PACKET_INIT(pPacket, SourceControl, false);
       pPacket->chroma_filter   = true;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketDestinationNone *pPacket = (BM2MC_PACKET_PacketDestinationNone *)next;
       BM2MC_PACKET_INIT(pPacket, DestinationNone, false);
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketOutputFeeder *pPacket = (BM2MC_PACKET_PacketOutputFeeder *)next;
       BM2MC_PACKET_INIT(pPacket, OutputFeeder, false);
       pPacket->plane           = planeRgba;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketOutputControl *pPacket = (BM2MC_PACKET_PacketOutputControl *)next;
       BM2MC_PACKET_INIT(pPacket, OutputControl, false);
       pPacket->chroma_filter    = true;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketBlend *pPacket = (BM2MC_PACKET_PacketBlend *)next;
       BM2MC_PACKET_INIT( pPacket, Blend, false );
       pPacket->color_blend     = copyColor;
       pPacket->alpha_blend     = copyAlpha;
       pPacket->color           = 0;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketFilterEnable *pPacket = (BM2MC_PACKET_PacketFilterEnable *)next;
       BM2MC_PACKET_INIT(pPacket, FilterEnable, false);
       pPacket->enable          = 1;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceControl *pPacket = (BM2MC_PACKET_PacketSourceControl *)next;
       BM2MC_PACKET_INIT(pPacket, SourceControl, false);
       pPacket->chroma_filter   = true;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
       pPacket->enable          = 1;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrix *pPacket = (BM2MC_PACKET_PacketSourceColorMatrix *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrix, false);
       NEXUS_Graphics2D_ConvertColorMatrix(&g_hwc_ai32_Matrix_YCbCrtoRGB, &pPacket->matrix);
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketScaleBlit *pPacket = (BM2MC_PACKET_PacketScaleBlit *)next;
       BM2MC_PACKET_INIT(pPacket, ScaleBlit, true);
       pPacket->src_rect.x       = 0;
       pPacket->src_rect.y       = 0;
       pPacket->src_rect.width   = planeRgba.width;
       pPacket->src_rect.height  = planeRgba.height;
       pPacket->out_rect.x       = 0;
       pPacket->out_rect.y       = 0;
       pPacket->out_rect.width   = planeRgba.width;
       pPacket->out_rect.height  = planeRgba.height;
       next = ++pPacket;
    }
    {
       BM2MC_PACKET_PacketSourceColorMatrixEnable *pPacket = (BM2MC_PACKET_PacketSourceColorMatrixEnable *)next;
       BM2MC_PACKET_INIT(pPacket, SourceColorMatrixEnable, false);
       pPacket->enable          = 0;
       next = ++pPacket;
    }

    rc = NEXUS_Graphics2D_PacketWriteComplete(gfx, (uint8_t*)next - (uint8_t*)buffer);
    if(rc!=NEXUS_SUCCESS) {return BERR_TRACE(rc);}
    rc = NEXUS_Graphics2D_Checkpoint(gfx, NULL);
    if (rc == NEXUS_GRAPHICS2D_QUEUED) {
        BKNI_WaitForEvent(checkpointEvent, BKNI_INFINITE);
    } else if(rc!=NEXUS_SUCCESS) {
        return BERR_TRACE(rc);
    }

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, finalRgba);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = (W_1080P-planeRgba.width)/2;
    comp.position.y            = (H_1080P-planeRgba.height)/2;
    comp.position.width        = planeRgba.width;
    comp.position.height       = planeRgba.height;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: {%d,%d} rgba final second pass surface: {%d,%d - %d,%d}, press 'ENTER' to TERMINATE.", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    NEXUS_Surface_UnlockPlaneAndPalette(dstYUV);
    NEXUS_Surface_Unlock(dstYUV);
    NEXUS_Surface_Destroy(dstYUV);
    NEXUS_Surface_UnlockPlaneAndPalette(finalRgba);
    NEXUS_Surface_Unlock(finalRgba);
    NEXUS_Surface_Destroy(finalRgba);

    NEXUS_Surface_UnlockPlaneAndPalette(srcCb);
    NEXUS_Surface_Unlock(srcCb);
    NEXUS_Surface_Destroy(srcCb);
    NEXUS_Surface_UnlockPlaneAndPalette(srcCr);
    NEXUS_Surface_Unlock(srcCr);
    NEXUS_Surface_Destroy(srcCr);
    NEXUS_Surface_UnlockPlaneAndPalette(srcY);
    NEXUS_Surface_Unlock(srcY);
    NEXUS_Surface_Destroy(srcY);

    NEXUS_MemoryBlock_Free(srcData);
    NEXUS_SurfaceClient_Release(blit_client);
    NxClient_Free(&allocResults);
    NxClient_Uninit();
    return 0;
}
