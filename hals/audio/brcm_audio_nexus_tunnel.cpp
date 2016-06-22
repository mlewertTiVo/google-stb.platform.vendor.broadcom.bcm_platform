/******************************************************************************
 *    (c)2016 Broadcom Corporation
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

#include "brcm_audio.h"
#include "OMX_Types.h"
#include "bomx_utils.h"
#include <inttypes.h>

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   44100
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

/* Supported stream out sample rate */
const static uint32_t nexus_out_sample_rates[] = {
   8000,
   11025,
   12000,
   16000,
   22050,
   24000,
   32000,
   44100,
   48000,
   64000,
   88200,
   96000,
   128000,
   176400,
   192000
};

#define NEXUS_OUT_SAMPLE_RATE_COUNT                     \
    (sizeof(nexus_out_sample_rates) / sizeof(uint32_t))

#define BRCM_AUDIO_NXCLIENT_NAME "BrcmAudioOutTunnel"

#define BRCM_AUDIO_STREAM_ID (0xC0)

#define BRCM_AUDIO_TUNNEL_DURATION_MS 5
#define BRCM_AUDIO_TUNNEL_HALF_DURATION_US (BRCM_AUDIO_TUNNEL_DURATION_MS * 500)

/*
 * Utility Functions
 */

static void nexus_tunnel_bout_data_callback(void *param1, int param2)
{
    UNUSED(param1);
    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param2);
}

/*
 * Operation Functions
 */

static int nexus_tunnel_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    (void)bout;
    brcm_audio_set_audio_volume(left, right);
    return 0;
}

static int nexus_tunnel_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Get render position failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }
    *dsp_frames = status.numBytesDecoded/bout->frameSize;

    return 0;
}

static int nexus_tunnel_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Get presentation position failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }
    *frames = (uint64_t)(bout->framesPlayed + status.numBytesDecoded/bout->frameSize);

    return 0;
}

static int nexus_tunnel_bout_start(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_SimpleAudioDecoderStartSettings start_settings;

    if (bout->suspended || !audio_decoder) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    ret = NEXUS_Playpump_Start(bout->nexus.tunnel.playpump);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start playpump failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);
    start_settings.primary.codec = NEXUS_AudioCodec_ePcmWav;
    start_settings.primary.pidChannel = bout->nexus.tunnel.pid_channel;
    ret = NEXUS_SimpleAudioDecoder_Start(audio_decoder, &start_settings);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start audio decoder failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }

    return 0;
}

static int nexus_tunnel_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (audio_decoder) {
        NEXUS_AudioDecoderStatus status;
        NEXUS_SimpleAudioDecoder_Stop(audio_decoder);
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Update frame played failed, ret=%d", __FUNCTION__, ret);
        }
        else {
            bout->framesPlayed += status.numBytesDecoded/bout->frameSize;
        }
    }

    if (playpump) {
        NEXUS_Playpump_Stop(playpump);
    }
    ALOGV("%s: setting framesPlayed to %u", __FUNCTION__, bout->framesPlayed);

    return 0;
}

static int nexus_tunnel_bout_pause(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->nexus.tunnel.stc_channel ||
            (bout->nexus.tunnel.stc_channel == (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC))
    {
        return -ENOENT;
    }

    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
    trickState.rate = 0;
    res = NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, res);
       return -ENOMEM;
    }

    res = NEXUS_SimpleStcChannel_Freeze(bout->nexus.tunnel.stc_channel, true);
    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error pausing STC %u", __FUNCTION__, res);

       trickState.rate = NEXUS_NORMAL_DECODE_RATE;
       NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
       return -ENOMEM;
    }

    return 0;
}

