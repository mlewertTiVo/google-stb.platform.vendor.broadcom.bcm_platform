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
 * $brcm_Workfile: bcmPlayer_base.cpp $
 * $brcm_Revision: 26 $
 * $brcm_Date: 2/14/13 1:36p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_base.cpp $
 * 
 * 26   2/14/13 1:36p mnelis
 * SWANDROID-336: Refactor bcmplayer_ip code
 * 
 * 25   2/7/13 11:06a mnelis
 * SWANDROID-274: Correct media state handling
 * 
 * 24   12/20/12 6:56p mnelis
 * SWANDROID-276: Improve buffering handling
 * 
 * 23   12/14/12 2:29p kagrawal
 * SWANDROID-277: Wrapper IPC APIs for
 *  NEXUS_SimpleXXX_Acquire()/_Release()
 * 
 * 22   12/11/12 10:58p robertwm
 * SWANDROID-255: [BCM97346 & BCM97425] Widevine DRM and GTS support
 * 
 * SWANDROID-255/1   12/11/12 1:49a robertwm
 * SWANDROID-255:  [BCM97346 & BCM97425] Widevine DRM and GTS support.
 * 
 * 21   12/3/12 3:26p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 20   10/30/12 4:08p mnelis
 * SWANDROID-241 : Add RTSP support to Android
 * 
 * 19   9/19/12 5:08p mnelis
 * SWANDROID-78: Fix audio decoder handle usage (Was being NULLed), and
 *  general tidy
 * 
 * 18   7/30/12 4:08p kagrawal
 * SWANDROID-104: Support for composite output
 * 
 * 17   6/21/12 3:07p franktcc
 * SWANDROID-120: Getting rid of playpump warning message while playpump
 *  stop/start
 * 
 * 16   6/20/12 11:16a kagrawal
 * SWANDROID-108: Add support for HDMI-Input with SimpleDecoder and w/ or
 *  w/o nexus client server mode
 * 
 * 15   6/13/12 3:19p kagrawal
 * SWANDROID-108: Fixed aspect ratio for standalone android usage
 * 
 * 14   6/5/12 2:39p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 13   5/24/12 3:07p franktcc
 * SWANDROID-103: Updated code to match playback_ip changes to support
 *  some China streaming sites.
 * 
 * 12   5/7/12 3:54p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 11   5/3/12 3:49p franktcc
 * SWANDROID-67: Adding UDP/RTP/RTSP streaming playback support
 * 
 * 10   4/3/12 5:04p kagrawal
 * SWANDROID-56: Added support for VideoWindow configuration in NSC mode
 * 
 * 9   3/8/12 3:31p franktcc
 * SWANDROID-36: Fixed aspect ratio not working on ICS
 * 
 * 8   2/24/12 4:11p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 7   2/10/12 2:22p kagrawal
 * SWANDROID-12: Fixed no audio issue with Nexus client-server
 * 
 * 6   2/8/12 2:53p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 5   12/29/11 6:44p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 3   12/13/11 10:26a franktcc
 * SW7420-1906: Adding capability of setAspectRatio to ICS
 * 
 * 2   12/10/11 7:17p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 9   11/28/11 6:10p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 8   11/18/11 7:43p alexpan
 * SW7425-1804: Revert udp and http source type changes
 * 
 * 7   11/9/11 5:17p zhangjq
 * SW7425-1701 : add more IP source type into bcmPlayer
 * 
 * 6   9/22/11 4:57p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 5   9/19/11 5:22p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 4   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 3   8/22/11 5:32p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 * 1   8/22/11 4:04p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 *****************************************************************************/
#define LOG_TAG "bcmPlayer_base"

#include "bcmPlayer.h"
#include "bcmPlayer_base.h"
#include "bcmPlayer_nexus.h"

#include "nexus_ipc_client_factory.h"

#define BCMPLAYER_PLAYPUMP_BUFFER_SIZE 5*1024*1024

#define DBG_MACRO    LOGD("%s(%d)", __FUNCTION__, iPlayerIndex)

/* handles are accessible from all player modules */
bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

