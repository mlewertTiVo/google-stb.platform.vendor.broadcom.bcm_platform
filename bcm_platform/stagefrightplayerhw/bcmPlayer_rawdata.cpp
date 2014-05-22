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
 * $brcm_Workfile: bcmPlayer_rawdata.cpp $
 * $brcm_Revision: 9 $
 * $brcm_Date: 2/7/13 11:06a $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_rawdata.cpp $
 * 
 * 9   2/7/13 11:06a mnelis
 * SWANDROID-274: Correct media state handling
 * 
 * 8   12/20/12 6:56p mnelis
 * SWANDROID-276: Improve buffering handling
 * 
 * 7   12/19/12 10:12p robertwm
 * SWANDROID-281: UriPlayer does not work in playing UDP/RTP multicast.
 * 
 * 6   12/14/12 2:29p kagrawal
 * SWANDROID-277: Wrapper IPC APIs for
 *  NEXUS_SimpleXXX_Acquire()/_Release()
 * 
 * 5   12/11/12 10:57p robertwm
 * SWANDROID-255: [BCM97346 & BCM97425] Widevine DRM and GTS support
 * 
 * SWANDROID-255/1   12/11/12 1:59a robertwm
 * SWANDROID-255:  [BCM97346 & BCM97425] Widevine DRM and GTS support.
 * 
 * 4   9/14/12 1:31p mnelis
 * SWANDROID-78: Use NEXUS_ANY_ID for STC channel
 * 
 * 3   6/5/12 2:40p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 2   5/8/12 2:20p franktcc
 * SWANDROID-67: Fixed compiling error when enable nexus multi-process
 * 
 * 1   5/3/12 3:47p franktcc
 * SWANDROID-67: Adding UDP/RTP/RTSP streaming playback support
 *
 *****************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "bcmPlayer_rawdata"

#include <utils/Errors.h>

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"
#include "b_os_lib.h"
#include <pthread.h>

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

typedef enum {
    STOP,
    PLAY,
    PAUSE,
} playback_status_t;

static stream_format_info bcm_stream_format_info[MAX_NEXUS_PLAYER];
static B_EventHandle      videoDataEvent[MAX_NEXUS_PLAYER];
static B_EventHandle      audioDataEvent[MAX_NEXUS_PLAYER];
static playback_status_t  playpump_status[MAX_NEXUS_PLAYER];
static pthread_mutex_t    mutexRawdata[MAX_NEXUS_PLAYER] = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
#if MAX_NEXUS_PLAYER > 2
    PTHREAD_MUTEX_INITIALIZER,
#endif
};
static bool               videoFlushing[MAX_NEXUS_PLAYER] = {
    false,
    false,
#if MAX_NEXUS_PLAYER > 2
    false,
#endif
};
static bool               audioFlushing[MAX_NEXUS_PLAYER] = {
    false, 
    false,
#if MAX_NEXUS_PLAYER > 2
    false,
#endif
};
static bool               forcePause[MAX_NEXUS_PLAYER]    = {
    false, 
    false,
#if MAX_NEXUS_PLAYER > 2
    false,
#endif
};

#define MUTEX_RAWDATA_INIT(index)\
{\
    int rc = pthread_mutex_init(&mutexRawdata[index], NULL);\
\
    if (rc)\
        LOGE("[%d]bcmPlayer_rawdata: Failed to create mutexRawdata!! [rc=%d]", index, rc);\
}

#define MUTEX_RAWDATA_DESTROY(index)\
{\
    int rc = pthread_mutex_destroy(&mutexRawdata[index]);\
\
    if (rc)\
        LOGE("[%d]bcmPlayer_rawdata: Failed to destroy mutexRawdata!! [rc=%d]", index, rc);\
}

#define MUTEX_RAWDATA_LOCK(index)   pthread_mutex_lock(&mutexRawdata[index])
#define MUTEX_RAWDATA_UNLOCK(index) pthread_mutex_unlock(&mutexRawdata[index]);

static void spaceAvailableCallback(void *context, int param)
{
    BSTD_UNUSED(param);
    LOGV("---> [%d]%s(%p)", iPlayerIndex, __FUNCTION__, context);
    B_Event_Set(context);
}