static int nexus_tunnel_bout_resume(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->nexus.tunnel.stc_channel ||
            (bout->nexus.tunnel.stc_channel == (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC))
    {
        return -ENOENT;
    }

    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
    trickState.rate = NEXUS_NORMAL_DECODE_RATE;
    res = NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error resuming audio decoder %u", __FUNCTION__, res);
       return -ENOMEM;
    }

    res = NEXUS_SimpleStcChannel_Freeze(bout->nexus.tunnel.stc_channel, false);
    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error resuming STC %u", __FUNCTION__, res);

       trickState.rate = 0;
       NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.tunnel.audio_decoder, &trickState);
       return -ENOMEM;
    }

    return 0;
}

static int nexus_tunnel_bout_drain(struct brcm_stream_out *bout, int action)
{
    (void)bout;
    (void)action;
    // TBD
    return 0;
}

static int nexus_tunnel_bout_flush(struct brcm_stream_out *bout)
{
    int ret = 0;
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    if (audio_decoder) {
       NEXUS_SimpleAudioDecoder_Flush(audio_decoder);
    } else {
       ret = -ENOENT;
    }
    return ret;
}

const static uint8_t av_sync_marker[] = {0x55, 0x55, 0x00, 0x01};
#define HW_AV_SYNC_HDR_LEN 16
static int nexus_tunnel_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    size_t bytes_written = 0;
    int ret = 0;
    void *nexus_buffer, *pts_buffer;
    uint32_t pts=0;
    size_t nexus_space;
    BKNI_EventHandle event = bout->nexus.event;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (bout->suspended || !audio_decoder || !playpump) {
        ALOGE("%s: not ready to output audio samples", __FUNCTION__);
        return -ENOSYS;
    }

    while ( bytes > 0 )
    {
        bool writeHeader = false;
        size_t frameBytes;

        // Search for next packet header
        pts_buffer = memmem(buffer, bytes, av_sync_marker, sizeof(av_sync_marker));
        if ( NULL != pts_buffer )
        {
            if ( buffer == pts_buffer )
            {
                // frame header is at payload start, write the header
                writeHeader = true;
                frameBytes = B_MEDIA_LOAD_UINT32_BE(pts_buffer, sizeof(uint32_t));
                uint64_t timestamp = B_MEDIA_LOAD_UINT32_BE(pts_buffer, 2*sizeof(uint32_t));
                timestamp <<= 32;
                timestamp |= B_MEDIA_LOAD_UINT32_BE(pts_buffer, 3*sizeof(uint32_t));
                timestamp /= 1000; // Convert ns -> us
                pts = BOMX_TickToPts((OMX_TICKS *)&timestamp);
                ALOGV("%s: av-sync header, ts=%" PRIu64 " pts=%" PRIu32 ", size=%zu, payload=%zu", __FUNCTION__, timestamp, pts, frameBytes, bytes);
                bytes -= HW_AV_SYNC_HDR_LEN;
                buffer = (void *)((uint8_t *)buffer + HW_AV_SYNC_HDR_LEN);
            }
            else
            {
                // frame header is later in the payload, write as many bytes as needed to get there
                frameBytes = (uint8_t *)pts_buffer - (uint8_t *)buffer;
                ALOGV("%s: later av-sync header, %zu bytes to write", __FUNCTION__, frameBytes);
            }
        }
        else
        {
            frameBytes = bytes;
            ALOGV("%s: no av-sync header, %zu bytes to write", __FUNCTION__, frameBytes);
        }

        while ( frameBytes > 0 )
        {
            bmedia_pes_info pes;
            size_t used, waveformat_size;
            size_t spaceRequired;

            ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
            if (ret) {
                ALOGE("%s: get playpump buffer failed, ret=%d", __FUNCTION__, ret);
                break;
            }

            spaceRequired = (frameBytes >= bytes) ? frameBytes : bytes;
            if ( writeHeader ) {
                spaceRequired += BMEDIA_PES_HEADER_MAX_SIZE + BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
            }
            if (nexus_space >= spaceRequired) {
                used = 0;
                if ( writeHeader ) {
                    // Format PES
                    BMEDIA_PES_INFO_INIT(&pes, BRCM_AUDIO_STREAM_ID);
                    BMEDIA_PES_SET_PTS(&pes, pts);
                    used += bmedia_pes_header_init((uint8_t *)nexus_buffer, frameBytes + BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4, &pes);
                    BKNI_Memcpy((uint8_t *)nexus_buffer + used, bmedia_frame_bcma.base, bmedia_frame_bcma.len);
                    used += bmedia_frame_bcma.len;
                    B_MEDIA_SAVE_UINT32_BE((uint8_t *)nexus_buffer + used, frameBytes);
                    used += sizeof(uint32_t);

                    // Format WAV header
                    waveformat_size = bmedia_write_waveformatex((uint8_t *)nexus_buffer + used, &bout->nexus.tunnel.wave_fmt);
                    ALOG_ASSERT(waveformat_size == BMEDIA_WAVEFORMATEX_BASE_SIZE);
                    used += waveformat_size;

                    // Header may indicate more data in frame than was passed in write()
                    if ( frameBytes > bytes ) {
                        frameBytes = bytes;
                    }
                }

                BKNI_Memcpy((uint8_t *)nexus_buffer + used, buffer, frameBytes);
                used += frameBytes;
                BDBG_ASSERT(used <= nexus_space);
                ret = NEXUS_Playpump_WriteComplete(playpump, 0, used);
                if (ret) {
                    ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                    break;
                }
                else {
                    bytes_written += frameBytes;
                    if ( writeHeader ) {
                        bytes_written += HW_AV_SYNC_HDR_LEN;
                        writeHeader = false;
                    }
                    buffer = (void *)((uint8_t *)buffer + frameBytes);
                    bytes -= frameBytes;
                    frameBytes = 0;
                }
            }
            else if ((uint8_t *)nexus_buffer + nexus_space >= bout->nexus.tunnel.pp_buffer_end) {
                // Look for a complete contiguous buffer
                ret = NEXUS_Playpump_WriteComplete(playpump, nexus_space, 0);
                if (ret) {
                    ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                    break;
                }
            }
            else {
                NEXUS_SimpleAudioDecoderHandle prev_audio_decoder = audio_decoder;
                NEXUS_PlaypumpHandle prev_playpump = playpump;

                pthread_mutex_unlock(&bout->lock);
                ret = BKNI_WaitForEvent(event, 500);
                pthread_mutex_lock(&bout->lock);

                // Sanity check when relocking
                audio_decoder = bout->nexus.tunnel.audio_decoder;
                playpump = bout->nexus.tunnel.playpump;
                ALOG_ASSERT(audio_decoder == prev_audio_decoder);
                ALOG_ASSERT(playpump == prev_playpump);
                ALOG_ASSERT(!bout->suspended);

                if (ret) {
                    ALOGE("%s: playpump write timeout, ret=%d", __FUNCTION__, ret);
                    break;
                }
            }
        }
    }

    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    // Throttle the output to prevent audio underruns
    nsecs_t delta = systemTime(SYSTEM_TIME_MONOTONIC) - bout->nexus.tunnel.last_write_time;
    int32_t throttle_us = BRCM_AUDIO_TUNNEL_HALF_DURATION_US - (delta / 1000);
    if (throttle_us <= BRCM_AUDIO_TUNNEL_HALF_DURATION_US && throttle_us > 0) {
        ALOGV("%s: throttle %d us", __FUNCTION__, throttle_us);
        usleep(throttle_us);
    }
    bout->nexus.tunnel.last_write_time = systemTime(SYSTEM_TIME_MONOTONIC);

    return bytes_written;
}