extern "C" {

void bcmPlayer_platformJoin(int iPlayerIndex)
{
    memset(&nexus_handle[iPlayerIndex], 0x0, sizeof(bcmPlayer_base_nexus_handle_t));

    LOGD("%s: Entering for index [%d]\n", __FUNCTION__, iPlayerIndex);

    // Get IPC Client from the abstract factory class...
    nexus_handle[iPlayerIndex].ipcclient = NexusIPCClientFactory::getClient("bcmPlayer");
   
    // Allocate all possible resources here...
    b_refsw_client_client_configuration config;
    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),"bcmPlayer");
    config.resources.screen.required = true; /* surface client id needed for simpleVideoDecoder NxClient_Join */
    config.resources.audioDecoder = true;
    config.resources.audioPlayback = true;            
    config.resources.videoDecoder = true;
    config.resources.hdmiInput = true;
    nexus_handle[iPlayerIndex].nexus_client = nexus_handle[iPlayerIndex].ipcclient->createClientContext(&config);
    
    if (nexus_handle[iPlayerIndex].nexus_client == NULL) {
        LOGE("%s: Could not create client \"%s\" context!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
    }
}

void bcmPlayer_platformUnjoin(int iPlayerIndex)
{
    if(nexus_handle[iPlayerIndex].ipcclient)
    {
        if(nexus_handle[iPlayerIndex].nexus_client)
        {
            if (nexus_handle[iPlayerIndex].mSharedData != NULL) {
                nexus_handle[iPlayerIndex].mSharedData->videoWindow.nexusClientContext = NULL;
            }
            nexus_handle[iPlayerIndex].ipcclient->destroyClientContext(nexus_handle[iPlayerIndex].nexus_client);
            nexus_handle[iPlayerIndex].nexus_client = NULL;
        }
        delete nexus_handle[iPlayerIndex].ipcclient;
        nexus_handle[iPlayerIndex].ipcclient = NULL;
    }
}


int bcmPlayer_init_base(int iPlayerIndex){
    NEXUS_PlaypumpOpenSettings  playpumpOpenSettings;
    NEXUS_ClientConfiguration   clientConfig;
    NEXUS_Error                 rc = 0;

    DBG_MACRO;

    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    playpumpOpenSettings.heap = clientConfig.heap[1]; /* playpump requires heap with eFull mapping */
    playpumpOpenSettings.fifoSize = BCMPLAYER_PLAYPUMP_BUFFER_SIZE; /* in bytes */
    nexus_handle[iPlayerIndex].playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);

    if (nexus_handle[iPlayerIndex].playpump == NULL) {
        LOGE("Nexus playpump open failed\n");
        rc = 1;
    }
    else {
        nexus_handle[iPlayerIndex].playback = NEXUS_Playback_Create();
        if (nexus_handle[iPlayerIndex].playback == NULL) {
            LOGE("Nexus playback create failed\n");
            NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
            nexus_handle[iPlayerIndex].playpump = NULL;
            rc = 1;
        }
        else {
            if (nexus_handle[iPlayerIndex].videoTrackIndex != -1) {
                nexus_handle[iPlayerIndex].simpleVideoDecoder = nexus_handle[iPlayerIndex].ipcclient->acquireVideoDecoderHandle();
                if (nexus_handle[iPlayerIndex].simpleVideoDecoder == NULL) {
                    LOGE("can not open video decoder\n");
                    NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
                    nexus_handle[iPlayerIndex].playback = NULL;
                    NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
                    nexus_handle[iPlayerIndex].playpump = NULL;
                    rc = 1;
                }
            }
            if (rc == 0 && nexus_handle[iPlayerIndex].audioTrackIndex != -1) {
                nexus_handle[iPlayerIndex].simpleAudioDecoder = nexus_handle[iPlayerIndex].ipcclient->acquireAudioDecoderHandle();
                if (nexus_handle[iPlayerIndex].simpleAudioDecoder == NULL) {
                    LOGE("can not open audio decoder\n");
                    nexus_handle[iPlayerIndex].ipcclient->releaseVideoDecoderHandle(nexus_handle[iPlayerIndex].simpleVideoDecoder);
                    nexus_handle[iPlayerIndex].simpleVideoDecoder = NULL;
                    NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
                    nexus_handle[iPlayerIndex].playback = NULL;
                    NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
                    nexus_handle[iPlayerIndex].playpump = NULL;
                    rc = 1;
                }
            }
        }
    }
    // Note: connection to video window is done internally in nexus when creating SimpleVideoDecoder
    return rc;
}