static int bcmPlayer_init_rawdata(int iPlayerIndex)
{
    NEXUS_PlaypumpOpenSettings  *pPlaypumpOpenSettings = NULL;
    NEXUS_ClientConfiguration   *pClientConfig         = NULL;
    NEXUS_SyncChannelSettings   *pSyncChannelSettings  = NULL;
    NEXUS_Error                 rc;
    B_EventSettings             eventSettings;
    B_EventHandle               eventHandle;
    
    MUTEX_RAWDATA_INIT(iPlayerIndex);

    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    
    B_Event_GetDefaultSettings(&eventSettings);
    eventHandle = B_Event_Create(&eventSettings);
    videoDataEvent[iPlayerIndex] = eventHandle;
    eventHandle = B_Event_Create(&eventSettings);
    audioDataEvent[iPlayerIndex] = eventHandle;
    
    rc = bcmPlayer_init_base(iPlayerIndex);
    
    if (rc != 0) {
        return rc;
    }

    if(nexus_handle[iPlayerIndex].playback) {
        NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
        nexus_handle[iPlayerIndex].playback = NULL;
    }

    pPlaypumpOpenSettings = (NEXUS_PlaypumpOpenSettings *)BKNI_Malloc(sizeof(NEXUS_PlaypumpOpenSettings));
    if (pPlaypumpOpenSettings == NULL) {
        LOGE("[%d]%s: pPlaypumpOpenSettings is null!", iPlayerIndex, __FUNCTION__);
        rc = 1;
    }
    else {
        NEXUS_Playpump_GetDefaultOpenSettings(pPlaypumpOpenSettings);
        
        pClientConfig = (NEXUS_ClientConfiguration *)BKNI_Malloc(sizeof(NEXUS_ClientConfiguration));
        if (pClientConfig == NULL) {
            LOGE("[%d]%s: pClientConfig is null!", iPlayerIndex, __FUNCTION__);
            if (pPlaypumpOpenSettings) {
                BKNI_Free(pPlaypumpOpenSettings);
            }
            rc = 1;
        }
        else {
            NEXUS_Platform_GetClientConfiguration(pClientConfig);

            pPlaypumpOpenSettings->heap = pClientConfig->heap[1]; /* playpump requires heap with eFull mapping */
            nexus_handle[iPlayerIndex].audioPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, pPlaypumpOpenSettings);
            if (pClientConfig) {
                BKNI_Free(pClientConfig);
            }
            if (pPlaypumpOpenSettings) {
                BKNI_Free(pPlaypumpOpenSettings);
            }
            if (nexus_handle[iPlayerIndex].audioPlaypump == NULL) {
                LOGE("[%d]%s: Nexus playpump open failed!", iPlayerIndex, __FUNCTION__);
                rc = 1;
            }
            else {
                /* create a sync channel */
                pSyncChannelSettings = (NEXUS_SyncChannelSettings *)BKNI_Malloc(sizeof(NEXUS_SyncChannelSettings));
                if (pSyncChannelSettings == NULL) {
                    LOGE("[%d]%s: pSyncChannelSettings is null!", iPlayerIndex, __FUNCTION__);
                    NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].audioPlaypump);
                    nexus_handle[iPlayerIndex].audioPlaypump = NULL;
                    rc = 1;
                }
                else {
                    NEXUS_SyncChannel_GetDefaultSettings(pSyncChannelSettings);
                    pSyncChannelSettings->adjustmentThreshold    = 300;
                    pSyncChannelSettings->enableMuteControl      = true;
                    pSyncChannelSettings->enablePrecisionLipsync = true;
                    nexus_handle[iPlayerIndex].syncChannel = NEXUS_SyncChannel_Create(pSyncChannelSettings);
                    if (pSyncChannelSettings) {
                        BKNI_Free(pSyncChannelSettings);
                    }
                    if (nexus_handle[iPlayerIndex].syncChannel == NULL) {
                        LOGE("[%d]%s: Nexus syncChannel open failed!", iPlayerIndex, __FUNCTION__);
                        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].audioPlaypump);
                        nexus_handle[iPlayerIndex].audioPlaypump = NULL;
                        rc = 1;
                    }
                    else {
                        // Now connect the client resources
                        b_refsw_client_client_info client_info;
                        nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

                        b_refsw_client_connect_resource_settings connectSettings;

                        BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));

                        connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
                        connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
                        connectSettings.simpleVideoDecoder[0].windowId = iPlayerIndex; /* Main or PIP Window */
                        if (nexus_handle[iPlayerIndex].bSupportsHEVC == true)
                        {
                            if ((nexus_handle[iPlayerIndex].maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                                (nexus_handle[iPlayerIndex].maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))
                            {
                                connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
                                connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
                            }
                            connectSettings.simpleVideoDecoder[0].decoderCaps.supportedCodecs[NEXUS_VideoCodec_eH265] = NEXUS_VideoCodec_eH265;
                        }
                        connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

                        if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true){
                            LOGE("[%d]%s: Could not connect client \"%s\" resources!", iPlayerIndex, __FUNCTION__,
                                  nexus_handle[iPlayerIndex].ipcclient->getClientName());
                            NEXUS_SyncChannel_Destroy(nexus_handle[iPlayerIndex].syncChannel);
                            nexus_handle[iPlayerIndex].syncChannel = NULL;
                            NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].audioPlaypump);
                            nexus_handle[iPlayerIndex].audioPlaypump = NULL;
                            rc = 1;
                        }

                        videoFlushing[iPlayerIndex] = false;
                        audioFlushing[iPlayerIndex] = false;
                        forcePause[iPlayerIndex] = false;
                    }
                }
            }
        }
    }
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

    return rc;
}

    
static void bcmPlayer_uninit_rawdata(int iPlayerIndex) 
{

    NEXUS_PlaybackStatus status;
    LOGD("==> [%d]bcmPlayer_uninit_rawdata", iPlayerIndex);

    playpump_status[iPlayerIndex] = STOP;
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = true;
    audioFlushing[iPlayerIndex] = true;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    B_Event_Set(videoDataEvent[iPlayerIndex]);
    B_Event_Set(audioDataEvent[iPlayerIndex]);

    if(nexus_handle[iPlayerIndex].playpump){
        NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].playpump);
    }
    if(nexus_handle[iPlayerIndex].audioPlaypump){
        NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].audioPlaypump);
    }

    if(nexus_handle[iPlayerIndex].simpleVideoDecoder)
        NEXUS_SimpleVideoDecoder_Stop(nexus_handle[iPlayerIndex].simpleVideoDecoder);

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder)
        NEXUS_SimpleAudioDecoder_Stop(nexus_handle[iPlayerIndex].simpleAudioDecoder);

    if(nexus_handle[iPlayerIndex].playback) {
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);
        
        if(status.state == NEXUS_PlaybackState_ePaused) {
            LOGW("[%d]%s: PLAYBACK HASN'T BEEN STARTED YET!", iPlayerIndex, __FUNCTION__);
        }

        if(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused) {
            NEXUS_Playback_Stop(nexus_handle[iPlayerIndex].playback);
                
            while(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
                  NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);
                    
            //usleep(2000000);/* wait for playback stoping */
        }
    }
    
    MUTEX_RAWDATA_LOCK(iPlayerIndex);

    if(nexus_handle[iPlayerIndex].playback)
        NEXUS_Playback_CloseAllPidChannels(nexus_handle[iPlayerIndex].playback);    

    if(nexus_handle[iPlayerIndex].playback) {
        NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
        nexus_handle[iPlayerIndex].playback = NULL;
    }

    if(nexus_handle[iPlayerIndex].videoPidChannel) {
        NEXUS_Playpump_ClosePidChannel(nexus_handle[iPlayerIndex].playpump, nexus_handle[iPlayerIndex].videoPidChannel);
        nexus_handle[iPlayerIndex].videoPidChannel = NULL;
    }
    
    if(nexus_handle[iPlayerIndex].audioPidChannel) {
        if(nexus_handle[iPlayerIndex].audioPlaypump) {
            NEXUS_Playpump_ClosePidChannel(nexus_handle[iPlayerIndex].audioPlaypump, nexus_handle[iPlayerIndex].audioPidChannel);
        }
        else {
            NEXUS_Playpump_ClosePidChannel(nexus_handle[iPlayerIndex].playpump, nexus_handle[iPlayerIndex].audioPidChannel);
        }
        nexus_handle[iPlayerIndex].audioPidChannel = NULL;
    }
    
    if(nexus_handle[iPlayerIndex].playpump) {
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
        nexus_handle[iPlayerIndex].playpump = NULL;
    }
    
    if(nexus_handle[iPlayerIndex].audioPlaypump) {
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].audioPlaypump);
        nexus_handle[iPlayerIndex].audioPlaypump = NULL;
    }
    
    if(nexus_handle[iPlayerIndex].simpleVideoDecoder) {
        nexus_handle[iPlayerIndex].ipcclient->releaseVideoDecoderHandle(nexus_handle[iPlayerIndex].simpleVideoDecoder);
        nexus_handle[iPlayerIndex].simpleVideoDecoder = NULL;
    }

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder) {
        nexus_handle[iPlayerIndex].ipcclient->releaseAudioDecoderHandle(nexus_handle[iPlayerIndex].simpleAudioDecoder);
        nexus_handle[iPlayerIndex].simpleAudioDecoder = NULL;
    }

    if(nexus_handle[iPlayerIndex].stcChannel) {
        NEXUS_StcChannel_Invalidate(nexus_handle[iPlayerIndex].stcChannel);
        NEXUS_StcChannel_Close(nexus_handle[iPlayerIndex].stcChannel);
        nexus_handle[iPlayerIndex].stcChannel = NULL;
    }

    /* disconnect sync channel */
    if(nexus_handle[iPlayerIndex].syncChannel) {
        NEXUS_SyncChannelSettings syncChannelSettings;
        NEXUS_SyncChannel_GetSettings(nexus_handle[iPlayerIndex].syncChannel, &syncChannelSettings);
        syncChannelSettings.videoInput = NULL;
        syncChannelSettings.audioInput[0] = NULL;
        syncChannelSettings.audioInput[1] = NULL;
        NEXUS_SyncChannel_SetSettings(nexus_handle[iPlayerIndex].syncChannel, &syncChannelSettings);
        NEXUS_SyncChannel_Destroy(nexus_handle[iPlayerIndex].syncChannel);
    }

    B_Event_Destroy(videoDataEvent[iPlayerIndex]);
    B_Event_Destroy(audioDataEvent[iPlayerIndex]);

    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    MUTEX_RAWDATA_DESTROY(iPlayerIndex);
}

