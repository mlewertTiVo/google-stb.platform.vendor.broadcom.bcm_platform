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

#include "hwc_comp.h"
#include "nexus_base_mmap.h"
#include "nexus_surface_client.h"
#include "bkni.h"

static const NEXUS_BlendEquation NEXUS_SurfaceCompositor_P_ColorCopySource = {
        NEXUS_BlendFactor_eSourceColor, NEXUS_BlendFactor_eOne, false, NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false, NEXUS_BlendFactor_eZero
};

static const NEXUS_BlendEquation NEXUS_SurfaceCompositor_P_AlphaCopySource = {
        NEXUS_BlendFactor_eSourceAlpha, NEXUS_BlendFactor_eOne, false, NEXUS_BlendFactor_eZero, NEXUS_BlendFactor_eZero, false, NEXUS_BlendFactor_eZero
};

void hwc_composer_client_composition_init(NEXUS_SurfaceComposition *composition)
{
    BKNI_Memset(composition, 0, sizeof(*composition));
    composition->position.width = 1920;
    composition->position.height = 1080;
    composition->visible = false;
    composition->constantColor = 0xFF000000;
    composition->colorBlend = NEXUS_SurfaceCompositor_P_ColorCopySource;
    composition->alphaBlend = NEXUS_SurfaceCompositor_P_AlphaCopySource;
    composition->horizontalFilter = NEXUS_Graphics2DFilterCoeffs_eAnisotropic;
    composition->verticalFilter = NEXUS_Graphics2DFilterCoeffs_eAnisotropic;
    composition->virtualDisplay.width = 1920;
    composition->virtualDisplay.height = 1080;
    composition->contentMode = NEXUS_VideoWindowContentMode_eBox;
    return;
}

void hwc_composer_clip_rect(const NEXUS_Rect *pBound, const NEXUS_Rect *pSrcRect, NEXUS_Rect *pDstRect)
{
    int inc;
    BDBG_ASSERT((const void *)pSrcRect != (const void *)pDstRect);

    if (pSrcRect->x >= pBound->x + pBound->width ||
        pSrcRect->y >= pBound->y + pBound->height ||
        pSrcRect->x + pSrcRect->width < pBound->x ||
        pSrcRect->y + pSrcRect->height < pBound->y)
    {
        pDstRect->y = pBound->y;
        pDstRect->x = pBound->x;
        pDstRect->width = pDstRect->height = 0;
        return;
    }

    inc = pSrcRect->x < pBound->x ? (pBound->x - pSrcRect->x) : 0;
    pDstRect->width = pSrcRect->width - inc;
    pDstRect->x = pSrcRect->x + inc;

    inc = pSrcRect->y < pBound->y ? (pBound->y - pSrcRect->y) : 0;
    pDstRect->height = pSrcRect->height - inc;
    pDstRect->y = pSrcRect->y + inc;

    if (pDstRect->x + pDstRect->width > pBound->x + pBound->width) {
        pDstRect->width = pBound->x + pBound->width - pDstRect->x;
    }
    if (pDstRect->y + pDstRect->height > pBound->y + pBound->height) {
        pDstRect->height = pBound->y + pBound->height - pDstRect->y;
    }
    return;
}

void hwc_composer_scale_clipped_rect(const NEXUS_Rect *pSrcRect, const NEXUS_Rect *pSrcClipRect, const NEXUS_Rect *pDstRect, NEXUS_Rect *pDstClipRect)
{
    int inc;
    BDBG_ASSERT((const void *)pDstRect != (const void *)pDstClipRect);

    /* pSrcClipRect must be within pSrcRect */
    BDBG_ASSERT(pSrcClipRect->x >= pSrcRect->x);
    BDBG_ASSERT(pSrcClipRect->x + pSrcClipRect->width <= pSrcRect->x + pSrcRect->width);
    BDBG_ASSERT(pSrcClipRect->y >= pSrcRect->y);
    BDBG_ASSERT(pSrcClipRect->y + pSrcClipRect->height <= pSrcRect->y + pSrcRect->height);

    if (pSrcRect->width) {
        inc = pSrcClipRect->x - pSrcRect->x;
        pDstClipRect->x = pDstRect->x + inc * pDstRect->width / pSrcRect->width;
        pDstClipRect->width = pSrcClipRect->width * pDstRect->width / pSrcRect->width;
    }
    else {
        pDstClipRect->x = pDstClipRect->width = 0;
    }

    if (pSrcRect->height) {
        inc = pSrcClipRect->y - pSrcRect->y;
        pDstClipRect->y = pDstRect->y + inc * pDstRect->height / pSrcRect->height;
        pDstClipRect->height = pSrcClipRect->height * pDstRect->height / pSrcRect->height;
    }
    else {
        pDstClipRect->y = pDstClipRect->height = 0;
    }

    return;
}
