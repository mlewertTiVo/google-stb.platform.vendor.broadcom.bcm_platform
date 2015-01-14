/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 *****************************************************************************/
//#define LOG_NDEBUG 0
#include "gralloc_destripe.h"
#include <cutils/log.h>

int gralloc_destripe_yv12(
    private_handle_t *pHandle,
    NEXUS_StripedSurfaceHandle hStripedSurface)
{
    NEXUS_Error errCode;
    NEXUS_SurfaceHandle hSurface422;
    NEXUS_SurfaceCreateSettings surfaceSettings;
    SHARED_DATA *pSharedData;
    void *pAddr;
    uint8_t *pPackedData, *pY, *pCb, *pCr;
    int x, y, stride, align=16, height, width;
    int rc=-EINVAL;

    if ( NULL == gralloc_g2d_hdl() )
    {
        LOGE("Graphics2D Not available.  Cannot access HW decoder data.");
        goto err_gfx2d;
    }

    pSharedData = (SHARED_DATA *)NEXUS_OffsetToCachedAddr(pHandle->sharedData);
    if ( NULL == pSharedData )
    {
        LOGE("Unable to access shared data");
        goto err_shared_data;
    }
    // HW destripe to 420 planar is not working.  We have to create an
    // intermediate 422 surface, destripe to that and SW convert back
    // to 420.
    height = pSharedData->planes[DEFAULT_PLANE].height;
    width = pSharedData->planes[DEFAULT_PLANE].width;
    NEXUS_Surface_GetDefaultCreateSettings(&surfaceSettings);
    surfaceSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;
    surfaceSettings.width = width;
    surfaceSettings.height = height;
    surfaceSettings.pitch = 2*surfaceSettings.width;
    hSurface422 = NEXUS_Surface_Create(&surfaceSettings);
    if ( NULL == hSurface422 )
    {
        LOGE("Unable to allocate destripe surface");
        goto err_surface;
    }
    errCode = NEXUS_Surface_Lock(hSurface422, &pAddr);
    if ( errCode )
    {
        LOGE("Error locking destripe surface");
        goto err_surface_lock;
    }
    NEXUS_Surface_Flush(hSurface422);

    // Issue destripe request
    errCode = NEXUS_Graphics2D_DestripeToSurface(gralloc_g2d_hdl(), hStripedSurface, hSurface422, NULL);
    if ( errCode )
    {
        LOGE("Unable to destripe surface");
        goto err_destripe;
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
          LOGW("Checkpoint Timeout");
          goto err_checkpoint;
      }
      break;
    default:
      LOGE("Checkpoint Error");
      goto err_checkpoint;
    }

    // Destripe done.  Now convert to planar for YV12
    pY = (uint8_t *)NEXUS_OffsetToCachedAddr(pSharedData->planes[DEFAULT_PLANE].physAddr);
    if ( NULL == pY )
    {
        LOGE("Unable to access YV12 pixels");
        goto err_yv12;
    }
    stride = (pSharedData->planes[DEFAULT_PLANE].width + (align-1)) & ~(align-1);
    pCr = (uint8_t *)(pY + (stride * pSharedData->planes[DEFAULT_PLANE].height));
    pCb = (uint8_t *)(pCr + ((pSharedData->planes[DEFAULT_PLANE].height/2) * ((stride/2 + (align-1)) & ~(align-1))));
    pPackedData = (uint8_t *)pAddr;
    for ( y = 0; y < height; ++y )
    {
        uint8_t y0, y1, cb, cr;
        if ( y & 1 )
        {
            for ( x = 0; x < width; x += 2 )
            {
                y0 = *pPackedData;
                pPackedData+=2;
                y1 = *pPackedData;
                pPackedData+=2;
                pY[x] = y0;
                pY[x+1] = y1;
            }
        }
        else
        {
            for ( x = 0; x < width; x += 2 )
            {
                y0 = *pPackedData++;
                cb = *pPackedData++;
                y1 = *pPackedData++;
                cr = *pPackedData++;
                pY[x] = y0;
                pY[x+1] = y1;
                pCr[x/2] = cr;
                pCb[x/2] = cb;
            }
            pCb += stride/2;
            pCr += stride/2;
        }
        pY += stride;
    }
    // Success
    rc = 0;

err_yv12:
err_checkpoint:
err_destripe:
err_surface_lock:
    NEXUS_Surface_Destroy(hSurface422);
err_surface:
err_shared_data:
err_gfx2d:
    return rc;
}