static int bcmPlayer_setDataSource_rawdata(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{

    NEXUS_StcChannelSettings stcSettings;
    NEXUS_PidChannelHandle videoPidChannel;
    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_SimpleAudioDecoderStartSettings audioProgram;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_SyncChannelSettings syncChannelSettings;
    NEXUS_SimpleVideoDecoderServerSettings videoSettings;
    NEXUS_SimpleAudioDecoderServerSettings audioSettings;
    stream_format_info *pSfInfo;
    NEXUS_Error rc;

    LOGD("[%d]bcmPlayer_setDataSource_rawdata('%s')", iPlayerIndex, url); 
    LOGD("[%d]bcmPlayer_setDataSource_rawdata(extraHeader = '%s')", iPlayerIndex, extraHeader);

    pSfInfo = (stream_format_info *)&bcm_stream_format_info[iPlayerIndex];

    memset(pSfInfo, 0, sizeof(*pSfInfo));

    if (extraHeader)
    {
        probe_stream_format(extraHeader, nexus_handle[iPlayerIndex].videoTrackIndex, nexus_handle[iPlayerIndex].audioTrackIndex, pSfInfo);

        if(!pSfInfo->transportType) {
            LOGE("[%d]%s: Cannot support the stream type of file:%s !", iPlayerIndex, __FUNCTION__, url);
            return 1;
        }

        *videoWidth  = pSfInfo->videoWidth;
        *videoHeight = pSfInfo->videoHeight;
    }
    else{
        pSfInfo->videoWidth = *videoWidth;
        pSfInfo->videoHeight = *videoHeight; 
       // Video/Audio codec
       pSfInfo->videoCodec = NEXUS_VideoCodec_eH264;
       pSfInfo->audioCodec = NEXUS_AudioCodec_eAacAdts;

       // PES info
       pSfInfo->videoPid = 0xE0;
       pSfInfo->audioPid = 0xC0;
       pSfInfo->transportType = NEXUS_TransportType_eMpeg2Pes;
    }

   
    
    NEXUS_StcChannel_GetDefaultSettings(NEXUS_ANY_ID, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    stcSettings.modeSettings.Auto.transportType = pSfInfo->transportType;
//    stcSettings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eAudioMaster;
//    stcSettings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eVideoMaster;
#if 0
    stcSettings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eFirstAvailable;
    stcSettings.modeSettings.Auto.offsetThreshold = 30;
#endif
    nexus_handle[iPlayerIndex].stcChannel = NEXUS_StcChannel_Open(NEXUS_ANY_ID, &stcSettings);
    
    if(NULL == nexus_handle[iPlayerIndex].stcChannel) {
        LOGE("[%d]%s: stcChannel open fails!!!", iPlayerIndex, __FUNCTION__);
        return -1;
    }
        
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoPid   = %d", iPlayerIndex, pSfInfo->videoPid);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoCodec = %d", iPlayerIndex, pSfInfo->videoCodec);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioPid   = %d", iPlayerIndex, pSfInfo->audioPid);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioCodec = %d", iPlayerIndex, pSfInfo->audioCodec);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, w x h = %d x %d", iPlayerIndex, pSfInfo->videoWidth, pSfInfo->videoHeight);

    NEXUS_Playpump_GetSettings(nexus_handle[iPlayerIndex].playpump, &playpumpSettings);

    playpumpSettings.originalTransportType = NEXUS_TransportType_eUnknown;
    playpumpSettings.transportType = pSfInfo->transportType;

    NEXUS_CALLBACKDESC_INIT(&playpumpSettings.dataCallback);
    playpumpSettings.dataCallback.callback = spaceAvailableCallback;
    playpumpSettings.dataCallback.context = videoDataEvent[iPlayerIndex];
    playpumpSettings.dataCallback.param = iPlayerIndex;
    rc = NEXUS_Playpump_SetSettings(nexus_handle[iPlayerIndex].playpump, &playpumpSettings);

    if (rc != NEXUS_SUCCESS) {
        LOGE("[%d]%s: NEXUS_Playpump_SetSettings failed for playpump [rc=%d]!!!", iPlayerIndex, __FUNCTION__, rc);
        return false;
    }

    NEXUS_Playpump_GetSettings(nexus_handle[iPlayerIndex].audioPlaypump, &playpumpSettings);

    playpumpSettings.originalTransportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.transportType = pSfInfo->transportType;

    NEXUS_CALLBACKDESC_INIT(&playpumpSettings.dataCallback);
    playpumpSettings.dataCallback.callback = spaceAvailableCallback;
    playpumpSettings.dataCallback.context = audioDataEvent[iPlayerIndex];
    playpumpSettings.dataCallback.param = iPlayerIndex;

    rc = NEXUS_Playpump_SetSettings(nexus_handle[iPlayerIndex].audioPlaypump, &playpumpSettings);
    if (rc != NEXUS_SUCCESS) {
        LOGE("[%d]%s: NEXUS_Playpump_SetSettings failed for audioPlaypump [rc=%d]!!!", iPlayerIndex, __FUNCTION__, rc);
        return false;
    }

    if (pSfInfo->videoCodec != NEXUS_VideoCodec_eUnknown) {
        nexus_handle[iPlayerIndex].videoPidChannel = NEXUS_Playpump_OpenPidChannel(nexus_handle[iPlayerIndex].playpump, pSfInfo->videoPid, NULL);
        if(nexus_handle[iPlayerIndex].videoPidChannel) {    
            LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoPidChannel", iPlayerIndex);
            NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
            videoProgram.codec = pSfInfo->videoCodec;
            videoProgram.pidChannel = nexus_handle[iPlayerIndex].videoPidChannel;
            videoProgram.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
        }
    }
    
    if (pSfInfo->audioCodec != NEXUS_AudioCodec_eUnknown) {
        if (pSfInfo->transportType == NEXUS_TransportType_eTs) {
            nexus_handle[iPlayerIndex].audioPidChannel = NEXUS_Playpump_OpenPidChannel(nexus_handle[iPlayerIndex].playpump, pSfInfo->audioPid, NULL);
            if(nexus_handle[iPlayerIndex].audioPlaypump) {
                NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].audioPlaypump);
                nexus_handle[iPlayerIndex].audioPlaypump = NULL;
            }
        }
        else {
            nexus_handle[iPlayerIndex].audioPidChannel = NEXUS_Playpump_OpenPidChannel(nexus_handle[iPlayerIndex].audioPlaypump, pSfInfo->audioPid, NULL);
        }
            
        if(nexus_handle[iPlayerIndex].audioPidChannel) {    
            LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioPidChannel", iPlayerIndex);
            NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram); 
            audioProgram.primary.codec = pSfInfo->audioCodec;        
            audioProgram.primary.pidChannel = nexus_handle[iPlayerIndex].audioPidChannel;        
            audioProgram.primary.stcChannel = nexus_handle[iPlayerIndex].stcChannel;        
        }
    }


    /* connect sync channel */
    if(nexus_handle[iPlayerIndex].simpleVideoDecoder && nexus_handle[iPlayerIndex].simpleAudioDecoder) {
        NEXUS_SimpleVideoDecoder_GetServerSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoSettings);
        NEXUS_SimpleAudioDecoder_GetServerSettings(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioSettings);
        NEXUS_SyncChannel_GetSettings(nexus_handle[iPlayerIndex].syncChannel, &syncChannelSettings);
        syncChannelSettings.videoInput = NEXUS_VideoDecoder_GetConnector(videoSettings.videoDecoder);
        syncChannelSettings.audioInput[0] = NEXUS_AudioDecoder_GetConnector(audioSettings.primary, NEXUS_AudioDecoderConnectorType_eStereo);
        syncChannelSettings.audioInput[1] = NULL;
        //NEXUS_SyncChannel_SetSettings(nexus_handle[iPlayerIndex].syncChannel, &syncChannelSettings);
    }


    if(nexus_handle[iPlayerIndex].simpleVideoDecoder && nexus_handle[iPlayerIndex].videoPidChannel) {
        LOGV("[%d]bcmPlayer_setDataSource_rawdata, simpleVideoDecoder", iPlayerIndex);
        NEXUS_SimpleVideoDecoderStartSettings svdStartSettings;
        NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svdStartSettings);
        svdStartSettings.settings = videoProgram;

        NEXUS_SimpleVideoDecoder_Start(nexus_handle[iPlayerIndex].simpleVideoDecoder, &svdStartSettings);
    }
        
    if(nexus_handle[iPlayerIndex].simpleAudioDecoder && nexus_handle[iPlayerIndex].audioPidChannel) {    
        LOGV("[%d]bcmPlayer_setDataSource_rawdata, simpleAudioDecoder", iPlayerIndex);
        NEXUS_SimpleAudioDecoder_Start(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioProgram);
    }
    
    if(nexus_handle[iPlayerIndex].playpump)NEXUS_Playpump_Start(nexus_handle[iPlayerIndex].playpump);
    if(nexus_handle[iPlayerIndex].audioPlaypump)NEXUS_Playpump_Start(nexus_handle[iPlayerIndex].audioPlaypump);

    playpump_status[iPlayerIndex] = PLAY;

    LOGI("[%d]%s: video and audio started", iPlayerIndex, __FUNCTION__);

    return 0;
}