void bcmPlayer_uninit_base(int iPlayerIndex) {
    DBG_MACRO;

    NEXUS_PlaybackStatus status;
    NEXUS_PlaypumpStatus playpumpStatus;

    if(nexus_handle[iPlayerIndex].simpleVideoDecoder)
        NEXUS_SimpleVideoDecoder_Stop(nexus_handle[iPlayerIndex].simpleVideoDecoder);

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder)
        NEXUS_SimpleAudioDecoder_Stop(nexus_handle[iPlayerIndex].simpleAudioDecoder);

    if(nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

        if(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
        {
            NEXUS_Playback_Stop(nexus_handle[iPlayerIndex].playback);

            while(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
                  NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

        }
    }

    if(nexus_handle[iPlayerIndex].playback) {
        NEXUS_Playback_CloseAllPidChannels(nexus_handle[iPlayerIndex].playback);
        nexus_handle[iPlayerIndex].audioPidChannel = NULL;
        nexus_handle[iPlayerIndex].videoPidChannel = NULL;

        NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
        nexus_handle[iPlayerIndex].playback = NULL;
    }

    if(nexus_handle[iPlayerIndex].playpump) {
        NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &playpumpStatus);
        if(playpumpStatus.started){
            NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].playpump);
            NEXUS_Playpump_CloseAllPidChannels(nexus_handle[iPlayerIndex].playpump);
        }
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
        nexus_handle[iPlayerIndex].playpump = NULL;
    }
    
    if(nexus_handle[iPlayerIndex].simpleVideoDecoder) {
        nexus_handle[iPlayerIndex].ipcclient->releaseVideoDecoderHandle(nexus_handle[iPlayerIndex].simpleVideoDecoder);
        nexus_handle[iPlayerIndex].simpleVideoDecoder = NULL;
    }

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder) {
        nexus_handle[iPlayerIndex].ipcclient->releaseAudioDecoderHandle(nexus_handle[iPlayerIndex].simpleAudioDecoder);
        nexus_handle[iPlayerIndex].simpleAudioDecoder = NULL;
    }

    // Disconnect the resources from the client...
    nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);

    // Ensure the video window is made invisible by the HWC if it is a layer...
    if (nexus_handle[iPlayerIndex].mSharedData != NULL &&
        android_atomic_acquire_load(&nexus_handle[iPlayerIndex].mSharedData->videoWindow.layerIdPlusOne) != 0) {
        const unsigned timeout = 20;    // 20ms wait
        unsigned cnt = 1000 / timeout;  // 1s wait

        while (android_atomic_acquire_load(&nexus_handle[iPlayerIndex].mSharedData->videoWindow.windowIdPlusOne) != 0 && cnt) {
            LOGV("%s: [%d] Waiting for video window visible to be acknowledged by HWC...", __FUNCTION__, iPlayerIndex);
            usleep(1000*timeout);
            cnt--;
        }
        if (cnt) {
            LOGV("%s: [%d] Video window visible acknowledged by HWC", __FUNCTION__, iPlayerIndex);
        }
        else {
            LOGW("%s: [%d] Video window visible was NOT acknowledged by HWC!", __FUNCTION__, iPlayerIndex);
        }
    }
}


int bcmPlayer_setDataSource_base(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader) {
    LOGV("bcmPlayer_setDataSource_base"); 
   
    return 0;
}

