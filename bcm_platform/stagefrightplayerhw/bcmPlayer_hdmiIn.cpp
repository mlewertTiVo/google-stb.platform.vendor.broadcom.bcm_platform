/******************************************************************************
 *    (c)2010-2012 Broadcom Corporation
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
 * $brcm_Workfile: bcmPlayer_hdmiIn.cpp $
 * $brcm_Revision: 10 $
 * $brcm_Date: 12/3/12 3:27p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_hdmiIn.cpp $
 * 
 * 10   12/3/12 3:27p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 9   9/26/12 11:49a alexpan
 * SWANDROID-224: Fix hdmi-input pink screen problem
 * 
 * 8   6/20/12 11:16a kagrawal
 * SWANDROID-108: Add support for HDMI-Input with SimpleDecoder and w/ or
 *  w/o nexus client server mode
 * 
 * 7   6/5/12 2:39p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 6   3/6/12 8:09p alexpan
 * SWANDROID-41: Fix build errors for platforms without hdmi-in
 * 
 * 5   2/24/12 4:11p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 4   2/8/12 2:53p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 3   12/29/11 6:46p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 2   12/10/11 7:18p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player and
 *  fixed HDMI crash issue
 * 
 * 7   11/28/11 6:16p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 6   11/9/11 5:05p fzhang
 * SW7425-1346: Fix crash when quit from hdmi-in player
 * 
 * 5   9/22/11 4:59p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 4   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 3   8/22/11 5:34p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 * 1   8/22/11 4:04p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 *
 *****************************************************************************/
#define LOG_TAG "bcmPlayer_hdmiIn"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

static int bcmPlayer_init_hdmiIn(int iPlayerIndex)
{
    int rc;

    LOGV("bcmPlayer_init_hdmiIn");

    rc = bcmPlayer_init_base(iPlayerIndex);

    LOGD("bcmPlayer_init_hdmiIn() completed (rc=%d)", rc);
    return rc;
}
    
static void bcmPlayer_uninit_hdmiIn(int iPlayerIndex) {
    LOGV("bcmPlayer_uninit_hdmiIn");
    
    bcmPlayer_uninit_base(iPlayerIndex);

    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }
    
    LOGD("bcmPlayer_uninit_hdmiIn() completed");
}

static void playback_turnoff(int iPlayerIndex){
    if(nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_PlaybackStatus status;
        
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

        if(status.state == NEXUS_PlaybackState_ePaused)
        {
            LOGE("PLAYBACK HASN'T BEEN STARTED YET!");
        }

        if(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
        {
            NEXUS_Playback_Stop(nexus_handle[iPlayerIndex].playback);

            while(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
                NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

            //usleep(2000000);/* wait for playback stoping */
        }
    }

    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }

    if(nexus_handle[iPlayerIndex].playback)
        NEXUS_Playback_CloseAllPidChannels(nexus_handle[iPlayerIndex].playback);    
    if(nexus_handle[iPlayerIndex].playback)
        NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
    nexus_handle[iPlayerIndex].playback = NULL;

    if(nexus_handle[iPlayerIndex].playpump)
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
    nexus_handle[iPlayerIndex].playpump = NULL;
}

int bcmPlayer_setDataSource_hdmiIn(int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{
    int rc = 0;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings connectSettings;

    LOGV("bcmPlayer_setDataSource_hdmiIn('%s')", url); 

    *videoWidth = 1280;
    *videoHeight = 720;
    
    // firstly, remove playback handle
    playback_turnoff(iPlayerIndex);

    nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

    // Now connect the client resources
    BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));
    connectSettings.hdmiInput.id = client_info.hdmiInputId;
    connectSettings.hdmiInput.windowId = iPlayerIndex;
    connectSettings.hdmiInput.surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

    if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true) {
        LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
        rc = 1;
    }
    else {
        LOGI("%s: video and audio are all started via hdmi_in", __FUNCTION__);
    }
    return rc;
}

static int bcmPlayer_getMediaExtractorFlags_hdmiIn(int iPlayerIndex, unsigned *flags)
{
    *flags = CAN_PAUSE;

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void player_reg_hdmiIn(bcmPlayer_func_t *pFuncs){

    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_hdmiIn;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_hdmiIn;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_hdmiIn;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_hdmiIn;
}