static int bcmPlayer_putData_rawdata(int iPlayerIndex, const void *rawdata, size_t data_len)
{
    void *buffer;
    size_t buffer_size;
    unsigned data_played = 0;
    char *data_buffer = (char *)rawdata;
    NEXUS_Error rc;
    uint16_t waitPutData=0;

    if(playpump_status[iPlayerIndex] == STOP){
        LOGE("[%d][VIDEO] playpump_status is STOP!!", iPlayerIndex);
        return 1;
    }
    if(nexus_handle[iPlayerIndex].playpump == NULL){
        LOGE("[%d][VIDEO] nexus_handle.playpump is NULL ..!!", iPlayerIndex);
        return 1;
    }
     
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = false;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    LOGV("[%d][VIDEO] data_len = %d; rawdata = %p\n\n", iPlayerIndex, data_len, rawdata);
    LOGV("[%d][VIDEO] nexus_handle.playpump = %p\n", iPlayerIndex, nexus_handle[iPlayerIndex].playpump);
    while(data_played < data_len && playpump_status[iPlayerIndex] != STOP && nexus_handle[iPlayerIndex].playpump)
    {
        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if (videoFlushing[iPlayerIndex]) {
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            LOGV("[%d]%s: video flushing, exiting loop!", iPlayerIndex, __FUNCTION__);
            return 0;
        }

        if(nexus_handle[iPlayerIndex].playpump){
            NEXUS_PlaypumpStatus status;

            NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &status);
            LOGV("[%d]Video playpump: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                    iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

            rc = NEXUS_Playpump_GetBuffer(nexus_handle[iPlayerIndex].playpump, &buffer, &buffer_size);
            if(rc != 0) {
                LOGE("[%d][VIDEO] bcmPlayer_putData_rawdata: NEXUS_Playpump_GetBuffer failed, rc:0x%x", iPlayerIndex, rc);
                MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
                return 1;
            }
        }
        MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        LOGV("[%d][VIDEO] data_len = %d; buffer_size = %d;\n\n", iPlayerIndex, data_len, buffer_size);
        if(buffer_size == 0) {
            LOGV("[%d]%s: Waiting for space in video playpump...", iPlayerIndex, __FUNCTION__);
            B_Event_Wait(videoDataEvent[iPlayerIndex], B_WAIT_FOREVER);
            continue;
        }
      
        if (buffer_size > (data_len - data_played)) {
            buffer_size = data_len - data_played;
        }

        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if(buffer_size > 0 && nexus_handle[iPlayerIndex].playpump && !videoFlushing[iPlayerIndex]) {
            BKNI_Memcpy(buffer,&data_buffer[data_played],buffer_size);
            rc = NEXUS_Playpump_WriteComplete(nexus_handle[iPlayerIndex].playpump, 0, buffer_size);
            if (rc != NEXUS_SUCCESS) {
                LOGE("[%d][VIDEO] bcmPlayer_putData_rawdata: NEXUS_Playpump_WriteComplete failed, rc:0x%x", iPlayerIndex, rc);
            }
            else{
                LOGV("[%d][VIDEO] putData_rawdata: played %d bytes", iPlayerIndex, buffer_size);
                data_played += buffer_size;
            }
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        }
        else{
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            usleep(10000);
           // LOGV("[%d][VIDEO] bcmPlayer_putData_rawdata: buffer_size:%d, data_len:%d ..!!", iPlayerIndex, buffer_size,data_len);
        }
    }
     
    LOGV("[%d]Exiting %s", iPlayerIndex, __FUNCTION__);
    return 0;
}
      