int bcmPlayer_setDataSource_file_handle_base(int iPlayerIndex, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight) {
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_setVideoSurface_base(int iPlayerIndex) {
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_setVideoSurfaceTexture_base(int iPlayerIndex, buffer_handle_t bufferHandle, bool visible) {
    DBG_MACRO;
    private_handle_t * hnd = const_cast<private_handle_t *>(reinterpret_cast<private_handle_t const*>(bufferHandle));
    PSHARED_DATA pSharedData = 
      (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

    nexus_handle[iPlayerIndex].mSharedData      = pSharedData;
    pSharedData->videoWindow.nexusClientContext = nexus_handle[iPlayerIndex].nexus_client;
    pSharedData->videoWindow.windowVisible      = visible;
    android_atomic_release_store(iPlayerIndex + 1, &pSharedData->videoWindow.windowIdPlusOne);
    
    LOGD("%s: [%d] %dx%d, client_context_handle=%p, sharedDataPhyAddr=0x%08x, visible=%d", __FUNCTION__,
            iPlayerIndex, hnd->width, hnd->height, (void *)pSharedData->videoWindow.nexusClientContext, hnd->sharedDataPhyAddr, visible);

    return 0;
}

int bcmPlayer_setAudioSink_base(int iPlayerIndex, bool connect) {
    DBG_MACRO;

    if(iPlayerIndex >= MAX_NEXUS_PLAYER || iPlayerIndex < 0) {
        LOGE("%s: Bad iPlayerIndex == [%d]!!!", __FUNCTION__, iPlayerIndex);
        return 1;
    }
    return 0;
}

int bcmPlayer_prepare_base(int iPlayerIndex) {
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_start_base(int iPlayerIndex) {
    DBG_MACRO;

    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Error err;
        NEXUS_PlaybackStatus playbackStatus;

        err = NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &playbackStatus);
        if (err != 0)
        {
            LOGE("%s: NEXUS_Playback_GetStatus() failed with %d", __FUNCTION__, err);
            return 1;
        }
		
        if (playbackStatus.state == NEXUS_PlaybackState_eStopped)
        {
            err = NEXUS_Playback_Start(nexus_handle[iPlayerIndex].playback,nexus_handle[iPlayerIndex].file, NULL);
            if (err != 0)
            {
                LOGE("%s: NEXUS_Playback_Start() failed with %d", __FUNCTION__, err);
                return 1;
            }
        }

        err = NEXUS_Playback_Play(nexus_handle[iPlayerIndex].playback);
        if (err != 0)
        {
            LOGE("%s: NEXUS_Playback_Play() failed with %d", __FUNCTION__, err);
            return 1;
        }

    }
    return 0;
}

int bcmPlayer_stop_base(int iPlayerIndex) {
    DBG_MACRO;

    if (nexus_handle[iPlayerIndex].playback) {
        NEXUS_Error err;
        NEXUS_PlaybackStatus status;

        err = NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);
        if (err != 0) {
            LOGE("%s: NEXUS_Playback_GetStatus() failed with %d", __FUNCTION__, err);
            return 1;
        }

        if (status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused) {
            NEXUS_Playback_Stop(nexus_handle[iPlayerIndex].playback);

            while (status.state != NEXUS_PlaybackState_eStopped) {
                err = NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);
                if (err != 0) {
                    LOGE("%s: NEXUS_Playback_GetStatus() failed with %d", __FUNCTION__, err);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int bcmPlayer_pause_base(int iPlayerIndex) {
    DBG_MACRO;
    
    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_Pause(nexus_handle[iPlayerIndex].playback);
        LOGD("NEXUS_Playback_Pause() called");
    }
    
    return 0;
}

int bcmPlayer_isPlaying_base(int iPlayerIndex) 
{
    //DBG_MACRO;
    
    NEXUS_PlaybackStatus status;

    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));
    
    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback,&status);
        LOGD(" nexus playback state:%d\n",status.state);
        return (status.state == NEXUS_PlaybackState_ePlaying)?1:0;
    }
    else
    {
        LOGE("nexus_handle[%d].playback is NULL", iPlayerIndex);
        return 0;
    }
}

int bcmPlayer_seekTo_base(int iPlayerIndex, int msec) 
{
    DBG_MACRO;
    
    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_Seek(nexus_handle[iPlayerIndex].playback, msec);
        LOGD("NEXUS_Playback_Seek() called");
    }
    
    return 0;
}

