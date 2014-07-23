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
 * $brcm_Workfile: bcmPlayer_rawdata.cpp $
 *
 *****************************************************************************/
// Verbose messages removed
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
    LOGV("---> [%d]%s(%p)", param, __FUNCTION__, context);
    B_Event_Set(context);
}

static int bcmPlayer_init_rawdata(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
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

    if (nexusHandle->playback) {
        NEXUS_Playback_Destroy(nexusHandle->playback);
        nexusHandle->playback = NULL;
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
            nexusHandle->audioPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, pPlaypumpOpenSettings);
            if (pClientConfig) {
                BKNI_Free(pClientConfig);
            }
            if (pPlaypumpOpenSettings) {
                BKNI_Free(pPlaypumpOpenSettings);
            }
            if (nexusHandle->audioPlaypump == NULL) {
                LOGE("[%d]%s: Nexus playpump open failed!", iPlayerIndex, __FUNCTION__);
                rc = 1;
            }
            else {
                // Now connect the client resources
                b_refsw_client_client_info client_info;
                nexusHandle->ipcclient->getClientInfo(nexusHandle->nexus_client, &client_info);

                b_refsw_client_connect_resource_settings connectSettings;

                nexusHandle->ipcclient->getDefaultConnectClientSettings(&connectSettings);

                connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
                connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
                connectSettings.simpleVideoDecoder[0].windowId = iPlayerIndex; /* Main or PIP Window */
                if (nexusHandle->bSupportsHEVC == true) {
                    if ((nexusHandle->maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                        (nexusHandle->maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))
                    {
                        connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
                        connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
                    }
                    connectSettings.simpleVideoDecoder[0].decoderCaps.supportedCodecs[NEXUS_VideoCodec_eH265] = true;
                }
                connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

                if (nexusHandle->ipcclient->connectClientResources(nexusHandle->nexus_client, &connectSettings) != true) {
                    LOGE("[%d]%s: Could not connect client \"%s\" resources!", iPlayerIndex, __FUNCTION__,
                          nexusHandle->ipcclient->getClientName());
                    NEXUS_Playpump_Close(nexusHandle->audioPlaypump);
                    nexusHandle->audioPlaypump = NULL;
                    rc = 1;
                }

                videoFlushing[iPlayerIndex] = false;
                audioFlushing[iPlayerIndex] = false;
                forcePause[iPlayerIndex] = false;
            }
        }
    }
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

    return rc;
}

    
static void bcmPlayer_uninit_rawdata(int iPlayerIndex) 
{
    LOGD("==> [%d]bcmPlayer_uninit_rawdata", iPlayerIndex);

    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    playpump_status[iPlayerIndex] = STOP;
    videoFlushing[iPlayerIndex] = true;
    audioFlushing[iPlayerIndex] = true;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    B_Event_Set(videoDataEvent[iPlayerIndex]);
    B_Event_Set(audioDataEvent[iPlayerIndex]);

    MUTEX_RAWDATA_LOCK(iPlayerIndex);

    bcmPlayer_uninit_base(iPlayerIndex);

    B_Event_Destroy(videoDataEvent[iPlayerIndex]);
    B_Event_Destroy(audioDataEvent[iPlayerIndex]);

    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    MUTEX_RAWDATA_DESTROY(iPlayerIndex);
}