static int bcmPlayer_putAudioData_rawdata(int iPlayerIndex, const void *rawdata, size_t data_len) 
{
    void *buffer;
    size_t buffer_size;
    unsigned data_played = 0;
    char *data_buffer = (char *)rawdata;

    NEXUS_Error rc;

    if(playpump_status[iPlayerIndex] == STOP){
        LOGE("[%d][AUDIO] playpump_status is STOP!!", iPlayerIndex);
        return 1;
    }
    if(nexus_handle[iPlayerIndex].audioPlaypump == NULL){
        LOGE("[%d][AUDIO] nexus_handle.audioPlaypump is NULL ..!!", iPlayerIndex);
        return 1;
    }

    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    audioFlushing[iPlayerIndex] = false;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    LOGV("[%d][AUDIO] data_len = %d; rawdata = %p\n\n", iPlayerIndex, data_len, rawdata);
    LOGV("[%d][AUDIO] header = 0x%08x", iPlayerIndex, *(unsigned int *)rawdata);
    LOGV("[%d][AUDIO] nexus_handle.playpump = %p\n", iPlayerIndex, nexus_handle[iPlayerIndex].audioPlaypump);
     
    while (data_played < data_len && playpump_status[iPlayerIndex] != STOP && nexus_handle[iPlayerIndex].audioPlaypump) 
    {
        void *playpump_buffer;
        unsigned buffer_size;

        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if (audioFlushing[iPlayerIndex]) {
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            LOGV("[%d]%s: audio flushing, exiting loop!", iPlayerIndex, __FUNCTION__);
            return 0;
        }

        if(nexus_handle[iPlayerIndex].audioPlaypump){
            NEXUS_PlaypumpStatus status;

            NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].audioPlaypump, &status);
            LOGV("[%d]Audio playpump: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                    iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

            if (forcePause[iPlayerIndex] && playpump_status[iPlayerIndex] == PLAY) {
                if (status.descFifoDepth > 0) {
                    NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 1, 0);
                    LOGV("[%d]%s: Unfreezing", iPlayerIndex, __FUNCTION__);
                    forcePause[iPlayerIndex] = false;
                }
            }

            rc = NEXUS_Playpump_GetBuffer(nexus_handle[iPlayerIndex].audioPlaypump, &buffer, &buffer_size);
            if (rc != NEXUS_SUCCESS) {
                LOGE("[%d][AUDIO] NEXUS_Playpump_GetBuffer failed!!, rc:0x%x", iPlayerIndex, rc);
                MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
                return rc;
            }
        }
        MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

        LOGV("[%d][AUDIO] data_len=%d; buffer_size=%d;\n\n", iPlayerIndex, data_len, buffer_size);

        if (buffer_size == 0) {
            LOGV("[%d]%s: Waiting for space in audio playpump...", iPlayerIndex, __FUNCTION__);
            B_Event_Wait(audioDataEvent[iPlayerIndex], B_WAIT_FOREVER);
            continue;
        }

        if (buffer_size > (data_len - data_played)) {
            buffer_size = data_len - data_played;
        }
     
        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if(buffer_size>0 && nexus_handle[iPlayerIndex].audioPlaypump && !audioFlushing[iPlayerIndex]) {
            BKNI_Memcpy(buffer, &data_buffer[data_played], buffer_size);
         
            rc = NEXUS_Playpump_WriteComplete(nexus_handle[iPlayerIndex].audioPlaypump, 0, buffer_size);
            if(rc != NEXUS_SUCCESS){
                LOGE("[%d][AUDIO] NEXUS_Playpump_WriteComplete failed!! rc:0x%x", iPlayerIndex, rc);
            }
            else{
                LOGV("[%d][AUDIO] audioPutData_rawdata: played %d bytes", iPlayerIndex, buffer_size);
                data_played += buffer_size;
            }
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        }
        else{
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            usleep(10000);
            //LOGV("[%d][AUDIO] audioPutData_rawdata: buffer_size:%d, data_len:%d ..!!", iPlayerIndex, buffer_size,data_len);
        }
    }

    LOGV("[%d]Exiting %s", iPlayerIndex, __FUNCTION__);
    return 0;
}
         
