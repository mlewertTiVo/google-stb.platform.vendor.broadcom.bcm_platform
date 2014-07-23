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
 * $brcm_Workfile: bcmPlayer.cpp $
 * 
 *****************************************************************************/
// Verbose messages removed
// #define LOG_NDEBUG 0

#define LOG_TAG "bcmPlayer"

#include "bcmPlayer.h"
#include "bcmPlayer_base.h"
#include "bcmPlayer_nexus.h"

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

typedef enum {
    ePlayerEndStream = 0,
    ePlayerError,
    ePlayerHeight,
    ePlayerWidth,
    ePlayerBufferingPauseStart,
    ePlayerBufferingPauseEnd
} CALLBACK_EVENTS;

static onEventCbPtr g_evtCb[MAX_NEXUS_PLAYER];

static bcmPlayer_func_t *pstCurrModule;

/* register function for implemented module */
static const bcmPlayer_module_t bcmPlayer_reg_methods[INPUT_SOURCE_MAX] =
{
    player_reg_local,
    player_reg_ip,
    player_reg_live,
    player_reg_hdmiIn,
    player_reg_analogIn,
    player_reg_rawdata,
};

static const bcmPlayer_func_t bcmPlayer_func_default = 
{
    bcmPlayer_init_base,
    bcmPlayer_uninit_base,
    bcmPlayer_setDataSource_base,
    bcmPlayer_setDataSource_file_handle_base,
    bcmPlayer_setVideoSurface_base,
    bcmPlayer_setVideoSurfaceTexture_base,
    bcmPlayer_setAudioSink_base,
    bcmPlayer_prepare_base,
    bcmPlayer_start_base,
    bcmPlayer_stop_base,
    bcmPlayer_pause_base,
    bcmPlayer_isPlaying_base,
    bcmPlayer_seekTo_base,
    bcmPlayer_getCurrentPosition_base,
    bcmPlayer_getDuration_base,
    bcmPlayer_getFifoDepth_base,
    bcmPlayer_reset_base,
    bcmPlayer_setLooping_base,
    bcmPlayer_suspend_base,
    bcmPlayer_resume_base,
    bcmPlayer_setAspectRatio_base,
    bcmPlayer_config_live,
    bcmPlayer_putData_base,
    bcmPlayer_putAudioData_base,
    bcmPlayer_getMediaExtractorFlags_base,
    bcmPlayer_selectTrack_base
};


int bcmPlayer_init(int iPlayerIndex, onEventCbPtr evtCb)
{
    NEXUS_PlatformSettings platformSettings;
    NEXUS_VideoFormat      maxVideoFormat;

    LOGD("[%d]bcmPlayer_init", iPlayerIndex);

    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    if (iPlayerIndex >= MAX_NEXUS_PLAYER) {
        LOGE("%s: Cannot instantiate more than %d bcmPlayer resources!", __FUNCTION__, MAX_NEXUS_PLAYER);
        return 1;
    }
    pstCurrModule = new bcmPlayer_func_t;

    /* giving default implementation */
    memcpy(pstCurrModule, &bcmPlayer_func_default, sizeof(bcmPlayer_func_t));

    /* callback function register */
    g_evtCb[iPlayerIndex] = evtCb;

    /* setup nexus platform.  After this, nexus_handle[] is valid for use. */
    bcmPlayer_platformJoin(iPlayerIndex);

    nexusHandle->iPlayerIndex = iPlayerIndex;

    NEXUS_Platform_GetSettings(&platformSettings);
    if ((iPlayerIndex == 0) && 
        (platformSettings.videoDecoderModuleSettings.supportedCodecs[NEXUS_VideoCodec_eH265])) {
        nexusHandle->bSupportsHEVC = true;
    }
    else {
        nexusHandle->bSupportsHEVC = false;
    }

    maxVideoFormat = platformSettings.videoDecoderModuleSettings.maxDecodeFormat;
    nexusHandle->maxVideoFormat = maxVideoFormat;
    LOGI("maxVideoFormat for %d = %d", iPlayerIndex, maxVideoFormat);

    nexusHandle->state = eBcmPlayerStateInitialising;
    return 0;
}