static int bcmPlayer_setDataSource_rawdata(int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    NEXUS_SimpleStcChannelSettings stcSettings;
    NEXUS_PidChannelHandle videoPidChannel;
    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_VideoDecoderSettings videoDecoderSettings;
    NEXUS_SimpleAudioDecoderStartSettings audioProgram;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_SimpleVideoDecoderServerSettings videoSettings;
    NEXUS_SimpleAudioDecoderServerSettings audioSettings;
    stream_format_info *pSfInfo;
    NEXUS_Error rc;

    LOGD("[%d]bcmPlayer_setDataSource_rawdata('%s')", iPlayerIndex, url); 
    LOGD("[%d]bcmPlayer_setDataSource_rawdata(extraHeader = '%s')", iPlayerIndex, extraHeader);

    pSfInfo = (stream_format_info *)&bcm_stream_format_info[iPlayerIndex];

    memset(pSfInfo, 0, sizeof(*pSfInfo));

    if (extraHeader) {
        probe_stream_format(extraHeader, nexusHandle->videoTrackIndex, nexusHandle->audioTrackIndex, pSfInfo);

        if (!pSfInfo->transportType) {
            LOGE("[%d]%s: Cannot support the stream type of file:%s !", iPlayerIndex, __FUNCTION__, url);
            return 1;
        }

        *videoWidth  = pSfInfo->videoWidth;
        *videoHeight = pSfInfo->videoHeight;
    }
    else {
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

    if (nexusHandle->simpleStcChannel) {
        NEXUS_SimpleStcChannel_GetSettings(nexusHandle->simpleStcChannel, &stcSettings);
        stcSettings.mode = NEXUS_StcChannelMode_eAuto;
        stcSettings.modeSettings.Auto.transportType = pSfInfo->transportType;
        rc = NEXUS_SimpleStcChannel_SetSettings(nexusHandle->simpleStcChannel, &stcSettings);
    }
    
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoPid   = %d", iPlayerIndex, pSfInfo->videoPid);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoCodec = %d", iPlayerIndex, pSfInfo->videoCodec);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioPid   = %d", iPlayerIndex, pSfInfo->audioPid);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioCodec = %d", iPlayerIndex, pSfInfo->audioCodec);
    LOGV("[%d]bcmPlayer_setDataSource_rawdata, w x h = %d x %d", iPlayerIndex, pSfInfo->videoWidth, pSfInfo->videoHeight);

    NEXUS_Playpump_GetSettings(nexusHandle->playpump, &playpumpSettings);

    playpumpSettings.originalTransportType = NEXUS_TransportType_eUnknown;
    playpumpSettings.transportType = pSfInfo->transportType;

    NEXUS_CALLBACKDESC_INIT(&playpumpSettings.dataCallback);
    playpumpSettings.dataCallback.callback = spaceAvailableCallback;
    playpumpSettings.dataCallback.context = videoDataEvent[iPlayerIndex];
    playpumpSettings.dataCallback.param = iPlayerIndex;
    rc = NEXUS_Playpump_SetSettings(nexusHandle->playpump, &playpumpSettings);

    if (rc != NEXUS_SUCCESS) {
        LOGE("[%d]%s: NEXUS_Playpump_SetSettings failed for playpump [rc=%d]!!!", iPlayerIndex, __FUNCTION__, rc);
        return 1;
    }

    NEXUS_Playpump_GetSettings(nexusHandle->audioPlaypump, &playpumpSettings);

    playpumpSettings.originalTransportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.transportType = pSfInfo->transportType;

    NEXUS_CALLBACKDESC_INIT(&playpumpSettings.dataCallback);
    playpumpSettings.dataCallback.callback = spaceAvailableCallback;
    playpumpSettings.dataCallback.context = audioDataEvent[iPlayerIndex];
    playpumpSettings.dataCallback.param = iPlayerIndex;

    rc = NEXUS_Playpump_SetSettings(nexusHandle->audioPlaypump, &playpumpSettings);
    if (rc != NEXUS_SUCCESS) {
        LOGE("[%d]%s: NEXUS_Playpump_SetSettings failed for audioPlaypump [rc=%d]!!!", iPlayerIndex, __FUNCTION__, rc);
        return 1;
    }

    if (pSfInfo->videoCodec != NEXUS_VideoCodec_eUnknown) {
        nexusHandle->videoPidChannel = NEXUS_Playpump_OpenPidChannel(nexusHandle->playpump, pSfInfo->videoPid, NULL);
        if (nexusHandle->videoPidChannel) {    
            LOGV("[%d]bcmPlayer_setDataSource_rawdata, videoPidChannel", iPlayerIndex);
            NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
            videoProgram.codec = pSfInfo->videoCodec;
            videoProgram.pidChannel = nexusHandle->videoPidChannel;
        }
    }
    
    if (pSfInfo->audioCodec != NEXUS_AudioCodec_eUnknown) {
        if (pSfInfo->transportType == NEXUS_TransportType_eTs) {
            nexusHandle->audioPidChannel = NEXUS_Playpump_OpenPidChannel(nexusHandle->playpump, pSfInfo->audioPid, NULL);
            if (nexusHandle->audioPlaypump) {
                NEXUS_Playpump_Close(nexusHandle->audioPlaypump);
                nexusHandle->audioPlaypump = NULL;
            }
        }
        else {
            nexusHandle->audioPidChannel = NEXUS_Playpump_OpenPidChannel(nexusHandle->audioPlaypump, pSfInfo->audioPid, NULL);
        }
            
        if (nexusHandle->audioPidChannel) {    
            LOGV("[%d]bcmPlayer_setDataSource_rawdata, audioPidChannel", iPlayerIndex);
            NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram); 
            audioProgram.primary.codec = pSfInfo->audioCodec;        
            audioProgram.primary.pidChannel = nexusHandle->audioPidChannel;        
        }
    }

    /* Setup A/V decoder stc channels before starting decoding... */
    if (nexusHandle->simpleVideoDecoder && nexusHandle->videoPidChannel) {
        rc = NEXUS_SimpleVideoDecoder_SetStcChannel(nexusHandle->simpleVideoDecoder, nexusHandle->simpleStcChannel);
        if (rc != NEXUS_SUCCESS) {
            LOGE("[%d]%s: Could not set Simple Video Decoder Stc Channel!", iPlayerIndex, __FUNCTION__);
            if (nexusHandle->audioPidChannel) {
                NEXUS_Playpump_ClosePidChannel(nexusHandle->audioPlaypump, nexusHandle->audioPidChannel);
                nexusHandle->audioPidChannel = NULL;
            }
            if (nexusHandle->videoPidChannel) {
                NEXUS_Playpump_ClosePidChannel(nexusHandle->playpump, nexusHandle->videoPidChannel);
                nexusHandle->videoPidChannel = NULL;
            }
            return 1;
        }
    }

    if (nexusHandle->simpleAudioDecoder && nexusHandle->audioPidChannel) {
        rc = NEXUS_SimpleAudioDecoder_SetStcChannel(nexusHandle->simpleAudioDecoder, nexusHandle->simpleStcChannel);
        if (rc != NEXUS_SUCCESS) {
            LOGE("[%d]%s: Could not set Simple Audio Decoder Stc Channel!", iPlayerIndex, __FUNCTION__);
            if (nexusHandle->audioPidChannel) {
                NEXUS_Playpump_ClosePidChannel(nexusHandle->audioPlaypump, nexusHandle->audioPidChannel);
                nexusHandle->audioPidChannel = NULL;
            }
            if (nexusHandle->videoPidChannel) {
                NEXUS_Playpump_ClosePidChannel(nexusHandle->playpump, nexusHandle->videoPidChannel);
                nexusHandle->videoPidChannel = NULL;
            }
            return 1;
        }
    }

    if (nexusHandle->simpleVideoDecoder && nexusHandle->videoPidChannel) {
        LOGV("[%d]bcmPlayer_setDataSource_rawdata, simpleVideoDecoder", iPlayerIndex);
        NEXUS_SimpleVideoDecoderStartSettings svdStartSettings;

        NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svdStartSettings);
        svdStartSettings.settings = videoProgram;
        NEXUS_SimpleVideoDecoder_GetSettings(nexusHandle->simpleVideoDecoder, &videoDecoderSettings);
        if ((iPlayerIndex == 0) && 
            (videoDecoderSettings.supportedCodecs[NEXUS_VideoCodec_eH265]) &&
            ((nexusHandle->maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                (nexusHandle->maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))) {
                svdStartSettings.maxWidth  = 3840;
                svdStartSettings.maxHeight = 2160;
        }
        NEXUS_SimpleVideoDecoder_Start(nexusHandle->simpleVideoDecoder, &svdStartSettings);
    }
        
    if (nexusHandle->simpleAudioDecoder && nexusHandle->audioPidChannel) {    
        LOGV("[%d]bcmPlayer_setDataSource_rawdata, simpleAudioDecoder", iPlayerIndex);
        NEXUS_SimpleAudioDecoder_Start(nexusHandle->simpleAudioDecoder, &audioProgram);
    }
    
    if (nexusHandle->playpump) {
        NEXUS_Playpump_Start(nexusHandle->playpump);
    }

    if (nexusHandle->audioPlaypump) {
        NEXUS_Playpump_Start(nexusHandle->audioPlaypump);
    }

    playpump_status[iPlayerIndex] = PLAY;

    LOGI("[%d]%s: video and audio started", iPlayerIndex, __FUNCTION__);

    return 0;
}

