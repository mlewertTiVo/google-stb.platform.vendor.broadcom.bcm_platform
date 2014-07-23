/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
 * 
 *****************************************************************************/
// Verbose messages removed
//#define LOG_NDEBUG 0

#define LOG_TAG "bcmPlayer_base"

#include "bcmPlayer.h"
#include "bcmPlayer_base.h"
#include "bcmPlayer_nexus.h"

#include "nexus_ipc_client_factory.h"

#define BCMPLAYER_PLAYPUMP_BUFFER_SIZE 5*1024*1024

#define DBG_MACRO    LOGV("%s(%d)", __FUNCTION__, iPlayerIndex)

/* handles are accessible from all player modules */
bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

extern "C" {

void bcmPlayer_platformJoin(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    memset(nexusHandle, 0x0, sizeof(*nexusHandle));

    LOGD("%s: Entering for index [%d]\n", __FUNCTION__, iPlayerIndex);

    // Get IPC Client from the abstract factory class...
    nexusHandle->ipcclient = NexusIPCClientFactory::getClient("bcmPlayer");
   
    // Allocate all possible resources here...
    b_refsw_client_client_configuration config;
    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),"bcmPlayer");
    config.resources.screen.required = true; /* surface client id needed for simpleVideoDecoder NxClient_Join */
    config.resources.audioDecoder = true;
    config.resources.audioPlayback = true;            
    config.resources.videoDecoder = true;
    config.resources.hdmiInput = true;
    nexusHandle->nexus_client = nexusHandle->ipcclient->createClientContext(&config);
    
    if (nexusHandle->nexus_client == NULL) {
        LOGE("%s(%d): Could not create client \"%s\" context!", __FUNCTION__, iPlayerIndex, nexusHandle->ipcclient->getClientName());
    }
}

void bcmPlayer_platformUnjoin(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    if (nexusHandle->ipcclient) {
        if (nexusHandle->nexus_client) {
            if (nexusHandle->mSharedData != NULL) {
                nexusHandle->mSharedData->videoWindow.nexusClientContext = NULL;
            }
            nexusHandle->ipcclient->destroyClientContext(nexusHandle->nexus_client);
            nexusHandle->nexus_client = NULL;
        }
        delete nexusHandle->ipcclient;
        nexusHandle->ipcclient = NULL;
    }
}


int bcmPlayer_init_base(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    NEXUS_PlaypumpOpenSettings  playpumpOpenSettings;
    NEXUS_ClientConfiguration   clientConfig;
    NEXUS_Error                 rc = 0;

    DBG_MACRO;

    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    playpumpOpenSettings.heap = clientConfig.heap[1]; /* playpump requires heap with eFull mapping */
    playpumpOpenSettings.fifoSize = BCMPLAYER_PLAYPUMP_BUFFER_SIZE; /* in bytes */
    nexusHandle->playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);

    if (nexusHandle->playpump == NULL) {
        LOGE("%s(%d): Nexus playpump open failed!", __FUNCTION__, iPlayerIndex);
        rc = 1;
    }
    else {
        nexusHandle->playback = NEXUS_Playback_Create();
        if (nexusHandle->playback == NULL) {
            LOGE("Nexus playback create failed\n");
            NEXUS_Playpump_Close(nexusHandle->playpump);
            nexusHandle->playpump = NULL;
            rc = 1;
        }
        else {
            nexusHandle->simpleStcChannel = NEXUS_SimpleStcChannel_Create(NULL);
            if (!nexusHandle->simpleStcChannel) {
                LOGW("%s(%d): Simple stc channel not available!", __FUNCTION__, iPlayerIndex);
            }
            if (nexusHandle->videoTrackIndex != -1) {
                nexusHandle->simpleVideoDecoder = nexusHandle->ipcclient->acquireVideoDecoderHandle();
                if (nexusHandle->simpleVideoDecoder == NULL) {
                    LOGE("%s(%d): Can not open video decoder!", __FUNCTION__, iPlayerIndex);
                    if (nexusHandle->simpleStcChannel) {
                        NEXUS_SimpleStcChannel_Destroy(nexusHandle->simpleStcChannel);
                        nexusHandle->simpleStcChannel = NULL;
                    }
                    NEXUS_Playback_Destroy(nexusHandle->playback);
                    nexusHandle->playback = NULL;
                    NEXUS_Playpump_Close(nexusHandle->playpump);
                    nexusHandle->playpump = NULL;
                    rc = 1;
                }
            }
            if (rc == 0 && nexusHandle->audioTrackIndex != -1) {
                nexusHandle->simpleAudioDecoder = nexusHandle->ipcclient->acquireAudioDecoderHandle();
                if (nexusHandle->simpleAudioDecoder == NULL) {
                    LOGE("%s(%d): Can not open audio decoder!", __FUNCTION__, iPlayerIndex);
                    nexusHandle->ipcclient->releaseVideoDecoderHandle(nexusHandle->simpleVideoDecoder);
                    nexusHandle->simpleVideoDecoder = NULL;
                    if (nexusHandle->simpleStcChannel) {
                        NEXUS_SimpleStcChannel_Destroy(nexusHandle->simpleStcChannel);
                        nexusHandle->simpleStcChannel = NULL;
                    }
                    NEXUS_Playback_Destroy(nexusHandle->playback);
                    nexusHandle->playback = NULL;
                    NEXUS_Playpump_Close(nexusHandle->playpump);
                    nexusHandle->playpump = NULL;
                    rc = 1;
                }
            }
        }
    }
    // Note: connection to video window is done internally in nexus when creating SimpleVideoDecoder
    return rc;
}