void bcmPlayer_uninit(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_uninit",iPlayerIndex);

    pstCurrModule->bcmPlayer_uninit_func(iPlayerIndex);

    if (nexusHandle->url) {
        free(nexusHandle->url);
    }

    if (nexusHandle->extraHeaders) {
        free(nexusHandle->extraHeaders);
    }

    /* undo ipc stuffs */ 
    bcmPlayer_platformUnjoin(iPlayerIndex);

    delete pstCurrModule;

    nexusHandle->state = eBcmPlayerStateEnd;
}

static int priv_identifySource(const char *url)
{
    /* Decide which module to use */
    if (strncmp("rtp://", url, 6) == 0)
        return INPUT_SOURCE_IP;
    else if (strncmp("udp://", url, 6) == 0)
        return INPUT_SOURCE_IP;
    else if (strncmp("rtsp://", url, 7) == 0)
        return INPUT_SOURCE_IP;
    else if (strncmp(url, "http://", 8) >= 0)
        return INPUT_SOURCE_IP;
    else if (strncmp(url,"brcm:live",9) == 0)
        return INPUT_SOURCE_LIVE;
    else if (strncmp(url,"brcm:hdmi_in",12) == 0)
        return INPUT_SOURCE_HDMI_IN;
    else if (strncmp(url,"brcm:analog_in",14) == 0)
        return INPUT_SOURCE_ANALOG_IN;
    else if (strncmp(url,"brcm:raw_data",13) == 0)
        return INPUT_SOURCE_RAW_DATA;
    else
        return INPUT_SOURCE_LOCAL;
}

int bcmPlayer_setDataSource(
    int iPlayerIndex,
    const char *url,
    uint16_t *videoWidth,
    uint16_t *videoHeight,
    char* extraHeader)
{
    int rc = 0;
    int input_source = INPUT_SOURCE_LOCAL;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    if (NULL == url) {
        LOGW("Calling with null url, abort...");
        return 1;
    }

    LOGD("[%d]bcmPlayer_setDataSource:  %s", iPlayerIndex, url);

    // Stash the URL 
    nexusHandle->url = strdup(url);
    if (nexusHandle->url == NULL) {
        LOGE("%s: Couldn't save URL", __FUNCTION__);
        return 1;
    }

    // Stash the extra header information
    if (extraHeader != NULL) {
        nexusHandle->extraHeaders = strdup(extraHeader);
        if (nexusHandle->extraHeaders == NULL) {
            LOGE("%s: Couldn't save extraHeader", __FUNCTION__);
            return 1;
        }
    }

    input_source = priv_identifySource(url);

    LOGD("=============================================");
    LOGD("Identified data source type %d", input_source);
    LOGD("=============================================");

    /* re-direct methods from module if implemented*/
    bcmPlayer_reg_methods[input_source](pstCurrModule);

    /* Init after knowing data source */
    rc = pstCurrModule->bcmPlayer_init_func(iPlayerIndex);

    if (rc == 0) {
        rc = pstCurrModule->bcmPlayer_setDataSource_func(iPlayerIndex, url, videoWidth, videoHeight, extraHeader);
        if (rc == 0) {
            nexusHandle->state = eBcmPlayerStateInitialised;
        }
    }
    return rc;
}

int bcmPlayer_setDataSource_file_handle(int iPlayerIndex, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight)
{
    int rc;

    LOGD("[%d]bcmPlayer_setDataSource_file_handle(%d, %lld, %lld)", iPlayerIndex, fd, offset, length);

    /* The file descriptor fd must come from local file system */
    bcmPlayer_reg_methods[INPUT_SOURCE_LOCAL](pstCurrModule);

    /* Init after knowing data source */
    pstCurrModule->bcmPlayer_init_func(iPlayerIndex);

    rc = pstCurrModule->bcmPlayer_setDataSource_file_handle_func(iPlayerIndex, fd, offset, length, videoWidth, videoHeight);
    if (rc == 0) {
        nexus_handle[iPlayerIndex].state = eBcmPlayerStateInitialised;
    }
    return rc;
}

int bcmPlayer_setVideoSurface(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_setVideoSurface", iPlayerIndex);

    return pstCurrModule->bcmPlayer_setVideoSurface_func(iPlayerIndex);
}