// returns true if nexus audio can enter standby
static bool nexus_tunnel_bout_standby_monitor(void *context)
{
    bool standby = true;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;

    if (bout != NULL) {
        pthread_mutex_lock(&bout->lock);
        standby = (bout->started == false);
        pthread_mutex_unlock(&bout->lock);
    }
    ALOGV("%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

static void nexus_tunnel_bout_playpump_callback(void *context, int param)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;
    ALOG_ASSERT(bout->nexus.event == (BKNI_EventHandle)(intptr_t)param);
    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param);
}

static int nexus_tunnel_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_Error rc;
    android::status_t status;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
    BKNI_EventHandle event;
    uint32_t audioResourceId;
    int i, ret = 0;
    NEXUS_PlaypumpStatus playpumpStatus;
    bmedia_waveformatex_header *wave_fmt = &bout->nexus.tunnel.wave_fmt;

    /* Check if sample rate is supported */
    for (i = 0; i < (int)NEXUS_OUT_SAMPLE_RATE_COUNT; i++) {
        if (config->sample_rate == nexus_out_sample_rates[i]) {
            break;
        }
    }
    if (i == NEXUS_OUT_SAMPLE_RATE_COUNT) {
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    }

    /* Only allow default config for others */
    config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    config->format = NEXUS_OUT_DEFAULT_FORMAT;

    bout->framesPlayed = 0;
    bout->frameSize = audio_bytes_per_sample(config->format) * popcount(config->channel_mask);
    bout->buffer_size = get_brcm_audio_buffer_size(config->sample_rate,
                                   config->format,
                                   popcount(config->channel_mask),
                                   BRCM_AUDIO_TUNNEL_DURATION_MS);

    ALOGV("%s: sample_rate=%" PRIu32 " frameSize=%" PRIu32 " buffer_size=%zu",
            __FUNCTION__, config->sample_rate, bout->frameSize, bout->buffer_size);

    /* Open Nexus simple playback */
    rc = brcm_audio_client_join(BRCM_AUDIO_NXCLIENT_NAME);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: brcm_audio_client_join error, rc:%d", __FUNCTION__, rc);
        return -ENOSYS;
    }

    /* Allocate simpleAudioPlayback */
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleAudioDecoder = 1;
    rc = NxClient_Alloc(&allocSettings, &(bout->nexus.allocResults));
    if (rc) {
        ALOGE("%s: Cannot allocate NxClient resources, rc:%d", __FUNCTION__, rc);
        ret = -ENOMEM;
        goto err_alloc;
    }

    audioResourceId = bout->nexus.allocResults.simpleAudioDecoder.id;
    bout->nexus.tunnel.audio_decoder = NEXUS_SimpleAudioDecoder_Acquire(audioResourceId);
    if (!bout->nexus.simple_decoder) {
        ALOGE("%s: at %d, acquire Nexus simple audio decoder handle failed\n",
             __FUNCTION__, __LINE__);
        ret = -ENOMEM;
        goto err_acquire;
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleAudioDecoder.id = audioResourceId;
    rc = NxClient_Connect(&connectSettings, &(bout->nexus.connectId));
    if (rc) {
        ALOGE("%s: error calling NxClient_Connect, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_acquire;
    }

    ret = BKNI_CreateEvent(&event);
    if (ret) {
        ALOGE("%s: at %d, create event failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -ENOMEM;
        goto err_event;
    }

    // register standby callback
    bout->standbyCallback = bout->bdev->standbyThread->RegisterCallback(nexus_tunnel_bout_standby_monitor, bout);
    if (bout->standbyCallback < 0) {
        ALOGE("Error registering standby callback");
        ret = -ENOSYS;
        goto err_callback;
    }

    bout->nexus.tunnel.playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, NULL);
    if (!bout->nexus.tunnel.playpump) {
        ALOGE("%s: Error opening playpump", __FUNCTION__);
        ret = -ENOMEM;
        goto err_playpump;
    }

    NEXUS_Playpump_GetSettings(bout->nexus.tunnel.playpump, &playpumpSettings);
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.dataCallback.callback = nexus_tunnel_bout_playpump_callback;
    playpumpSettings.dataCallback.context = bout;
    playpumpSettings.dataCallback.param = (int)(intptr_t)event;
    rc = NEXUS_Playpump_SetSettings(bout->nexus.tunnel.playpump, &playpumpSettings);
    if (rc) {
        ALOGE("%s: Error setting playpump settings, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_playpump_set;
    }

    rc = NEXUS_Playpump_GetStatus(bout->nexus.tunnel.playpump, &playpumpStatus);
    if (rc) {
        ALOGE("%s: Error getting playpump status, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_playpump_set;
    }
    bout->nexus.tunnel.pp_buffer_end = (uint8_t *)playpumpStatus.bufferBase + playpumpStatus.fifoDepth;

    if (bout->nexus.tunnel.stc_channel_owner == BRCM_OWNER_OUTPUT) {
        if (property_get_int32(BRCM_PROPERTY_AUDIO_OUTPUT_HW_SYNC_FAKE, 0)) {
           bout->nexus.tunnel.stc_channel = (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC;
        } else {
           NEXUS_SimpleStcChannelSettings stcChannelSettings;
           NEXUS_SimpleStcChannel_GetDefaultSettings(&stcChannelSettings);
           stcChannelSettings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eAudioMaster;
           bout->nexus.tunnel.stc_channel = NEXUS_SimpleStcChannel_Create(&stcChannelSettings);
           if (!bout->nexus.tunnel.stc_channel) {
              ALOGE("%s: Error creating STC channel", __FUNCTION__);
              ret = -ENOMEM;
              goto err_playpump_set;
           }
           NEXUS_Platform_SetSharedHandle(bout->nexus.tunnel.stc_channel, true);
        }
    }

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
    pidSettings.pidType = NEXUS_PidType_eAudio;
    bout->nexus.tunnel.pid_channel = NEXUS_Playpump_OpenPidChannel(bout->nexus.tunnel.playpump, BRCM_AUDIO_STREAM_ID, &pidSettings);
    if (!bout->nexus.tunnel.pid_channel) {
        ALOGE("%s: Error openning pid channel", __FUNCTION__);
        ret = -ENOMEM;
        goto err_pid;
    }

    if (!property_get_int32(BRCM_PROPERTY_AUDIO_OUTPUT_HW_SYNC_FAKE, 0)) {
       ALOG_ASSERT(bout->nexus.tunnel.stc_channel != NULL);
       NEXUS_SimpleAudioDecoder_SetStcChannel(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.stc_channel);
    }

    // Initialize WAV format parameters
    bmedia_init_waveformatex(wave_fmt);
    wave_fmt->wFormatTag = 0x0001;  // PCM
    wave_fmt->nChannels = popcount(bout->config.channel_mask);
    wave_fmt->nSamplesPerSec = bout->config.sample_rate;
    wave_fmt->wBitsPerSample = audio_bytes_per_sample(bout->config.format) * 8;
    wave_fmt->nBlockAlign = (wave_fmt->nChannels * wave_fmt->wBitsPerSample) / 8;
    wave_fmt->cbSize = 0;
    wave_fmt->nAvgBytesPerSec = (wave_fmt->wBitsPerSample * wave_fmt->nSamplesPerSec * wave_fmt->nChannels) / 8;
    ALOGV("%s: wave_fmt channel=%d sample_rate=%" PRId32 " bit_per_sample=%d align=%d avg_bytes_rate=%" PRId32 "",
            __FUNCTION__, wave_fmt->nChannels, wave_fmt->nSamplesPerSec, wave_fmt->wBitsPerSample,
            wave_fmt->nBlockAlign, wave_fmt->nAvgBytesPerSec);

    bout->nexus.tunnel.last_write_time = 0;

    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

    return 0;

err_pid:
    if (bout->nexus.tunnel.stc_channel_owner == BRCM_OWNER_OUTPUT) {
       if (bout->nexus.tunnel.stc_channel != (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC) {
          NEXUS_Platform_SetSharedHandle(bout->nexus.tunnel.stc_channel, false);
          NEXUS_SimpleStcChannel_Destroy(bout->nexus.tunnel.stc_channel);
       }
       bout->nexus.tunnel.stc_channel = NULL;
    }
err_playpump_set:
    NEXUS_Playpump_Close(bout->nexus.tunnel.playpump);
    bout->nexus.tunnel.playpump = NULL;
err_playpump:
    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);
err_callback:
    BKNI_DestroyEvent(event);
err_event:
    NxClient_Disconnect(bout->nexus.connectId);
err_acquire:
    NxClient_Free(&(bout->nexus.allocResults));
err_alloc:
    NxClient_Uninit();

    return ret;
}

static int nexus_tunnel_bout_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

    if (bout->nexus.state == BRCM_NEXUS_STATE_DESTROYED) {
       return 0;
    }

    nexus_tunnel_bout_stop(bout);

    if (bout->nexus.tunnel.pid_channel) {
        ALOG_ASSERT(bout->nexus.tunnel.playpump != NULL);
        NEXUS_Playpump_ClosePidChannel(bout->nexus.tunnel.playpump, bout->nexus.tunnel.pid_channel);
        bout->nexus.tunnel.pid_channel = NULL;
    }

    if ((bout->nexus.tunnel.stc_channel_owner == BRCM_OWNER_OUTPUT) &&
        (bout->nexus.tunnel.stc_channel != NULL)) {
       if (bout->nexus.tunnel.stc_channel != (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC) {
          NEXUS_SimpleStcChannel_Destroy(bout->nexus.tunnel.stc_channel);
       }
       bout->nexus.tunnel.stc_channel = NULL;
    }

    if (bout->nexus.tunnel.playpump) {
        NEXUS_Playpump_Close(bout->nexus.tunnel.playpump);
        bout->nexus.tunnel.playpump = NULL;
    }

    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    if (bout->nexus.tunnel.audio_decoder) {
        NEXUS_SimpleAudioDecoder_Release(bout->nexus.tunnel.audio_decoder);
    }

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));
    NxClient_Uninit();
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    return 0;
}

static char *nexus_tunnel_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    char* result_str;

    pthread_mutex_lock(&bout->lock);
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_HW_AV_SYNC)) {
        str_parms_add_int(result, AUDIO_PARAMETER_STREAM_HW_AV_SYNC,
                          (int)(intptr_t)bout->nexus.tunnel.stc_channel);
    }
    pthread_mutex_unlock(&bout->lock);

    result_str = str_parms_to_str(result);
    str_parms_destroy(query);
    str_parms_destroy(result);

    ALOGV("%s: result = %s", __FUNCTION__, result_str);

    return result_str;
}

