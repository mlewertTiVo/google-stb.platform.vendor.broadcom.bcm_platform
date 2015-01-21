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

#define LOG_TAG "gclip"

#include <cutils/log.h>
#include "nxclient.h"
#include "nexus_surface.h"
#include "nexus_surface_client.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

BDBG_MODULE(client);

#define NEXUS_TRUSTED_DATA_PATH        "/data/misc/nexus"

int main(void)
{
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_SurfaceHandle surface;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_SurfaceClientHandle blit_client;
    NEXUS_SurfaceMemory mem;
    NxClient_JoinSettings joinSettings;
    NEXUS_Error rc;
    unsigned x, y, x_factor, y_factor;
    NEXUS_SurfaceComposition comp;
    char value[256];
    FILE *key = NULL;
    NEXUS_Rect clipped;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "gclip");
    joinSettings.timeout = 60;
    joinSettings.mode = NEXUS_ClientMode_eUntrusted;

    sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(value, "r");
    if (key != NULL) {
       memset(value, 0, sizeof(value));
       fread(value, 256, 1, key);
       if (strstr(value, "trusted:") == value) {
          const char *password = &value[8];
          joinSettings.mode = NEXUS_ClientMode_eProtected;
          joinSettings.certificate.length = strlen(password);
          memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
       }
       fclose(key);
    }
    rc = NxClient_Join(&joinSettings);
    if (rc) return -1;

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.surfaceClient = 1;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) return BERR_TRACE(rc);
    blit_client = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
    createSettings.width       = 720;
    createSettings.height      = 480;
    surface = NEXUS_Surface_Create(&createSettings);

    NEXUS_Surface_GetMemory(surface, &mem);

    for (y=0;y<createSettings.height;y++) {
        uint32_t *ptr = (uint32_t *)(((uint8_t*)mem.buffer) + y * mem.pitch);
        for (x=0;x<createSettings.width;x++) {
            if (x < createSettings.width/2) {
                ptr[x] = y < createSettings.height/2 ? 0xFFFF0000 : 0xFF00FF00;
            }
            else {
                ptr[x] = y < createSettings.height/2 ? 0xFF0000FF : 0xFFFF00FF;
            }
        }
    }
    NEXUS_Surface_Flush(surface);

    rc = NEXUS_SurfaceClient_SetSurface(blit_client, surface);
    BDBG_ASSERT(!rc);
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    comp.zorder          = 10; /* above android's top most layer. */
    comp.position.x      = 400;
    comp.position.y      = 200;
    comp.position.width  = 1000;
    comp.position.height = 600;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);

    ALOGW("virtual: {%d,%d}, input: {%d,%d}, position: {%d,%d - %d,%d}",
          comp.virtualDisplay.width, comp.virtualDisplay.height,
          createSettings.width, createSettings.height,
          comp.position.x, comp.position.y, comp.position.width, comp.position.height);
    ALOGW("press ENTER");
    getchar();

    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 1920*1/4;
    comp.clipRect.y      = 0;
    comp.clipRect.width  = 1920*3/4;
    comp.clipRect.height = 1080;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("left: {%d,%d - %d,%d}", comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    ALOGW("press ENTER");
    getchar();

    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 0;
    comp.clipRect.y      = 0;
    comp.clipRect.width  = 1920*3/4;
    comp.clipRect.height = 1080;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("right: {%d,%d - %d,%d}", comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    ALOGW("press ENTER");
    getchar();

    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 0;
    comp.clipRect.y      = 1080*1/4;
    comp.clipRect.width  = 1920;
    comp.clipRect.height = 1080*3/4;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("top: {%d,%d - %d,%d}", comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    ALOGW("press ENTER");
    getchar();

    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 0;
    comp.clipRect.y      = 0;
    comp.clipRect.width  = 1920;
    comp.clipRect.height = 1080*3/4;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("bottom: {%d,%d - %d,%d}", comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    ALOGW("press ENTER");
    getchar();

    ALOGW("clip all around - easy");
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 1920*1/4;
    comp.clipRect.y      = 1080*1/4;
    comp.clipRect.width  = 1920*3/4;
    comp.clipRect.height = 1080*3/4;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("around - easy: {%d,%d - %d,%d}", comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height);
    ALOGW("press ENTER");
    getchar();

    ALOGW("clip all around - harder");
    NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    BDBG_ASSERT(comp.virtualDisplay.width == 1920 && comp.virtualDisplay.height == 1080);
    comp.clipRect.x      = 100;
    comp.clipRect.y      = 150;
    comp.clipRect.width  = 386;
    comp.clipRect.height = 255;
    comp.clipBase.width  = 720;
    comp.clipBase.height = 480;
    NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    ALOGW("around - harder: {%d,%d - %d,%d} -> base {%d,%d}",
          comp.clipRect.x, comp.clipRect.y, comp.clipRect.width, comp.clipRect.height,
          comp.clipBase.width, comp.clipBase.height);
    ALOGW("press ENTER");
    getchar();

    NEXUS_SurfaceClient_Release(blit_client);
    NxClient_Free(&allocResults);
    NxClient_Uninit();
    return 0;
}