int bcmPlayer_getCurrentPosition_base(int iPlayerIndex, int *msec) 
{
    DBG_MACRO;
    
    NEXUS_PlaybackStatus status;
    
    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));

    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);
        *msec = status.position;
    }
    else
    {
        LOGE("nexus_handle[%d].playback is NULL", iPlayerIndex);
        *msec = 0;
    }
    LOGD("position: %ld,  readPosition:  %ld\n", (unsigned long)status.position, (unsigned long)status.readPosition);
    
    return 0;
}

int bcmPlayer_getDuration_base(int iPlayerIndex, int *msec) 
{
    DBG_MACRO;

    NEXUS_PlaybackStatus status;

    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));

    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status); 
        *msec = status.last - status.first;
        LOGD("last: %ld,  first: %ld,  duration: %ld\n",status.last, status.first, status.last-status.first);
    }
    else
    {
        LOGE("nexus_handle[%d].playback is NULL", iPlayerIndex);
        *msec = 0;
    }

    return 0;
}

int bcmPlayer_getFifoDepth_base(int iPlayerIndex, bcmPlayer_fifo_t fifo, int *depth) {
    DBG_MACRO;

    *depth = 0;
    return 0;
}

int bcmPlayer_reset_base(int iPlayerIndex) {
    DBG_MACRO;
    return 0;
}

int bcmPlayer_setLooping_base(int iPlayerIndex, int loop) {
    DBG_MACRO;

    return 0;
}