static int nexus_tunnel_bout_dump(struct brcm_stream_out *bout, int fd)
{
   dprintf(fd, "\nnexus_tunnel_bout_dump:\n"
            "\tstc-channel = %p\n"
            "\tpid-channel = %p\n"
            "\tplaypump    = %p\n"
            "\tdecoder     = %p\n",
            (void *)bout->nexus.tunnel.stc_channel,
            (void *)bout->nexus.tunnel.pid_channel,
            (void *)bout->nexus.tunnel.playpump,
            (void *)bout->nexus.tunnel.audio_decoder);

   return 0;
}

struct brcm_stream_out_ops nexus_tunnel_bout_ops = {
    .do_bout_open = nexus_tunnel_bout_open,
    .do_bout_close = nexus_tunnel_bout_close,
    .do_bout_start = nexus_tunnel_bout_start,
    .do_bout_stop = nexus_tunnel_bout_stop,
    .do_bout_write = nexus_tunnel_bout_write,
    .do_bout_set_volume = nexus_tunnel_bout_set_volume,
    .do_bout_get_render_position = nexus_tunnel_bout_get_render_position,
    .do_bout_get_presentation_position = nexus_tunnel_bout_get_presentation_position,
    .do_bout_dump = nexus_tunnel_bout_dump,
    .do_bout_get_parameters = nexus_tunnel_bout_get_parameters,
    .do_bout_pause = nexus_tunnel_bout_pause,
    .do_bout_resume = nexus_tunnel_bout_resume,
    .do_bout_drain = nexus_tunnel_bout_drain,
    .do_bout_flush = nexus_tunnel_bout_flush,
};