void bcmPlayer_uninit_base(int iPlayerIndex)
{
    B_PlaybackIpError   rc;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    BcmPlayerIpHandle*  ipHandle = nexusHandle->ipHandle;
    NEXUS_PlaybackStatus status;
    NEXUS_PlaypumpStatus playpumpStatus;

    DBG_MACRO;

    if (nexusHandle->simpleVideoDecoder) {
        NEXUS_SimpleVideoDecoder_Stop(nexusHandle->simpleVideoDecoder);
        NEXUS_SimpleVideoDecoder_SetStcChannel(nexusHandle->simpleVideoDecoder, NULL);
    }

    if (nexusHandle->simpleAudioDecoder) {
        NEXUS_SimpleAudioDecoder_Stop(nexusHandle->simpleAudioDecoder);
        NEXUS_SimpleAudioDecoder_SetStcChannel(nexusHandle->simpleAudioDecoder, NULL);
    }

    if (nexusHandle->playback) {
        NEXUS_Playback_GetStatus(nexusHandle->playback, &status);

        if (status.state != NEXUS_PlaybackState_eStopped) {
            NEXUS_Playback_Stop(nexusHandle->playback);

            while (status.state != NEXUS_PlaybackState_eStopped && status.state != NEXUS_PlaybackState_eAborted) {
                  NEXUS_Playback_GetStatus(nexusHandle->playback, &status);
            }
        }
        NEXUS_Playback_CloseAllPidChannels(nexusHandle->playback);
    }

    if (nexusHandle->playpump) {
        NEXUS_Playpump_GetStatus(nexusHandle->playpump, &playpumpStatus);
        if (playpumpStatus.started){
            NEXUS_Playpump_Stop(nexusHandle->playpump);
            NEXUS_Playpump_CloseAllPidChannels(nexusHandle->playpump);
        }
    }
    
    if (nexusHandle->audioPlaypump){
        NEXUS_Playpump_GetStatus(nexusHandle->audioPlaypump, &playpumpStatus);
        if (playpumpStatus.started){
            NEXUS_Playpump_Stop(nexusHandle->audioPlaypump);
            NEXUS_Playpump_CloseAllPidChannels(nexusHandle->audioPlaypump);
        }
    }

    nexusHandle->audioPidChannel = NULL;
    nexusHandle->videoPidChannel = NULL;
    nexusHandle->pcrPidChannel   = NULL;

    if (nexusHandle->file){
        NEXUS_FilePlay_Close(nexusHandle->file);
        nexusHandle->file = NULL;
    }

    if (nexusHandle->custom_file){
        NEXUS_FilePlay_Close(nexusHandle->custom_file);
        nexusHandle->custom_file = NULL;
    }

    if (ipHandle && ipHandle->playbackIp) {
        rc = B_PlaybackIp_SessionStop(ipHandle->playbackIp);
        if (rc != B_ERROR_SUCCESS) {
            LOGE("%s: B_PlaybackIp_SessionStop returned : %d\n", __FUNCTION__, rc);
        }

        if (ipHandle->sessionOpened == true) {
            rc = B_PlaybackIp_SessionClose(ipHandle->playbackIp);
            if (rc != B_ERROR_SUCCESS) {
                LOGD("B_PlaybackIp_SessionClose rc = %d\n", rc);
            }
            else {
                ipHandle->sessionOpened = false;
            }
        }

        if (ipHandle->sslCtx) {
            B_PlaybackIp_SslUnInit(ipHandle->sslCtx);
            ipHandle->sslCtx = NULL;
        }
    }

    if (nexusHandle->playback) {
        NEXUS_Playback_Destroy(nexusHandle->playback);
        nexusHandle->playback = NULL;
    }

    if (nexusHandle->playpump) {
        NEXUS_Playpump_Close(nexusHandle->playpump);
        nexusHandle->playpump = NULL;
    }

    if (nexusHandle->audioPlaypump) {
        NEXUS_Playpump_Close(nexusHandle->audioPlaypump);
        nexusHandle->audioPlaypump = NULL;
    }

    if (nexusHandle->simpleVideoDecoder) {
        nexusHandle->ipcclient->releaseVideoDecoderHandle(nexusHandle->simpleVideoDecoder);
        nexusHandle->simpleVideoDecoder = NULL;
    }

    if (nexusHandle->simpleAudioDecoder) {
        nexusHandle->ipcclient->releaseAudioDecoderHandle(nexusHandle->simpleAudioDecoder);
        nexusHandle->simpleAudioDecoder = NULL;
    }

    if (nexusHandle->simpleStcChannel) {
        NEXUS_SimpleStcChannel_Destroy(nexusHandle->simpleStcChannel);
        nexusHandle->simpleStcChannel = NULL;
    }

    if (ipHandle) {
        if (ipHandle->playbackIp) {
            rc = B_PlaybackIp_Close(ipHandle->playbackIp);
            if (rc != B_ERROR_SUCCESS) {
                LOGD("B_PlaybackIp_Close rc = %d\n", rc);
            }
            ipHandle->playbackIp = NULL;
        }
        free(ipHandle);
        nexusHandle->ipHandle = NULL;
    }

    // Disconnect the resources from the client...
    nexusHandle->ipcclient->disconnectClientResources(nexusHandle->nexus_client);

    // Ensure the video window is made invisible by the HWC if it is a layer...
    if (nexusHandle->mSharedData != NULL &&
        android_atomic_acquire_load(&nexusHandle->mSharedData->videoWindow.layerIdPlusOne) != 0) {
        const unsigned timeout = 20;    // 20ms wait
        unsigned cnt = 1000 / timeout;  // 1s wait

        while (android_atomic_acquire_load(&nexusHandle->mSharedData->videoWindow.windowIdPlusOne) != 0 && cnt) {
            LOGV("%s(%d): Waiting for video window visible to be acknowledged by HWC...", __FUNCTION__, iPlayerIndex);
            usleep(1000*timeout);
            cnt--;
        }
        if (cnt) {
            LOGV("%s(%d): Video window visible acknowledged by HWC", __FUNCTION__, iPlayerIndex);
        }
        else {
            LOGW("%s(%d): Video window visible was NOT acknowledged by HWC!", __FUNCTION__, iPlayerIndex);
        }
    }
}