static int bcmPlayer_start_rawdata(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_start_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] == PAUSE)
    {
        if (nexus_handle[iPlayerIndex].stcChannel) {
            if(nexus_handle[iPlayerIndex].audioPlaypump) {
                NEXUS_PlaypumpStatus status;

                NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].audioPlaypump, &status);
                LOGV("[%d]Audio playpump start: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                        iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

                if (status.descFifoDepth > 0) {
                    NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 1, 0);
                    forcePause[iPlayerIndex] = false;
                    LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
                }
                else {
                    forcePause[iPlayerIndex] = true;
                }
            }
            else if(nexus_handle[iPlayerIndex].playpump) {
                NEXUS_PlaypumpStatus status;

                NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &status);
                LOGV("[%d]Playpump start: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                        iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

                if (status.fifoDepth > (status.fifoSize / 2)) {
                    NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 1, 0);
                    forcePause[iPlayerIndex] = false;
                    LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
                }
                else {
                    forcePause[iPlayerIndex] = true;
                }

            }
            else {
                NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 1, 0);
                LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
            }
        }
    }

    playpump_status[iPlayerIndex] = PLAY;
    return 0;
}
     
static int bcmPlayer_stop_rawdata(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_stop_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] != STOP) {
        if (nexus_handle[iPlayerIndex].audioPlaypump != NULL)
            NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].audioPlaypump);

        if (nexus_handle[iPlayerIndex].playpump != NULL)
            NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].playpump);
    }

    playpump_status[iPlayerIndex] = STOP;
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = false;
    audioFlushing[iPlayerIndex] = false;
    forcePause[iPlayerIndex] = false;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

    B_Event_Set(videoDataEvent[iPlayerIndex]);
    B_Event_Set(audioDataEvent[iPlayerIndex]);
    return 0;
}