int bcmPlayer_suspend_base(int iPlayerIndex) {
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_resume_base(int iPlayerIndex) {
    DBG_MACRO;
    if (nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_Playback_Play(nexus_handle[iPlayerIndex].playback);
        LOGD("NEXUS_Playback_Play()");
    }
    
    return 0;
}

void bcmPlayer_config_base(int iPlayerIndex, const char* flag,const char* value){
    return;
}

int  bcmPlayer_putData_base(int iPlayerIndex, const void *data, size_t data_len){
	return 0;
}

int  bcmPlayer_putAudioData_base(int iPlayerIndex, const void *data, size_t data_len){
	return 0;
}

int  bcmPlayer_getMediaExtractorFlags_base(int iPlayerIndex, unsigned *flags)
{
    *flags = 0;
	return 0;
}

int  bcmPlayer_selectTrack_base(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select)
{
    DBG_MACRO;

    if (trackType == TRACK_TYPE_AUDIO) {
        nexus_handle[iPlayerIndex].audioTrackIndex = select ? (int)trackIndex : -1;

        if (nexus_handle[iPlayerIndex].simpleVideoDecoder) {
            LOGD("[%d]%s: stopping video decoder", iPlayerIndex, __FUNCTION__);
            NEXUS_SimpleVideoDecoder_Stop(nexus_handle[iPlayerIndex].simpleVideoDecoder);
        }
        if (nexus_handle[iPlayerIndex].simpleAudioDecoder) {
            LOGD("[%d]%s: stopping audio decoder", iPlayerIndex, __FUNCTION__);
            NEXUS_SimpleAudioDecoder_Stop(nexus_handle[iPlayerIndex].simpleAudioDecoder);
        }
        bcmPlayer_stop_base(iPlayerIndex);

        /* and close any audio pid channels */
        if (nexus_handle[iPlayerIndex].playback && nexus_handle[iPlayerIndex].audioPidChannel) {
            NEXUS_Playback_ClosePidChannel(nexus_handle[iPlayerIndex].playback, nexus_handle[iPlayerIndex].audioPidChannel);
            nexus_handle[iPlayerIndex].audioPidChannel = NULL;
        }

        if (nexus_handle[iPlayerIndex].simpleAudioDecoder) {
            nexus_handle[iPlayerIndex].ipcclient->releaseAudioDecoderHandle(nexus_handle[iPlayerIndex].simpleAudioDecoder);
            nexus_handle[iPlayerIndex].simpleAudioDecoder = NULL;
        }
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);

        if (select) {
            LOGD("[%d]%s: selecting audio track %d", iPlayerIndex, __FUNCTION__, trackIndex);
            if (nexus_handle[iPlayerIndex].simpleAudioDecoder == NULL) {
                nexus_handle[iPlayerIndex].simpleAudioDecoder = nexus_handle[iPlayerIndex].ipcclient->acquireAudioDecoderHandle();
                if (nexus_handle[iPlayerIndex].simpleAudioDecoder == NULL) {
                    LOGE("[%d]%s: Can not open audio decoder!", iPlayerIndex, __FUNCTION__);
                    return 1;
                }
            }
        }
        else {
            LOGD("[%d]%s: deselecting audio track %d", iPlayerIndex, __FUNCTION__, trackIndex);
        }
    }
    else if (trackType == TRACK_TYPE_VIDEO) {
        nexus_handle[iPlayerIndex].videoTrackIndex = select ? (int)trackIndex : -1;

        /* Always stop the video decoder first if it is running */
        if (nexus_handle[iPlayerIndex].simpleVideoDecoder) {
            LOGD("[%d]%s: stopping video decoder", iPlayerIndex, __FUNCTION__);
            NEXUS_SimpleVideoDecoder_Stop(nexus_handle[iPlayerIndex].simpleVideoDecoder);
        }
        if (nexus_handle[iPlayerIndex].simpleAudioDecoder) {
            LOGD("[%d]%s: stopping audio decoder", iPlayerIndex, __FUNCTION__);
            NEXUS_SimpleAudioDecoder_Stop(nexus_handle[iPlayerIndex].simpleAudioDecoder);
        }
        bcmPlayer_stop_base(iPlayerIndex);

        /* and close any video pid channels */
        if (nexus_handle[iPlayerIndex].playback && nexus_handle[iPlayerIndex].videoPidChannel) {
            NEXUS_Playback_ClosePidChannel(nexus_handle[iPlayerIndex].playback, nexus_handle[iPlayerIndex].videoPidChannel);
            nexus_handle[iPlayerIndex].videoPidChannel = NULL;
        }

        if (nexus_handle[iPlayerIndex].simpleVideoDecoder) {
            nexus_handle[iPlayerIndex].ipcclient->releaseVideoDecoderHandle(nexus_handle[iPlayerIndex].simpleVideoDecoder);
            nexus_handle[iPlayerIndex].simpleVideoDecoder = NULL;
        }
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);

        if (select) {
            LOGD("[%d]%s: selecting video track %d", iPlayerIndex, __FUNCTION__, trackIndex);
            if (nexus_handle[iPlayerIndex].simpleVideoDecoder == NULL) {
                nexus_handle[iPlayerIndex].simpleVideoDecoder = nexus_handle[iPlayerIndex].ipcclient->acquireVideoDecoderHandle();
                if (nexus_handle[iPlayerIndex].simpleVideoDecoder == NULL) {
                    LOGE("[%d]%s: Can not open video decoder!", iPlayerIndex, __FUNCTION__);
                    return 1;
                }
            }
        }
        else {
            LOGD("[%d]%s: deselecting video track %d", iPlayerIndex, __FUNCTION__, trackIndex);
        }
    }
    return 0;
}

int bcmPlayer_setAspectRatio_base(int iPlayerIndex, int ar){
    FILE *nexus_hd = NULL;
    int iReturn = 0;
    DBG_MACRO;    
        
    if(nexus_handle[iPlayerIndex].ipcclient /* && nexus_handle[iPlayerIndex].nexus_client */)
    {        
        uint32_t window_id = iPlayerIndex; //there is a 1-to-1 mapping between video window id and iPlayerIndex 
        b_video_window_settings window_settings;    
        
        nexus_handle[iPlayerIndex].ipcclient->getVideoWindowSettings(nexus_handle[iPlayerIndex].nexus_client, window_id, &window_settings);    
        if(ar < NEXUS_VideoWindowContentMode_eMax){
            window_settings.contentMode = (NEXUS_VideoWindowContentMode)ar;
        }
        else{
            LOGW("Invalid ar %d for NEXUS_VideoWindowContentMode", ar);
            iReturn = -2;
        }

        LOGD("Set nexus video aspect ratio to %d", ar);
        
        nexus_handle[iPlayerIndex].ipcclient->setVideoWindowSettings(nexus_handle[iPlayerIndex].nexus_client, window_id, &window_settings);    
    }
    return iReturn;
}

} /* extern "C" */