static int bcmPlayer_putData_rawdata(int iPlayerIndex, const void *rawdata, size_t data_len)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    void *buffer;
    size_t buffer_size;
    unsigned data_played = 0;
    char *data_buffer = (char *)rawdata;
    NEXUS_Error rc;
    uint16_t waitPutData=0;

    if (playpump_status[iPlayerIndex] == STOP) {
        LOGE("[%d][VIDEO] playpump_status is STOP!!", iPlayerIndex);
        return 1;
    }
    if (nexusHandle->playpump == NULL) {
        LOGE("[%d][VIDEO] nexus_handle.playpump is NULL ..!!", iPlayerIndex);
        return 1;
    }
     
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = false;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    LOGV("[%d][VIDEO] data_len = %d; rawdata = %p\n\n", iPlayerIndex, data_len, rawdata);
    LOGV("[%d][VIDEO] nexus_handle.playpump = %p\n", iPlayerIndex, nexusHandle->playpump);
    while(data_played < data_len && playpump_status[iPlayerIndex] != STOP && nexusHandle->playpump) {
        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if (videoFlushing[iPlayerIndex]) {
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            LOGV("[%d]%s: video flushing, exiting loop!", iPlayerIndex, __FUNCTION__);
            return 0;
        }

        if (nexusHandle->playpump) {
            NEXUS_PlaypumpStatus status;

            NEXUS_Playpump_GetStatus(nexusHandle->playpump, &status);
            LOGV("[%d]Video playpump: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                    iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

            rc = NEXUS_Playpump_GetBuffer(nexusHandle->playpump, &buffer, &buffer_size);
            if (rc != 0) {
                LOGE("[%d][VIDEO] bcmPlayer_putData_rawdata: NEXUS_Playpump_GetBuffer failed, rc:0x%x", iPlayerIndex, rc);
                MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
                return 1;
            }
        }
        MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        LOGV("[%d][VIDEO] data_len = %d; buffer_size = %d;\n\n", iPlayerIndex, data_len, buffer_size);

        if (buffer_size == 0) {
            LOGV("[%d]%s: Waiting for space in video playpump...", iPlayerIndex, __FUNCTION__);
            B_Event_Wait(videoDataEvent[iPlayerIndex], B_WAIT_FOREVER);
            continue;
        }
      
        if (buffer_size > (data_len - data_played)) {
            buffer_size = data_len - data_played;
        }

        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if (buffer_size > 0 && nexusHandle->playpump && !videoFlushing[iPlayerIndex]) {
            BKNI_Memcpy(buffer,&data_buffer[data_played],buffer_size);
            rc = NEXUS_Playpump_WriteComplete(nexusHandle->playpump, 0, buffer_size);
            if (rc != NEXUS_SUCCESS) {
                LOGE("[%d][VIDEO] bcmPlayer_putData_rawdata: NEXUS_Playpump_WriteComplete failed, rc:0x%x", iPlayerIndex, rc);
            }
            else {
                LOGV("[%d][VIDEO] putData_rawdata: played %d bytes", iPlayerIndex, buffer_size);
                data_played += buffer_size;
            }
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        }
        else {
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
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    void *buffer;
    size_t buffer_size;
    unsigned data_played = 0;
    char *data_buffer = (char *)rawdata;

    NEXUS_Error rc;

    if (playpump_status[iPlayerIndex] == STOP) {
        LOGE("[%d][AUDIO] playpump_status is STOP!!", iPlayerIndex);
        return 1;
    }
    if (nexusHandle->audioPlaypump == NULL) {
        LOGE("[%d][AUDIO] nexus_handle.audioPlaypump is NULL ..!!", iPlayerIndex);
        return 1;
    }

    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    audioFlushing[iPlayerIndex] = false;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
    
    LOGV("[%d][AUDIO] data_len = %d; rawdata = %p\n\n", iPlayerIndex, data_len, rawdata);
    LOGV("[%d][AUDIO] header = 0x%08x", iPlayerIndex, *(unsigned int *)rawdata);
    LOGV("[%d][AUDIO] nexus_handle.playpump = %p\n", iPlayerIndex, nexusHandle->audioPlaypump);
     
    while (data_played < data_len && playpump_status[iPlayerIndex] != STOP && nexusHandle->audioPlaypump) {
        void *playpump_buffer;
        unsigned buffer_size;

        MUTEX_RAWDATA_LOCK(iPlayerIndex);
        if (audioFlushing[iPlayerIndex]) {
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
            LOGV("[%d]%s: audio flushing, exiting loop!", iPlayerIndex, __FUNCTION__);
            return 0;
        }

        if (nexusHandle->audioPlaypump) {
            NEXUS_PlaypumpStatus status;

            NEXUS_Playpump_GetStatus(nexusHandle->audioPlaypump, &status);
            LOGV("[%d]Audio playpump: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                    iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

            if (forcePause[iPlayerIndex] && playpump_status[iPlayerIndex] == PLAY) {
                if (status.descFifoDepth > 0) {
                    if (nexusHandle->simpleStcChannel) {
                        NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, false);
                        LOGV("[%d]%s: Unfreezing", iPlayerIndex, __FUNCTION__);
                        forcePause[iPlayerIndex] = false;
                    }
                }
            }

            rc = NEXUS_Playpump_GetBuffer(nexusHandle->audioPlaypump, &buffer, &buffer_size);
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
        if (buffer_size>0 && nexusHandle->audioPlaypump && !audioFlushing[iPlayerIndex]) {
            BKNI_Memcpy(buffer, &data_buffer[data_played], buffer_size);
         
            rc = NEXUS_Playpump_WriteComplete(nexusHandle->audioPlaypump, 0, buffer_size);
            if (rc != NEXUS_SUCCESS) {
                LOGE("[%d][AUDIO] NEXUS_Playpump_WriteComplete failed!! rc:0x%x", iPlayerIndex, rc);
            }
            else {
                LOGV("[%d][AUDIO] audioPutData_rawdata: played %d bytes", iPlayerIndex, buffer_size);
                data_played += buffer_size;
            }
            MUTEX_RAWDATA_UNLOCK(iPlayerIndex);
        }
        else {
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
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_start_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] == PAUSE) {
        if (nexusHandle->simpleStcChannel) {
            if (nexusHandle->audioPlaypump) {
                NEXUS_PlaypumpStatus status;

                NEXUS_Playpump_GetStatus(nexusHandle->audioPlaypump, &status);
                LOGV("[%d]Audio playpump start: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                        iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

                if (status.descFifoDepth > 0) {
                    NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, false);
                    forcePause[iPlayerIndex] = false;
                    LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
                }
                else {
                    forcePause[iPlayerIndex] = true;
                }
            }
            else if (nexusHandle->playpump) {
                NEXUS_PlaypumpStatus status;

                NEXUS_Playpump_GetStatus(nexusHandle->playpump, &status);
                LOGV("[%d]Playpump start: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld",
                        iPlayerIndex, status.fifoDepth, status.fifoSize, status.descFifoDepth, status.descFifoSize, status.bytesPlayed);

                if (status.fifoDepth > (status.fifoSize / 2)) {
                    NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, false);
                    forcePause[iPlayerIndex] = false;
                    LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
                }
                else {
                    forcePause[iPlayerIndex] = true;
                }

            }
            else {
                NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, false);
                LOGV("[%d]%s: Starting Playback", iPlayerIndex, __FUNCTION__);
            }
        }
    }

    playpump_status[iPlayerIndex] = PLAY;
    return 0;
}
     
static int bcmPlayer_stop_rawdata(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_stop_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] != STOP) {
        if (nexusHandle->audioPlaypump != NULL) {
            NEXUS_Playpump_Stop(nexusHandle->audioPlaypump);
        }

        if (nexusHandle->playpump != NULL) {
            NEXUS_Playpump_Stop(nexusHandle->playpump);
        }
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
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_pause_rawdata", iPlayerIndex);

    if (playpump_status[iPlayerIndex] == PLAY) {
        if (nexusHandle->simpleStcChannel) {
            NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, true);
        }
    }

    playpump_status[iPlayerIndex] = PAUSE;
    return 0;
}