int bcmPlayer_setVideoSurfaceTexture(int iPlayerIndex, buffer_handle_t bufferHandle, bool visible)
{
    LOGD("[%d]bcmPlayer_setVideoSurfaceTexture buffer handle=%p, visible=%d", iPlayerIndex, (void *)bufferHandle, visible);

    return pstCurrModule->bcmPlayer_setVideoSurfaceTexture_func(iPlayerIndex, bufferHandle, visible);
}

int bcmPlayer_setAudioSink(int iPlayerIndex, bool connect)
{
    LOGD("[%d]bcmPlayer_setAudioSink %sconnect", iPlayerIndex, (connect==true) ? "" : "dis");

    return pstCurrModule->bcmPlayer_setAudioSink_func(iPlayerIndex, connect);
}

int bcmPlayer_prepare(int iPlayerIndex)
{
    int rc = 0;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_prepare", iPlayerIndex);

    if (nexusHandle->state != eBcmPlayerStatePrepared) {
        rc = pstCurrModule->bcmPlayer_prepare_func(iPlayerIndex);
        if (rc == 0) {
            nexusHandle->state = eBcmPlayerStatePrepared;
        }
    }
    return rc;
}

int bcmPlayer_start(int iPlayerIndex)
{
    int rc;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGD("[%d]bcmPlayer_start", iPlayerIndex);

    rc = pstCurrModule->bcmPlayer_start_func(iPlayerIndex);
    if (rc == 0) {
        // Did a seekTo occur during the prepared state?  If so, then perform the actual seek at start
        if (nexusHandle->state == eBcmPlayerStatePrepared && nexusHandle->seekPositionMs != 0) { 
            nexusHandle->state = eBcmPlayerStateStarted;    // Must set start state here before we perform the seekTo
            rc = pstCurrModule->bcmPlayer_seekTo_func(iPlayerIndex, nexusHandle->seekPositionMs);
        } else {
            nexusHandle->state = eBcmPlayerStateStarted;
        }
    }
    return rc;
}

int bcmPlayer_stop(int iPlayerIndex) 
{
    int rc;

    LOGD("[%d]bcmPlayer_stop", iPlayerIndex);

    rc = pstCurrModule->bcmPlayer_stop_func(iPlayerIndex);
    if (rc == 0) {
        nexus_handle[iPlayerIndex].state = eBcmPlayerStateStopped;
    }
    return rc;
}

int bcmPlayer_pause(int iPlayerIndex)
{
    int rc;

    LOGD("[%d]bcmPlayer_pause", iPlayerIndex);

    rc = pstCurrModule->bcmPlayer_pause_func(iPlayerIndex);

    if (rc == 0) {
        nexus_handle[iPlayerIndex].state = eBcmPlayerStatePaused;
    }
    return rc;
}

int bcmPlayer_isPlaying(int iPlayerIndex)
{
    LOGV("[%d]bcmPlayer_isPlaying", iPlayerIndex);

    return pstCurrModule->bcmPlayer_isPlaying_func(iPlayerIndex);
}

int bcmPlayer_seekTo(int iPlayerIndex, int msec)
{
    LOGD("[%d]bcmPlayer_seekTo,  msec:  %d", iPlayerIndex, msec);

    return pstCurrModule->bcmPlayer_seekTo_func(iPlayerIndex, msec);
}

int bcmPlayer_getCurrentPosition(int iPlayerIndex, int *msec)
{
    LOGV("[%d]bcmPlayer_getCurrentPosition", iPlayerIndex);

    return pstCurrModule->bcmPlayer_getCurrentPosition_func(iPlayerIndex, msec);
}

int bcmPlayer_getDuration(int iPlayerIndex, int *msec)
{
    LOGV("[%d]bcmPlayer_getDuration", iPlayerIndex);

    return pstCurrModule->bcmPlayer_getDuration_func(iPlayerIndex, msec);
}

int bcmPlayer_getFifoDepth(int iPlayerIndex, bcmPlayer_fifo_t fifo, int *depth)
{
    LOGV("[%d]bcmPlayer_getFifoDepth[fifo %d]", iPlayerIndex, fifo);

    return pstCurrModule->bcmPlayer_getFifoDepth_func(iPlayerIndex, fifo, depth);
}