static int bcmPlayer_pause_rawdata(int iPlayerIndex) 
{
    LOGD("[%d]bcmPlayer_pause_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] == PLAY) {
        if (nexus_handle[iPlayerIndex].stcChannel) {
            NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 0, 0);
        }
    }

    playpump_status[iPlayerIndex] = PAUSE;
    return 0;
}

static int bcmPlayer_get_position_rawdata(int iPlayerIndex, int *msec) 
{
    NEXUS_Error              nexusRC;
    NEXUS_VideoDecoderStatus decoderStatus;

    nexusRC = NEXUS_SUCCESS;
  
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    
    if (nexus_handle[iPlayerIndex].simpleVideoDecoder != NULL) {
        nexusRC = NEXUS_SimpleVideoDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleVideoDecoder, &decoderStatus );
        if (nexusRC != NEXUS_SUCCESS) {
            LOGE("[%d]%s: NEXUS_SimpleVideoDecoder_GetStatus returned %d!", iPlayerIndex, __FUNCTION__, nexusRC);
        }
        else {
            if(decoderStatus.firstPtsPassed && decoderStatus.pts) {
                *msec = (int)decoderStatus.pts/45;
            }
            else {
                *msec = 0;
            }
        }
    }

    LOGV("[%d]%s=> PTS: %d msec \n", iPlayerIndex, __FUNCTION__, *msec);

    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    return 0;
}

int bcmPlayer_seekto_rawdata(int iPlayerIndex, int msec)
{
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = true;
    audioFlushing[iPlayerIndex] = true;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

    /* The Data seek is handled in NexusPlayer, this is here to flush the pipeline. */
    if(nexus_handle[iPlayerIndex].playpump){
        NEXUS_Playpump_SetPause(nexus_handle[iPlayerIndex].playpump, true);
        NEXUS_Playpump_Flush(nexus_handle[iPlayerIndex].playpump);
    }

    if(nexus_handle[iPlayerIndex].audioPlaypump){
        NEXUS_Playpump_SetPause(nexus_handle[iPlayerIndex].audioPlaypump, true);
        NEXUS_Playpump_Flush(nexus_handle[iPlayerIndex].audioPlaypump);
    }

    if(nexus_handle[iPlayerIndex].simpleVideoDecoder)
        NEXUS_SimpleVideoDecoder_Flush(nexus_handle[iPlayerIndex].simpleVideoDecoder);

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder)
        NEXUS_SimpleAudioDecoder_Flush(nexus_handle[iPlayerIndex].simpleAudioDecoder);

    if(nexus_handle[iPlayerIndex].stcChannel) {
        NEXUS_StcChannel_Invalidate(nexus_handle[iPlayerIndex].stcChannel);
        //if (playpump_status[iPlayerIndex] == PAUSE)
        {
            forcePause[iPlayerIndex] = true;
            NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 0, 0);
            LOGV("[%d]%s: Stc Channel frozen", iPlayerIndex, __FUNCTION__);
        }
    }

    if(nexus_handle[iPlayerIndex].playpump){
        NEXUS_Playpump_SetPause(nexus_handle[iPlayerIndex].playpump, false);
    }

    if(nexus_handle[iPlayerIndex].audioPlaypump){
        NEXUS_Playpump_SetPause(nexus_handle[iPlayerIndex].audioPlaypump, false);
    }


    if (nexus_handle[iPlayerIndex].playpump)
    {
        NEXUS_PlaypumpStatus status;

        NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &status);
        LOGV("[%d]%s: Video playpump: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                iPlayerIndex, __FUNCTION__, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);
    }
    B_Event_Set(videoDataEvent[iPlayerIndex]);
    B_Event_Set(audioDataEvent[iPlayerIndex]);

    return 0;
}

int bcmPlayer_isPlaying_rawdata(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_isPlaying_rawdata", iPlayerIndex);

    return (playpump_status[iPlayerIndex] == PLAY);
}


int bcmPlayer_prepare_rawdata(int iPlayerIndex)
{
    NEXUS_Error rc = 0;

    LOGD("[%d]%s", iPlayerIndex, __FUNCTION__);
    
    if(nexus_handle[iPlayerIndex].stcChannel) {
        forcePause[iPlayerIndex] = true;
        if (playpump_status[iPlayerIndex] != PAUSE) {
            rc = NEXUS_StcChannel_SetRate(nexus_handle[iPlayerIndex].stcChannel, 0, 0);
            if (rc == NEXUS_SUCCESS) {
                playpump_status[iPlayerIndex] = PAUSE;
                LOGV("[%d]%s: Stc Channel frozen", iPlayerIndex, __FUNCTION__);

                if(nexus_handle[iPlayerIndex].audioPlaypump) {
                    do {
                        NEXUS_PlaypumpStatus audioStatus;

                        NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].audioPlaypump, &audioStatus);

                        LOGV("[%d]%s[AUDIO]: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld", iPlayerIndex, __FUNCTION__,
                                audioStatus.fifoDepth, audioStatus.fifoSize, audioStatus.descFifoDepth, audioStatus.descFifoSize, audioStatus.bytesPlayed);

                        if (audioStatus.descFifoDepth > 0) {
                            LOGV("[%d]%s: audio fifo ready", iPlayerIndex, __FUNCTION__);
                            break;
                        }
                        usleep(100000);
                    } while (playpump_status[iPlayerIndex] != STOP);
                }
                else if(nexus_handle[iPlayerIndex].playpump) {
                    do {
                        NEXUS_PlaypumpStatus pumpStatus;

                        NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &pumpStatus);

                        LOGV("[%d]%s[PUMP]: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld", iPlayerIndex, __FUNCTION__,
                                pumpStatus.fifoDepth, pumpStatus.fifoSize, pumpStatus.descFifoDepth, pumpStatus.descFifoSize, pumpStatus.bytesPlayed);

                        if (pumpStatus.fifoDepth > (pumpStatus.fifoSize / 2)) {
                            LOGV("[%d]%s: pump fifo ready", iPlayerIndex, __FUNCTION__);
                            break;
                        }
                        usleep(100000);
                    } while (playpump_status[iPlayerIndex] != STOP);
                }
            }
        }
    }
    return rc;
}