static int bcmPlayer_get_position_rawdata(int iPlayerIndex, int *msec) 
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    NEXUS_Error              nexusRC;
    NEXUS_VideoDecoderStatus decoderStatus;

    nexusRC = NEXUS_SUCCESS;
  
    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    
    if (nexusHandle->simpleVideoDecoder != NULL) {
        nexusRC = NEXUS_SimpleVideoDecoder_GetStatus(nexusHandle->simpleVideoDecoder, &decoderStatus );
        if (nexusRC != NEXUS_SUCCESS) {
            LOGE("[%d]%s: NEXUS_SimpleVideoDecoder_GetStatus returned %d!", iPlayerIndex, __FUNCTION__, nexusRC);
        }
        else {
            if (decoderStatus.firstPtsPassed && decoderStatus.pts) {
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
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    MUTEX_RAWDATA_LOCK(iPlayerIndex);
    videoFlushing[iPlayerIndex] = true;
    audioFlushing[iPlayerIndex] = true;
    MUTEX_RAWDATA_UNLOCK(iPlayerIndex);

    /* The Data seek is handled in NexusPlayer, this is here to flush the pipeline. */
    if (nexusHandle->playpump) {
        NEXUS_Playpump_SetPause(nexusHandle->playpump, true);
        NEXUS_Playpump_Flush(nexusHandle->playpump);
    }

    if (nexusHandle->audioPlaypump) {
        NEXUS_Playpump_SetPause(nexusHandle->audioPlaypump, true);
        NEXUS_Playpump_Flush(nexusHandle->audioPlaypump);
    }

    if (nexusHandle->simpleVideoDecoder) {
        NEXUS_SimpleVideoDecoder_Flush(nexusHandle->simpleVideoDecoder);
    }

    if (nexusHandle->simpleAudioDecoder) {
        NEXUS_SimpleAudioDecoder_Flush(nexusHandle->simpleAudioDecoder);
    }

    if (nexusHandle->simpleStcChannel) {
        NEXUS_SimpleStcChannel_Invalidate(nexusHandle->simpleStcChannel);
        //if (playpump_status[iPlayerIndex] == PAUSE)
        {
            forcePause[iPlayerIndex] = true;
            NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, true);
            LOGV("[%d]%s: Stc Channel frozen", iPlayerIndex, __FUNCTION__);
        }
    }

    if (nexusHandle->playpump) {
        NEXUS_Playpump_SetPause(nexusHandle->playpump, false);
    }

    if (nexusHandle->audioPlaypump) {
        NEXUS_Playpump_SetPause(nexusHandle->audioPlaypump, false);
    }

    if (nexusHandle->playpump) {
        NEXUS_PlaypumpStatus status;

        NEXUS_Playpump_GetStatus(nexusHandle->playpump, &status);
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
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]%s", iPlayerIndex, __FUNCTION__);
    
    if (nexusHandle->simpleStcChannel) {
        forcePause[iPlayerIndex] = true;
        if (playpump_status[iPlayerIndex] != PAUSE) {
            rc = NEXUS_SimpleStcChannel_Freeze(nexusHandle->simpleStcChannel, true);
            if (rc == NEXUS_SUCCESS) {
                playpump_status[iPlayerIndex] = PAUSE;
                LOGV("[%d]%s: Stc Channel frozen", iPlayerIndex, __FUNCTION__);

                if (nexusHandle->audioPlaypump) {
                    do {
                        NEXUS_PlaypumpStatus audioStatus;

                        NEXUS_Playpump_GetStatus(nexusHandle->audioPlaypump, &audioStatus);

                        LOGV("[%d]%s[AUDIO]: depth=%d, size=%d, desc depth=%d, desc size=%d, played=%lld", iPlayerIndex, __FUNCTION__,
                                audioStatus.fifoDepth, audioStatus.fifoSize, audioStatus.descFifoDepth, audioStatus.descFifoSize, audioStatus.bytesPlayed);

                        if (audioStatus.descFifoDepth > 0) {
                            LOGV("[%d]%s: audio fifo ready", iPlayerIndex, __FUNCTION__);
                            break;
                        }
                        usleep(100000);
                    } while (playpump_status[iPlayerIndex] != STOP);
                }
                else if (nexusHandle->playpump) {
                    do {
                        NEXUS_PlaypumpStatus pumpStatus;

                        NEXUS_Playpump_GetStatus(nexusHandle->playpump, &pumpStatus);

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

    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]%s: fifo %d", iPlayerIndex, __FUNCTION__, fifo);

    if (depth == NULL) {
        LOGE("[%d]%s: depth=NULL!!!", iPlayerIndex, __FUNCTION__);
        return 1;
    }

    MUTEX_RAWDATA_LOCK(iPlayerIndex);

    switch (fifo)
    {
        case FIFO_VIDEO:
            if (nexusHandle->simpleVideoDecoder != NULL) {
                NEXUS_VideoDecoderStatus decoderStatus;
                if (NEXUS_SimpleVideoDecoder_GetStatus(nexusHandle->simpleVideoDecoder, &decoderStatus ) == NEXUS_SUCCESS) {
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
            if (nexusHandle->simpleAudioDecoder != NULL) {
                NEXUS_AudioDecoderStatus decoderStatus;
                if (NEXUS_SimpleAudioDecoder_GetStatus(nexusHandle->simpleAudioDecoder, &decoderStatus ) == NEXUS_SUCCESS) {
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