int bcmPlayer_setDataSource_base(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{
    DBG_MACRO;
   
    return 0;
}

int bcmPlayer_setDataSource_file_handle_base(int iPlayerIndex, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight)
{
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_setVideoSurface_base(int iPlayerIndex)
{
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_setVideoSurfaceTexture_base(int iPlayerIndex, buffer_handle_t bufferHandle, bool visible)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    private_handle_t * hnd = const_cast<private_handle_t *>(reinterpret_cast<private_handle_t const*>(bufferHandle));
    PSHARED_DATA pSharedData = 
      (PSHARED_DATA) NEXUS_OffsetToCachedAddr(hnd->sharedDataPhyAddr);

    DBG_MACRO;

    nexusHandle->mSharedData      = pSharedData;
    pSharedData->videoWindow.nexusClientContext = nexusHandle->nexus_client;
    pSharedData->videoWindow.windowVisible      = visible;
    android_atomic_release_store(iPlayerIndex + 1, &pSharedData->videoWindow.windowIdPlusOne);
    
    LOGD("%s(%d): %dx%d, client_context_handle=%p, sharedDataPhyAddr=0x%08x, visible=%d", __FUNCTION__,
            iPlayerIndex, hnd->width, hnd->height, (void *)pSharedData->videoWindow.nexusClientContext, hnd->sharedDataPhyAddr, visible);

    return 0;
}

int bcmPlayer_setAudioSink_base(int iPlayerIndex, bool connect)
{
    DBG_MACRO;

    if (iPlayerIndex >= MAX_NEXUS_PLAYER || iPlayerIndex < 0) {
        LOGE("%s(%d): Bad iPlayerIndex!", __FUNCTION__, iPlayerIndex);
        return 1;
    }
    return 0;
}

int bcmPlayer_prepare_base(int iPlayerIndex) 
{
    DBG_MACRO;
    
    return 0;
}

int bcmPlayer_start_base(int iPlayerIndex) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;

    if (nexusHandle->playback) {
        NEXUS_Error err;
        NEXUS_PlaybackStatus playbackStatus;

        err = NEXUS_Playback_GetStatus(nexusHandle->playback, &playbackStatus);
        if (err != 0) {
            LOGE("%s(%d): NEXUS_Playback_GetStatus() failed with %d!", __FUNCTION__, iPlayerIndex, err);
            return 1;
        }
		
        if (playbackStatus.state == NEXUS_PlaybackState_eStopped) {
            err = NEXUS_Playback_Start(nexusHandle->playback,nexusHandle->file, NULL);
            if (err != 0) {
                LOGE("%s(%d): NEXUS_Playback_Start() failed with %d!", __FUNCTION__, iPlayerIndex, err);
                return 1;
            }
        }

        err = NEXUS_Playback_Play(nexusHandle->playback);
        if (err != 0) {
            LOGE("%s(%d): NEXUS_Playback_Play() failed with %d!", __FUNCTION__, iPlayerIndex, err);
            return 1;
        }
    }
    return 0;
}

int bcmPlayer_stop_base(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;

    if (nexusHandle->playback) {
        NEXUS_Error err;
        NEXUS_PlaybackStatus status;

        err = NEXUS_Playback_GetStatus(nexusHandle->playback, &status);
        if (err != 0) {
            LOGE("%s(%d): NEXUS_Playback_GetStatus() failed with %d!", __FUNCTION__, iPlayerIndex, err);
            return 1;
        }

        if (status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused) {
            NEXUS_Playback_Stop(nexusHandle->playback);

            while (status.state != NEXUS_PlaybackState_eStopped) {
                err = NEXUS_Playback_GetStatus(nexusHandle->playback, &status);
                if (err != 0) {
                    LOGE("%s(%d): NEXUS_Playback_GetStatus() failed with %d", __FUNCTION__, iPlayerIndex, err);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int bcmPlayer_pause_base(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;
    
    if (nexusHandle->playback) {
        NEXUS_Playback_Pause(nexusHandle->playback);
    }
    return 0;
}

int bcmPlayer_isPlaying_base(int iPlayerIndex) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;
    
    NEXUS_PlaybackStatus status;

    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));
    
    if (nexusHandle->playback) {
        NEXUS_Playback_GetStatus(nexusHandle->playback,&status);
        LOGD("%s(%d): nexus playback state: %d", __FUNCTION__, iPlayerIndex, status.state);
        return (status.state == NEXUS_PlaybackState_ePlaying)?1:0;
    }
    else {
        LOGE("%s(%d): nexus_handle[%d].playback is NULL!", __FUNCTION__, iPlayerIndex, iPlayerIndex);
        return 0;
    }
}

int bcmPlayer_seekTo_base(int iPlayerIndex, int msec) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;
    
    if (nexusHandle->playback) {
        NEXUS_Playback_Seek(nexusHandle->playback, msec);
    }
    return 0;
}

int bcmPlayer_getCurrentPosition_base(int iPlayerIndex, int *msec) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    NEXUS_PlaybackStatus status;
    
    DBG_MACRO;
    
    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));

    if (nexusHandle->playback) {
        NEXUS_Playback_GetStatus(nexusHandle->playback, &status);
        *msec = status.position;
    }
    else {
        LOGE("%s(%d): nexus_handle[%d].playback is NULL!", __FUNCTION__, iPlayerIndex, iPlayerIndex);
        *msec = 0;
    }
    LOGD("%s(%d): position: %ld, readPosition: %ld", __FUNCTION__, iPlayerIndex, (unsigned long)status.position, (unsigned long)status.readPosition);
    
    return 0;
}

