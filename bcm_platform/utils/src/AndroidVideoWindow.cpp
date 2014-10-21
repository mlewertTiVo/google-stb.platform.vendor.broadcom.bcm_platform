/******************************************************************************
* (c) 2014 Broadcom Corporation
*
* This program is the proprietary software of Broadcom Corporation and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

#include "AndroidVideoWindow.h"
#include "nexus_base_mmap.h"


AndroidVideoWindow::AndroidVideoWindow()
    : PlayerID (0xffffffff), pANW (NULL), pNxClientContext(NULL),
    IsVideoWinSet(false), pSharedData(NULL)
{
    LOG_CREATE_DESTROY("%s: ENTER",__FUNCTION__);
}

AndroidVideoWindow::~AndroidVideoWindow()
{
    LOG_CREATE_DESTROY("%s: ENTER",__FUNCTION__);
    RemoveVideoWindow();
}

bool
AndroidVideoWindow::SetPlatformSpecificContext(NexusClientContext *NxClintCnxt)
{
    if(pNxClientContext)
    {
        LOG_ERROR("%s: NxClientContext Already Set  == Returning Error ==",__FUNCTION__);
        return false;
    }

    pNxClientContext = NxClintCnxt;

    //if( (pANW) && (PlayerID!=0xffffffff) && (pNxClientContext))
    //{
    //    //All the values are set, we will set the video window now
    //    SetVideoWindow();
    //}

    return true;
}

bool
AndroidVideoWindow::SetPrivData(ANativeWindowBuffer *anb,
                                unsigned int PlID)
{
    LOG_ERROR("%s:  Setting Priv Data Of ANB:%p",__FUNCTION__,anb);
    PlayerID = PlID;

    BCMOMX_DBG_ASSERT(anb);

    private_handle_t * hnd = const_cast<private_handle_t *>
        (reinterpret_cast<private_handle_t const*>(anb->handle));

    PSHARED_DATA pShData =
        (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

    pSharedData = pShData;
    pShData->videoWindow.nexusClientContext = pNxClientContext;
    pShData->videoWindow.windowVisible = 1;

    LOG_ERROR("%s: Accessing The Shared Data Now %p",__FUNCTION__,pShData);

    android_atomic_release_store(PlayerID + 1, &pShData->videoWindow.windowIdPlusOne);

    LOG_ERROR("%s: [%d] %dx%d, client_context_handle=%p, sharedDataPhyAddr=0X%x, visible=%d pShData->videoWindow.windowIdPlusOne:%d", __FUNCTION__,
        PlayerID, hnd->width, hnd->height, (void *)pSharedData->videoWindow.nexusClientContext,
        hnd->sharedDataPhyAddr, 1, pShData->videoWindow.windowIdPlusOne);

    return true;
}

bool
AndroidVideoWindow::RemoveVideoWindow()
{
    ALOGE("%s: Enter", __FUNCTION__);

    // Ensure the video window is made invisible by the HWC if it is a layer...
    if (pSharedData != NULL && android_atomic_acquire_load(&pSharedData->videoWindow.layerIdPlusOne) != 0)
    {
        const unsigned timeout = 20;    // 20ms wait
        unsigned cnt = 1000 / timeout;  // 1s wait

		/* JIRA SWANDROID-718, disable waiting for acknowledgement from hwcomposer */
#if 0
        while (android_atomic_acquire_load(&pSharedData->videoWindow.windowIdPlusOne) != 0 && cnt)
        {
            LOGD("%s: [%d] Waiting for video window visible to be acknowledged by HWC...", __FUNCTION__, PlayerID);
            usleep(1000*timeout);
            cnt--;
        }

        if (cnt)
        {
            LOG_ERROR("%s: [%d] Video window visible acknowledged by HWC", __FUNCTION__, PlayerID);
            ALOGE("%s: Exit ", __FUNCTION__);
            return true;
        }else {
            LOG_ERROR("%s: [%d] Video window visible was NOT acknowledged by HWC!", __FUNCTION__, PlayerID);
        }
#endif
    }

    ALOGE("%s: Exit ", __FUNCTION__);
    return false;
}





