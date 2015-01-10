/*
 * Copyright (C) 2015 Broadcom Canada Ltd.
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

#include <cutils/log.h>

#include "hwc_comp.h"
#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "bkni.h"
#include "blst_queue.h"

#define BLIT_DEBUG_LOG          0
#define COMP_CHECKPOINT_TIMEOUT 500

static void hwc_composer_post_checkpoint(NEXUS_HwcComposerHandle handle);
static void hwc_composer_checkpoint_callback(void *pParam, int param2);

extern void hwc_composer_client_composition_init(NEXUS_SurfaceComposition *composition);
extern void hwc_composer_clip_rect(const NEXUS_Rect *pBound,
                                   const NEXUS_Rect *pSrcRect,
                                   NEXUS_Rect *pDstRect);
extern void hwc_composer_scale_clipped_rect(const NEXUS_Rect *pSrcRect,
                                            const NEXUS_Rect *pSrcClipRect,
                                            const NEXUS_Rect *pDstRect,
                                            NEXUS_Rect *pDstClipRect);

BDBG_OBJECT_ID(NEXUS_HwcComposer);
struct NEXUS_HwcComposer
{
    BDBG_OBJECT(NEXUS_HwcComposer)
    BLST_Q_HEAD(clientlist, NEXUS_HwcClient) clients;
    NEXUS_Graphics2DHandle gfx;
    BKNI_EventHandle checkpoint_event;
};

BDBG_OBJECT_ID(NEXUS_HwcClient);
struct NEXUS_HwcClient
{
    BDBG_OBJECT(NEXUS_HwcClient)
    BLST_Q_ENTRY(NEXUS_HwcClient) link;
    NEXUS_HwcComposerHandle parent;
    NEXUS_SurfaceComposition settings;
    struct {
        NEXUS_SurfaceHandle surface;
        unsigned cnt;
        NEXUS_Rect surfaceRect;
    } set;
};

static void hwc_comp_checkpoint_callback(void *pParam, int param2)
{
    NEXUS_HwcComposerHandle handle = (NEXUS_HwcComposerHandle)pParam;
    (void)param2;
    BDBG_OBJECT_ASSERT(handle, NEXUS_HwcComposer);
    BKNI_SetEvent(handle->checkpoint_event);
}

NEXUS_HwcComposerHandle NEXUS_HwcComposer_Create(void)
{
    NEXUS_HwcComposerHandle handle;
    NEXUS_Graphics2DSettings settings;

    handle = BKNI_Malloc(sizeof(*handle));
    if (!handle) return NULL;
    BKNI_Memset(handle, 0, sizeof(*handle));
    BDBG_OBJECT_SET(handle, NEXUS_HwcComposer);
    handle->gfx = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
    BKNI_CreateEvent(&handle->checkpoint_event);
    NEXUS_Graphics2D_GetSettings(handle->gfx, &settings);
    settings.pollingCheckpoint = false;
    settings.checkpointCallback.callback = hwc_comp_checkpoint_callback;
    settings.checkpointCallback.context = handle;
    NEXUS_Graphics2D_SetSettings(handle->gfx, &settings);
    return handle;
}

void NEXUS_HwcComposer_Destroy( NEXUS_HwcComposerHandle handle )
{
    BDBG_OBJECT_ASSERT(handle, NEXUS_HwcComposer);
    NEXUS_Graphics2D_Close(handle->gfx);
    BKNI_DestroyEvent(handle->checkpoint_event);
    BDBG_OBJECT_DESTROY(handle, NEXUS_HwcComposer);
    BKNI_Free(handle);
}

NEXUS_HwcClientHandle NEXUS_HwcComposer_CreateClient(NEXUS_HwcComposerHandle handle)
{
    NEXUS_HwcClientHandle client;
    BDBG_OBJECT_ASSERT(handle, NEXUS_HwcComposer);
    client = BKNI_Malloc(sizeof(*client));
    if (!client) return NULL;
    BKNI_Memset(client, 0, sizeof(*client));
    BDBG_OBJECT_SET(client, NEXUS_HwcClient);
    client->parent = handle;
    hwc_composer_client_composition_init(&client->settings);
    BLST_Q_INSERT_HEAD(&handle->clients, client, link);
    return client;
}

void NEXUS_HwcComposer_DestroyClient(NEXUS_HwcClientHandle client)
{
    BDBG_OBJECT_ASSERT(client, NEXUS_HwcClient);
    BLST_Q_REMOVE(&client->parent->clients, client, link);
    BDBG_OBJECT_DESTROY(client, NEXUS_HwcClient);
    BKNI_Free(client);
}

static void hwc_composer_insert_client(NEXUS_HwcComposerHandle server, NEXUS_HwcClientHandle client, const NEXUS_SurfaceComposition *pSettings)
{
    NEXUS_HwcClientHandle temp, prev = NULL;
    for (temp = BLST_Q_FIRST(&server->clients); temp; temp = BLST_Q_NEXT(temp, link)) {
        BDBG_ASSERT(temp != client);
        if (temp->settings.zorder > pSettings->zorder) {
            if (prev) {
                BLST_Q_INSERT_AFTER(&server->clients, prev, client, link);
            }
            else {
                BLST_Q_INSERT_HEAD(&server->clients, client, link);
            }
            break;
        }
        prev = temp;
    }
    if (!temp) {
        if (prev) {
            BLST_Q_INSERT_AFTER(&server->clients, prev, client, link);
        }
        else {
            BLST_Q_INSERT_HEAD(&server->clients, client, link);
        }
    }
}

NEXUS_Error NEXUS_HwcComposer_SetClientSettings(NEXUS_HwcComposerHandle handle, NEXUS_HwcClientHandle client, const NEXUS_SurfaceComposition *pSettings)
{
    BDBG_OBJECT_ASSERT(handle, NEXUS_HwcComposer);
    client->settings = *pSettings;

    BLST_Q_REMOVE(&client->parent->clients, client, link);
    hwc_composer_insert_client(handle, client, pSettings);
    return 0;
}

NEXUS_Error NEXUS_HwcComposer_GetClientSettings(NEXUS_HwcComposerHandle handle, NEXUS_HwcClientHandle client, NEXUS_SurfaceComposition *pSettings)
{
    BDBG_OBJECT_ASSERT(handle, NEXUS_HwcComposer);
    *pSettings = client->settings;
    return 0;
}

int NEXUS_HwcComposer_Compose(NEXUS_HwcComposerHandle handle, NEXUS_SurfaceHandle output)
{
    NEXUS_HwcClientHandle client;
    NEXUS_Graphics2DBlitSettings blitSettings;
    unsigned i;
    int rc;

    NEXUS_Graphics2D_GetDefaultBlitSettings(&blitSettings);
    for (client = BLST_Q_FIRST(&handle->clients); client; client = BLST_Q_NEXT(client, link)) {
        NEXUS_Rect rect;
        NEXUS_Rect vdrect;

        if (!client->set.surface || !client->settings.visible) continue;
        blitSettings.source.surface = client->set.surface;
        blitSettings.output.surface = output;

        vdrect.x = 0;
        vdrect.y = 0;
        if (client->settings.clipBase.width && client->settings.clipBase.height) {
           vdrect.width = client->settings.clipBase.width;
           vdrect.height = client->settings.clipBase.height;
        } else {
           vdrect.width = client->settings.virtualDisplay.width;
           vdrect.height = client->settings.virtualDisplay.height;
        }
        hwc_composer_clip_rect(&vdrect, &client->settings.clipRect, &blitSettings.output.rect);
        if (!blitSettings.output.rect.width || !blitSettings.output.rect.height) continue;
        hwc_composer_scale_clipped_rect(&client->settings.clipRect, &blitSettings.output.rect, &client->set.surfaceRect, &blitSettings.source.rect);

        blitSettings.dest.surface = output;
        blitSettings.dest.rect = blitSettings.output.rect;
        blitSettings.colorBlend = client->settings.colorBlend;
        blitSettings.alphaBlend = client->settings.alphaBlend;
        blitSettings.colorKey.source = client->settings.colorKey.source;
        blitSettings.colorKey.dest = client->settings.colorKey.dest;
        blitSettings.horizontalFilter = client->settings.horizontalFilter;
        blitSettings.verticalFilter = client->settings.verticalFilter;
        blitSettings.alphaPremultiplySourceEnabled = client->settings.alphaPremultiplySourceEnabled;
        blitSettings.constantColor = client->settings.constantColor;
        blitSettings.conversionMatrixEnabled = client->settings.colorMatrixEnabled;
        if (blitSettings.conversionMatrixEnabled) {
            blitSettings.conversionMatrix = client->settings.colorMatrix;
        }
        if (BLIT_DEBUG_LOG) {
           ALOGI("blitting: source %p {%d,%d-%d,%d} - dest %p {%d,%d-%d,%d}",
                 client->set.surface, blitSettings.source.rect.x, blitSettings.source.rect.y, blitSettings.source.rect.width, blitSettings.source.rect.height,
                 blitSettings.dest.surface, blitSettings.dest.rect.x, blitSettings.dest.rect.y, blitSettings.dest.rect.width, blitSettings.dest.rect.height);
        }
        rc = NEXUS_Graphics2D_Blit(handle->gfx, &blitSettings);
        if (rc) BERR_TRACE(rc);
    }

    rc = NEXUS_Graphics2D_Checkpoint(handle->gfx, NULL);
    switch ( rc ) {
       case NEXUS_SUCCESS:
       break;
       case NEXUS_GRAPHICS2D_QUEUED:
         rc = BKNI_WaitForEvent(handle->checkpoint_event, COMP_CHECKPOINT_TIMEOUT);
         if ( rc ) {
            rc = -1;
         }
       break;
       default:
         rc = -1;
    }

    hwc_composer_post_checkpoint(handle);
    return rc;
}

static void hwc_composer_checkpoint_callback(void *pParam, int param2)
{
    NEXUS_HwcComposerHandle handle = pParam;
    (void)param2;
    BKNI_SetEvent(handle->checkpoint_event);
}

static void hwc_composer_post_checkpoint(NEXUS_HwcComposerHandle handle)
{
    NEXUS_HwcClientHandle client;
    for (client = BLST_Q_FIRST(&handle->clients); client; client = BLST_Q_NEXT(client, link)) {
        if (!client->set.surface) continue;
        client->set.surface = NULL;
    }
}

NEXUS_Error NEXUS_HwcClient_SetSurface(NEXUS_HwcClientHandle client, NEXUS_SurfaceHandle surface)
{
    BDBG_OBJECT_ASSERT(client, NEXUS_HwcClient);
    client->set.surface = surface;
    if (client->set.surface) {
        NEXUS_SurfaceCreateSettings surfaceCreateSettings;
        NEXUS_Surface_GetCreateSettings(client->set.surface, &surfaceCreateSettings);
        client->set.surfaceRect.width = surfaceCreateSettings.width;
        client->set.surfaceRect.height = surfaceCreateSettings.height;
        ++client->set.cnt;
    }

    return 0;
}