int bcmPlayer_reset_rawdata(int iPlayerIndex)
{
    LOGD("[%d]%s", iPlayerIndex, __FUNCTION__);

    return bcmPlayer_stop_rawdata(iPlayerIndex);
}


int bcmPlayer_getFifoDepth_rawdata(int iPlayerIndex, bcmPlayer_fifo_t fifo, int *depth) {

    LOGD("[%d]%s: fifo %d", iPlayerIndex, __FUNCTION__, fifo);

    if(depth == NULL) {
        LOGE("[%d]%s: depth=NULL!!!", iPlayerIndex, __FUNCTION__);
        return 1;
    }

    MUTEX_RAWDATA_LOCK(iPlayerIndex);

    switch (fifo)
    {
        case FIFO_VIDEO:
            if (nexus_handle[iPlayerIndex].simpleVideoDecoder != NULL) {
                NEXUS_VideoDecoderStatus decoderStatus;
                if (NEXUS_SimpleVideoDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleVideoDecoder, &decoderStatus ) == NEXUS_SUCCESS) {
                    LOGV("[%d]%s:[ms=%d (pts passed=%d)] fifoDepth=%d, fifoSize=%d, queueDepth=%d", iPlayerIndex, __FUNCTION__,
                            (int)decoderStatus.pts/45, decoderStatus.firstPtsPassed, decoderStatus.fifoDepth, decoderStatus.fifoSize, decoderStatus.queueDepth); 
                    *depth = decoderStatus.fifoDepth;
                } else {
                    LOGE("[%d]%s: NEXUS_SimpleVideoDecoder_GetStatus != NEXUS_SUCCESS", iPlayerIndex, __FUNCTION__);
                    *depth = 0;
                }
            }
            break;
        case FIFO_AUDIO:
            if (nexus_handle[iPlayerIndex].simpleAudioDecoder != NULL) {
                NEXUS_AudioDecoderStatus decoderStatus;
                if (NEXUS_SimpleAudioDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleAudioDecoder, &decoderStatus ) == NEXUS_SUCCESS) {
                    LOGV("[%d]%s:[ms=%d] fifoDepth=%d, fifoSize=%d, queuedFrames=%d", iPlayerIndex, __FUNCTION__,
                            (int)decoderStatus.pts/45, decoderStatus.fifoDepth, decoderStatus.fifoSize, decoderStatus.queuedFrames); 
                    *depth = decoderStatus.fifoDepth;
                } else {
                    LOGE("[%d]%s: NEXUS_SimpleAudioDecoder_GetStatus != NEXUS_SUCCESS", iPlayerIndex, __FUNCTION__);
                    *depth = 0;
                }
            }
            break;
        default:
            *depth = 0;
    }

    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    return 0;
}

int bcmPlayer_getMediaExtractorFlags_rawdata(int iPlayerIndex, unsigned *flags)
{
    char *url = nexus_handle[iPlayerIndex].url;

    /* Don't allow trick-play operations on RTP/UDP sources */
    if (strncmp("brcm:raw_data:udprtp", url, 20) == 0) {
        *flags = 0;
    }
    else {
        *flags = ( CAN_SEEK_BACKWARD |
                   CAN_SEEK_FORWARD  |
                   CAN_PAUSE         |
                   CAN_SEEK );
    }
    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);
    return 0;
}

void player_reg_rawdata(bcmPlayer_func_t *pFuncs)
{
    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_rawdata;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_rawdata;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_rawdata;
    pFuncs->bcmPlayer_putData_func = bcmPlayer_putData_rawdata;
    pFuncs->bcmPlayer_putAudioData_func = bcmPlayer_putAudioData_rawdata;
    pFuncs->bcmPlayer_start_func = bcmPlayer_start_rawdata;
    pFuncs->bcmPlayer_stop_func = bcmPlayer_stop_rawdata;
    pFuncs->bcmPlayer_pause_func = bcmPlayer_pause_rawdata;
    pFuncs->bcmPlayer_getCurrentPosition_func = bcmPlayer_get_position_rawdata;
    pFuncs->bcmPlayer_seekTo_func = bcmPlayer_seekto_rawdata;
    pFuncs->bcmPlayer_isPlaying_func = bcmPlayer_isPlaying_rawdata;
    pFuncs->bcmPlayer_prepare_func = bcmPlayer_prepare_rawdata;
    pFuncs->bcmPlayer_reset_func = bcmPlayer_reset_rawdata;
    pFuncs->bcmPlayer_getFifoDepth_func = bcmPlayer_getFifoDepth_rawdata;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_rawdata;
}