int bcmPlayer_getDuration_base(int iPlayerIndex, int *msec) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    NEXUS_PlaybackStatus status;

    DBG_MACRO;

    memset(&status, 0, sizeof(NEXUS_PlaybackStatus));

    if (nexusHandle->playback) {
        NEXUS_Playback_GetStatus(nexusHandle->playback, &status); 
        *msec = status.last - status.first;
        LOGD("%s(%d): last: %ld, first: %ld, duration: %ld", __FUNCTION__, iPlayerIndex, status.last, status.first, status.last-status.first);
    }
    else {
        LOGE("%s(%d): nexus_handle[%d].playback is NULL!", __FUNCTION__, iPlayerIndex, iPlayerIndex);
        *msec = 0;
    }
    return 0;
}

int bcmPlayer_getFifoDepth_base(int iPlayerIndex, bcmPlayer_fifo_t fifo, int *depth)
{
    DBG_MACRO;

    *depth = 0;
    return 0;
}

int bcmPlayer_reset_base(int iPlayerIndex)
{
    DBG_MACRO;
    return 0;
}

int bcmPlayer_setLooping_base(int iPlayerIndex, int loop)
{
    DBG_MACRO;
    return 0;
}

int bcmPlayer_suspend_base(int iPlayerIndex)
{
    DBG_MACRO;
    return 0;
}

