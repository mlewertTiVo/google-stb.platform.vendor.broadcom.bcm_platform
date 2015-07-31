/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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

static NEXUS_SurfaceHandle to_nx_surface(int width, int height, int stride, NEXUS_PixelFormat format,
                                         int is_mma, unsigned handle, unsigned offset, uint8_t *data)
{
    NEXUS_SurfaceHandle shdl = NULL;
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat   = format;
    createSettings.width         = width;
    createSettings.height        = height;
    createSettings.pitch         = stride;
    createSettings.managedAccess = false;
    if (!is_mma && data) {
       createSettings.pMemory = data;
    } else if (is_mma && handle) {
       createSettings.pixelMemory = (NEXUS_MemoryBlockHandle) handle;
       createSettings.pixelMemoryOffset = offset;
    }

    return NEXUS_Surface_Create(&createSettings);
}

int gralloc_destripe_yv12(
    private_handle_t *pHandle,
    NEXUS_StripedSurfaceHandle hStripedSurface)
{
   NEXUS_Error errCode = NEXUS_SUCCESS;
   NEXUS_SurfaceHandle hSurfaceY = NULL, hSurfaceCb = NULL, hSurfaceCr = NULL;
   NEXUS_SurfaceCreateSettings surfaceSettings;
   NEXUS_MemoryBlockHandle block_handle = NULL;
   PSHARED_DATA pSharedData;
   void *slock, *pMemory;

   if (gralloc_g2d_hdl() == NULL) {
      ALOGE("gralloc_destripe_yv12: no gfx2d.");
      errCode = NEXUS_INVALID_PARAMETER;
      goto err_gfx2d;
   }

   if (pHandle->is_mma) {
      pMemory = NULL;
      block_handle = (NEXUS_MemoryBlockHandle)pHandle->sharedData;
      NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
      pSharedData = (PSHARED_DATA) pMemory;
   } else {
      pSharedData = (PSHARED_DATA) NEXUS_OffsetToCachedAddr(pHandle->sharedData);
   }
   if (pSharedData == NULL) {
      ALOGE("gralloc_destripe_yv12: invalid buffer?");
      errCode = NEXUS_INVALID_PARAMETER;
      goto err_shared_data;
   }

   hSurfaceY = to_nx_surface(pSharedData->container.width,
                             pSharedData->container.height,
                             pSharedData->container.stride,
                             NEXUS_PixelFormat_eY8,
                             pHandle->is_mma,
                             pSharedData->container.physAddr,
                             0,
                             NULL /*!! non-mma case.*/);
   if (hSurfaceY == NULL) {
      ALOGE("gralloc_destripe_yv12: failed to create Y plane.");
      errCode = NEXUS_INVALID_PARAMETER;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceY, &slock);
   NEXUS_Surface_Flush(hSurfaceY);

   hSurfaceCr = to_nx_surface(pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              (pSharedData->container.stride/2 + (pHandle->alignment-1)) & ~(pHandle->alignment-1),
                              NEXUS_PixelFormat_eCr8,
                              pHandle->is_mma,
                              pSharedData->container.physAddr,
                              pSharedData->container.stride * pSharedData->container.height,
                              NULL /*!! non-mma case.*/);
   if (hSurfaceCr == NULL) {
      ALOGE("gralloc_destripe_yv12: failed to create Cr plane.");
      errCode = NEXUS_INVALID_PARAMETER;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceCr, &slock);
   NEXUS_Surface_Flush(hSurfaceCr);

   hSurfaceCb = to_nx_surface(pSharedData->container.width/2,
                              pSharedData->container.height/2,
                              (pSharedData->container.stride/2 + (pHandle->alignment-1)) & ~(pHandle->alignment-1),
                              NEXUS_PixelFormat_eCb8,
                              pHandle->is_mma,
                              pSharedData->container.physAddr,
                              (pSharedData->container.stride * pSharedData->container.height) +
                              ((pSharedData->container.height/2) * ((pSharedData->container.stride/2 + (pHandle->alignment-1)) & ~(pHandle->alignment-1))),
                              NULL /*!! non-mma case.*/);
   if (hSurfaceCb == NULL) {
      ALOGE("gralloc_destripe_yv12: failed to create Cb plane.");
      errCode = NEXUS_INVALID_PARAMETER;
      goto err_surfaces;
   }
   NEXUS_Surface_Lock(hSurfaceCb, &slock);
   NEXUS_Surface_Flush(hSurfaceCb);

   errCode = NEXUS_Graphics2D_DestripeToSurface(gralloc_g2d_hdl(), hStripedSurface, hSurfaceY, NULL);
   if (errCode) {
      goto err_destripe;
   }
   errCode = NEXUS_Graphics2D_DestripeToSurface(gralloc_g2d_hdl(), hStripedSurface, hSurfaceCb, NULL);
   if (errCode) {
      goto err_destripe;
   }
   errCode = NEXUS_Graphics2D_DestripeToSurface(gralloc_g2d_hdl(), hStripedSurface, hSurfaceCr, NULL);
   if (errCode) {
      goto err_destripe;
   }

   errCode = NEXUS_Graphics2D_Checkpoint(gralloc_g2d_hdl(), NULL);
   switch (errCode) {
   case NEXUS_SUCCESS:
      break;
   case NEXUS_GRAPHICS2D_QUEUED:
      errCode = BKNI_WaitForEvent(gralloc_g2d_evt(), CHECKPOINT_TIMEOUT);
      if (errCode) {
          ALOGE("gralloc_destripe_yv12: checkpoint timeout.");
          goto err_checkpoint;
      }
      break;
   default:
      ALOGE("gralloc_destripe_yv12: checkpoint error.");
      goto err_checkpoint;
   }

   NEXUS_Surface_Flush(hSurfaceY);
   NEXUS_Surface_Flush(hSurfaceCr);
   NEXUS_Surface_Flush(hSurfaceCb);

err_checkpoint:
err_destripe:
err_surfaces:
   if (hSurfaceY) {
      NEXUS_Surface_Unlock(hSurfaceY);
      NEXUS_Surface_Destroy(hSurfaceY);
   }
   if (hSurfaceCr) {
      NEXUS_Surface_Unlock(hSurfaceCr);
      NEXUS_Surface_Destroy(hSurfaceCr);
   }
   if (hSurfaceCb) {
      NEXUS_Surface_Unlock(hSurfaceCb);
      NEXUS_Surface_Destroy(hSurfaceCb);
   }
err_shared_data:
   if (pHandle->is_mma && block_handle) {
      NEXUS_MemoryBlock_Unlock(block_handle);
   }
err_gfx2d:
    return errCode;
}