int bcmPlayer_reset(int iPlayerIndex)
{
    int rc;

    LOGD("[%d]bcmPlayer_reset", iPlayerIndex);

    rc = pstCurrModule->bcmPlayer_reset_func(iPlayerIndex);
    if (rc == 0) {
        nexus_handle[iPlayerIndex].state = eBcmPlayerStateIdle;
    }
    return rc;
}

int bcmPlayer_setLooping(int iPlayerIndex, int loop)
{
    LOGD("[%d]bcmPlayer_setLooping, loop = %d\n", iPlayerIndex, loop);

    nexus_handle[iPlayerIndex].bKeepLooping = (loop != 0);
    return pstCurrModule->bcmPlayer_setLooping_func(iPlayerIndex, loop);
}

int bcmPlayer_suspend(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_suspend", iPlayerIndex);

    return pstCurrModule->bcmPlayer_suspend_func(iPlayerIndex);
}

int bcmPlayer_resume(int iPlayerIndex)
{
    LOGD("[%d]bcmPlayer_resume", iPlayerIndex);

    return pstCurrModule->bcmPlayer_resume_func(iPlayerIndex);
}

int bcmPlayer_setAspectRatio(int iPlayerIndex, int ar) 
{
    LOGD("[%d]bcmPlayer_setAspectRatio", iPlayerIndex);
    return pstCurrModule->bcmPlayer_setAspectRatio_func(iPlayerIndex, ar);
}

void bcmPlayer_config(int iPlayerIndex, const char* flag,const char* value)
{
    LOGD("bcmPlayer_config");
    
    return pstCurrModule->bcmPlayer_config_func(iPlayerIndex, flag, value); 
}

int bcmPlayer_putData(int iPlayerIndex, const void *data, size_t data_len)
{
    LOGV("[%d]bcmPlayer_putData", iPlayerIndex);
    return pstCurrModule->bcmPlayer_putData_func(iPlayerIndex, data, data_len);
}

int bcmPlayer_putAudioData(int iPlayerIndex, const void *data, size_t data_len)
{
    LOGV("[%d]bcmPlayer_putAudioData", iPlayerIndex);
    return pstCurrModule->bcmPlayer_putAudioData_func(iPlayerIndex, data, data_len);
}

int bcmPlayer_getMediaExtractorFlags(int iPlayerIndex, unsigned *flags)
{
    LOGD("[%d]bcmPlayer_getMediaExtractorFlags", iPlayerIndex);
    return pstCurrModule->bcmPlayer_getMediaExtractorFlags_func(iPlayerIndex, flags);
}

int bcmPlayer_selectTrack(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select)
{
    LOGD("[%d]bcmPlayer_selectTrack(%d, %d, %d)", iPlayerIndex, trackType, trackIndex, select);
    return pstCurrModule->bcmPlayer_selectTrack_func(iPlayerIndex, trackType, trackIndex, select);
}

/********** end of library API ********/

void bcmPlayer_endOfStreamCallback(void *context, int param)
{
    LOGD("%s param=%d", __FUNCTION__, param);
    if (0 == nexus_handle[param].bKeepLooping) {
         bcmPlayer_pause(param);
        (*g_evtCb[param])(param, ePlayerEndStream, 0);
    }
    else {
        LOGD("Keep looping, suspend the bcmPlayer_endOfStreamCallback");
    }
}

void bcmPlayer_errorCallback(void *context, int param)
{
    LOGD("%s param=%d", __FUNCTION__, param);
    (*g_evtCb[param])(param, ePlayerError, 0);
    nexus_handle[param].state = eBcmPlayerStateError;
}

void bcmPlayer_widthHeightCallback(int param, int width, int height)
{
    LOGD("%s param=%d %dx%d", __FUNCTION__, param, height, width);
    (*g_evtCb[param])(param, ePlayerHeight, height);
    (*g_evtCb[param])(param, ePlayerWidth, width);
}

void bcmPlayer_bufferingPauseStartCallback (int param) 
{
    LOGD("%s param=%d", __FUNCTION__, param);
    (*g_evtCb[param])(param, ePlayerBufferingPauseStart, 0);
}

void bcmPlayer_bufferingPauseEndCallback (int param) 
{
    LOGD("%s param=%d", __FUNCTION__, param);
    (*g_evtCb[param])(param, ePlayerBufferingPauseEnd, 0);
}