int bcmPlayer_resume_base(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;

    if (nexusHandle->playback) {
        NEXUS_Playback_Play(nexusHandle->playback);
    }
    return 0;
}

void bcmPlayer_config_base(int iPlayerIndex, const char* flag,const char* value)
{
    return;
}

int bcmPlayer_putData_base(int iPlayerIndex, const void *data, size_t data_len)
{
	return 0;
}

int bcmPlayer_putAudioData_base(int iPlayerIndex, const void *data, size_t data_len)
{
	return 0;
}

int bcmPlayer_getMediaExtractorFlags_base(int iPlayerIndex, unsigned *flags)
{
    *flags = 0;
	return 0;
}

int bcmPlayer_selectTrack_base(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;

    if (trackType == TRACK_TYPE_AUDIO) {
        nexusHandle->audioTrackIndex = select ? (int)trackIndex : -1;

        if (nexusHandle->simpleVideoDecoder) {
            LOGD("%s(%d): stopping video decoder", __FUNCTION__, iPlayerIndex);
            NEXUS_SimpleVideoDecoder_Stop(nexusHandle->simpleVideoDecoder);
            NEXUS_SimpleVideoDecoder_SetStcChannel(nexusHandle->simpleVideoDecoder, NULL);
        }
        if (nexusHandle->simpleAudioDecoder) {
            LOGD("%s(%d): stopping audio decoder", __FUNCTION__, iPlayerIndex);
            NEXUS_SimpleAudioDecoder_Stop(nexusHandle->simpleAudioDecoder);
            NEXUS_SimpleAudioDecoder_SetStcChannel(nexusHandle->simpleAudioDecoder, NULL);
        }
        bcmPlayer_stop_base(iPlayerIndex);

        /* and close any audio pid channels */
        if (nexusHandle->playback && nexusHandle->audioPidChannel) {
            NEXUS_Playback_ClosePidChannel(nexusHandle->playback, nexusHandle->audioPidChannel);
            nexusHandle->audioPidChannel = NULL;
        }

        if (nexusHandle->simpleAudioDecoder) {
            nexusHandle->ipcclient->releaseAudioDecoderHandle(nexusHandle->simpleAudioDecoder);
            nexusHandle->simpleAudioDecoder = NULL;
        }
        nexusHandle->ipcclient->disconnectClientResources(nexusHandle->nexus_client);

        if (select) {
            LOGD("%s(%d): selecting audio track %d", __FUNCTION__, iPlayerIndex, trackIndex);
            if (nexusHandle->simpleAudioDecoder == NULL) {
                nexusHandle->simpleAudioDecoder = nexusHandle->ipcclient->acquireAudioDecoderHandle();
                if (nexusHandle->simpleAudioDecoder == NULL) {
                    LOGE("%s(%d): Can not open audio decoder!", __FUNCTION__, iPlayerIndex);
                    return 1;
                }
            }
        }
        else {
            LOGD("%s(%d): deselecting audio track %d", __FUNCTION__, iPlayerIndex, trackIndex);
        }
    }
    else if (trackType == TRACK_TYPE_VIDEO) {
        nexusHandle->videoTrackIndex = select ? (int)trackIndex : -1;

        /* Always stop the video decoder first if it is running */
        if (nexusHandle->simpleVideoDecoder) {
            LOGD("%s(%d): stopping video decoder", __FUNCTION__, iPlayerIndex);
            NEXUS_SimpleVideoDecoder_Stop(nexusHandle->simpleVideoDecoder);
            NEXUS_SimpleVideoDecoder_SetStcChannel(nexusHandle->simpleVideoDecoder, NULL);
        }
        if (nexusHandle->simpleAudioDecoder) {
            LOGD("%s(%d): stopping audio decoder", __FUNCTION__, iPlayerIndex);
            NEXUS_SimpleAudioDecoder_Stop(nexusHandle->simpleAudioDecoder);
            NEXUS_SimpleAudioDecoder_SetStcChannel(nexusHandle->simpleAudioDecoder, NULL);
        }
        bcmPlayer_stop_base(iPlayerIndex);

        /* and close any video pid channels */
        if (nexusHandle->playback && nexusHandle->videoPidChannel) {
            NEXUS_Playback_ClosePidChannel(nexusHandle->playback, nexusHandle->videoPidChannel);
            nexusHandle->videoPidChannel = NULL;
        }

        if (nexusHandle->simpleVideoDecoder) {
            nexusHandle->ipcclient->releaseVideoDecoderHandle(nexusHandle->simpleVideoDecoder);
            nexusHandle->simpleVideoDecoder = NULL;
        }
        nexusHandle->ipcclient->disconnectClientResources(nexusHandle->nexus_client);

        if (select) {
            LOGD("%s(%d): selecting video track %d", __FUNCTION__, iPlayerIndex, trackIndex);
            if (nexusHandle->simpleVideoDecoder == NULL) {
                nexusHandle->simpleVideoDecoder = nexusHandle->ipcclient->acquireVideoDecoderHandle();
                if (nexusHandle->simpleVideoDecoder == NULL) {
                    LOGE("%s(%d): Can not open video decoder!", __FUNCTION__, iPlayerIndex);
                    return 1;
                }
            }
        }
        else {
            LOGD("%s(%d): deselecting video track %d", __FUNCTION__, iPlayerIndex, trackIndex);
        }
    }
    return 0;
}

int bcmPlayer_setAspectRatio_base(int iPlayerIndex, int ar){
    FILE *nexus_hd = NULL;
    int iReturn = 0;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    DBG_MACRO;    
        
    if (nexusHandle->ipcclient /* && nexusHandle->nexus_client */) {        
        uint32_t window_id = iPlayerIndex; //there is a 1-to-1 mapping between video window id and iPlayerIndex 
        b_video_window_settings window_settings;    
        
        nexusHandle->ipcclient->getVideoWindowSettings(nexusHandle->nexus_client, window_id, &window_settings);    
        if (ar < NEXUS_VideoWindowContentMode_eMax) {
            window_settings.contentMode = (NEXUS_VideoWindowContentMode)ar;
        }
        else {
            LOGW("%s(%d): Invalid ar %d for NEXUS_VideoWindowContentMode!", __FUNCTION__, iPlayerIndex, ar);
            iReturn = -2;
        }

        LOGD("%s(%d): Set nexus video aspect ratio to %d", __FUNCTION__, iPlayerIndex, ar);
        
        nexusHandle->ipcclient->setVideoWindowSettings(nexusHandle->nexus_client, window_id, &window_settings);    
    }
    return iReturn;
}

} /* extern "C" */
