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
    NEXUS_SurfaceHandle finalRgba;
    void *slock;
    size_t size, sread;
    FILE *fp = NULL;
    int curarg = 1;
    int width = -1, height = -1;
    char fname[256];
    NEXUS_Graphics2DHandle gfx;
    BKNI_EventHandle checkpointEvent, spaceAvailableEvent;
    NEXUS_Rect outAdj;
    NEXUS_Graphics2DFillSettings fillSettings;
    NEXUS_Graphics2DSettings gfxSettings;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_SurfaceClientHandle blit_client;
    NEXUS_SurfaceMemory outMem;

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
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "rgbaview");
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

    size = height*width*RGBA_BPP;

    finalRgba = create_nexus_surface(width, height, width*RGBA_BPP, NEXUS_PixelFormat_eA8_B8_G8_R8, NULL, 0);
    NEXUS_Surface_Lock(finalRgba, &slock);

    if (finalRgba == NULL) {ALOGE("%s: failed allocating nexus surfaces.", __FUNCTION__); return -1;}

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = finalRgba;
    fillSettings.color = 0x0000FF00; /* green background */
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
    ALOGI("%s: {%d,%d} background fill'ed surface: {%d,%d - %d,%d}, press 'ENTER' to continue...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    NEXUS_Surface_GetMemory(finalRgba, &outMem);
    if (outMem.buffer == NULL) return -1;
    sread = fread(outMem.buffer, 1, size, fp);
    NEXUS_Surface_Flush(finalRgba);
    fclose(fp);

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, finalRgba);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.colorBlend            = nexusColorBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.alphaBlend            = nexusAlphaBlendingEquation[BLENDIND_TYPE_SRC_OVER];
    comp.visible               = true;
    comp.virtualDisplay.width  = W_1080P;
    comp.virtualDisplay.height = H_1080P;
    comp.zorder                = 10; /* above android's top most layer. */
    comp.position.x            = (W_1080P-width)/2;
    comp.position.y            = (H_1080P-height)/2;
    comp.position.width        = width;
    comp.position.height       = height;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGI("%s: {%d,%d} final-rgba: {%d,%d - %d,%d}, press 'ENTER' to terminate...", __FUNCTION__,
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    getchar();

    NEXUS_Surface_Destroy(finalRgba);
    NEXUS_SurfaceClient_Release(blit_client);
    NxClient_Free(&allocResults);
    NxClient_Uninit();
    return 0;
}
