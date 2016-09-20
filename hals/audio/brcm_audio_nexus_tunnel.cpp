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
#include "brcm_audio_nexus_hdmi.h"
#include "OMX_Types.h"
#include "bomx_utils.h"
#include <inttypes.h>

#define STRINGIFY(x)                    #x
#define TOSTRING(x)                     STRINGIFY(x)

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

#define BRCM_AUDIO_TUNNEL_PROPERTY_PES_DEBUG    "media.brcm.aout_t_pes_debug"

#define BRCM_AUDIO_NXCLIENT_NAME                "BrcmAudioOutTunnel"

#define BRCM_AUDIO_STREAM_ID                    (0xC0)

#define BRCM_AUDIO_TUNNEL_DURATION_MS           (5)
#define BRCM_AUDIO_TUNNEL_HALF_DURATION_US      (BRCM_AUDIO_TUNNEL_DURATION_MS * 500)
#define BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS  (300)

#define BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE      (2048)
#define BRCM_AUDIO_TUNNEL_COMP_FRAME_QUEUED     (100)
#define BRCM_AUDIO_TUNNEL_COMP_DRAIN_DELAY_US   (50000)
/* Drain delay x retries = 500ms */
#define BRCM_AUDIO_TUNNEL_COMP_DRAIN_DELAY_MAX  (10)

/*
 * Function declarations
 */
static int nexus_tunnel_bout_pause(struct brcm_stream_out *bout);
static int nexus_tunnel_bout_resume(struct brcm_stream_out *bout);

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

    if (bout->frameSize == 0) {
        *dsp_frames = 0;
        return 0;
    }

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Get render position failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }

    if (!status.started || status.numBytesDecoded == 0) {
      *dsp_frames = 0;
      return 0;
    }

    *dsp_frames = (uint32_t)(status.numBytesDecoded/bout->frameSize);
    return 0;
}

static int nexus_tunnel_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_Error ret;

    if (bout->frameSize == 0) {
        *frames = (uint64_t)(bout->framesPlayed);
        return 0;
    }

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Get presentation position failed, ret = %d", __FUNCTION__, ret);
        return -ENOSYS;
    }

    if (!status.started || status.numBytesDecoded == 0) {
      *frames = (uint64_t)bout->framesPlayed;
      return 0;
    }

    *frames = (uint64_t)(bout->framesPlayed + status.numBytesDecoded/bout->frameSize);
    return 0;
}

static void nexus_tunnel_bout_debounce_reset(struct brcm_stream_out *bout)
{
    bout->nexus.tunnel.debounce = false;
    bout->nexus.tunnel.debounce_pausing = false;
    bout->nexus.tunnel.debounce_more = false;
    bout->nexus.tunnel.debounce_expired = false;
    bout->nexus.tunnel.debounce_stopping = false;
    bout->nexus.tunnel.last_pause_time = 0;
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
    start_settings.primary.codec = brcm_audio_get_codec_from_format(bout->config.format);
    start_settings.primary.pidChannel = bout->nexus.tunnel.pid_channel;
    ret = NEXUS_SimpleAudioDecoder_Start(audio_decoder, &start_settings);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start audio decoder failed, ret = %d", __FUNCTION__, ret);
        NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
        return -ENOSYS;
    }
    ALOGV("%s: Audio decoder started (0x%x:%d)", __FUNCTION__, bout->config.format, start_settings.primary.codec);

    nexus_tunnel_bout_debounce_reset(bout);
    bout->nexus.tunnel.last_write_time = 0;

    return 0;
}

static int nexus_tunnel_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    if (bout->nexus.tunnel.debounce) {
       // Wait for the debouncing thread to finish
       ALOGV("%s: Waiting for debouncing to finish", __FUNCTION__);
       pthread_t thread = bout->nexus.tunnel.debounce_thread;
       bout->nexus.tunnel.debounce_stopping = true;

       pthread_mutex_unlock(&bout->lock);
       pthread_join(thread, NULL);
       pthread_mutex_lock(&bout->lock);
       ALOGV("%s:      ... done", __FUNCTION__);
    }

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (audio_decoder) {
        if (bout->frameSize > 0) {
            NEXUS_AudioDecoderStatus status;
            ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
            if (ret != NEXUS_SUCCESS) {
                ALOGE("%s: Update frame played failed, ret=%d", __FUNCTION__, ret);
            }
            else {
                bout->framesPlayed += status.numBytesDecoded/bout->frameSize;
            }
        }
        NEXUS_SimpleAudioDecoder_Stop(audio_decoder);
    }

    if (playpump) {
        NEXUS_Playpump_Stop(playpump);
    }
    ALOGV("%s: setting framesPlayed to %llu", __FUNCTION__, bout->framesPlayed);

    return 0;
}

static void *nexus_tunnel_bout_debounce_task(void *argv)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)argv;
    nsecs_t now;
    int delta_ms;

    ALOGV("%s: Debouncing started", __FUNCTION__);

    usleep(BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS * 1000);

    pthread_mutex_lock(&bout->lock);
    while (!bout->nexus.tunnel.debounce_stopping && bout->nexus.tunnel.debounce_more) {
       bout->nexus.tunnel.debounce_more = false;

       now = systemTime(SYSTEM_TIME_MONOTONIC);
       delta_ms = BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS - toMillisecondTimeoutDelay(bout->nexus.tunnel.last_pause_time, now);
       if (delta_ms > 0 && delta_ms <= BRCM_AUDIO_TUNNEL_DEBOUNCE_DURATION_MS) {
          ALOGV("%s: Debouncing more for %d ms", __FUNCTION__, delta_ms);
          pthread_mutex_unlock(&bout->lock);
          usleep(delta_ms * 1000);
          pthread_mutex_lock(&bout->lock);
       }
    }

    bout->nexus.tunnel.debounce_expired = true;

    if (!bout->nexus.tunnel.debounce_stopping) {
       if (bout->nexus.tunnel.debounce_pausing) {
          ALOGV("%s: Pause after debouncing", __FUNCTION__);
          nexus_tunnel_bout_pause(bout);
       }
       else {
          ALOGV("%s: Resume after debouncing", __FUNCTION__);
          nexus_tunnel_bout_resume(bout);
       }

       pthread_detach(pthread_self());
    }

    nexus_tunnel_bout_debounce_reset(bout);

    pthread_mutex_unlock(&bout->lock);

    ALOGV("%s: Debouncing stopped", __FUNCTION__);

    return NULL;
}

static int nexus_tunnel_bout_pause(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->nexus.tunnel.stc_channel ||
            (bout->nexus.tunnel.stc_channel == (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC)) {
       return -ENOENT;
    }

    // Audio underruns can happen when the app is not feeding the audio frames fast enough
    // especially at the beginning of the playback or after seeking. We debounce the pause/resume
    // operations in such underruns by deferring the pause for 0.3 seconds.
    if (!bout->nexus.tunnel.debounce) {
       if (pthread_create(&bout->nexus.tunnel.debounce_thread, NULL, nexus_tunnel_bout_debounce_task, bout) != 0) {
          ALOGE("%s: Error starting debouncing", __FUNCTION__);
       }
       else {
          pthread_setname_np(bout->nexus.tunnel.debounce_thread, "baudio_debounce");
          bout->nexus.tunnel.debounce = true;
          bout->nexus.tunnel.debounce_pausing = true;

          return 0;
       }
    }

    // No-op when debouncing
    if (bout->nexus.tunnel.debounce && !bout->nexus.tunnel.debounce_expired) {
       ALOGV("%s: No-op", __FUNCTION__);
       if (!bout->nexus.tunnel.debounce_pausing) {
          bout->nexus.tunnel.debounce_more = true;
          bout->nexus.tunnel.last_pause_time = systemTime(SYSTEM_TIME_MONOTONIC);
       }
       bout->nexus.tunnel.debounce_pausing = true;
       return 0;
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
            (bout->nexus.tunnel.stc_channel == (NEXUS_SimpleStcChannelHandle)(intptr_t)DUMMY_HW_SYNC)) {
       return -ENOENT;
    }

    // No-op when debouncing
    if (bout->nexus.tunnel.debounce && !bout->nexus.tunnel.debounce_expired) {
       ALOGV("%s: No-op", __FUNCTION__);
       bout->nexus.tunnel.debounce_pausing = false;
       bout->nexus.tunnel.debounce_more = false;
       return 0;
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
    NEXUS_Error res;
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (!audio_decoder || !playpump) {
        return -ENOENT;
    }

    res = NEXUS_Playpump_Flush(playpump);
    if (res != NEXUS_SUCCESS) {
       ALOGI("%s: Error flushing playpump %u", __FUNCTION__, res);
       return -ENOMEM;
    }

    NEXUS_SimpleAudioDecoder_Flush(audio_decoder);

    return 0;
}

// Table copied from AC3FrameScanner.h. It contains the number of 16-bit words in an AC3 frame.
#define AC3_MAX_FRAME_SIZE      38
#define AC3_MAX_SAMPLE_RATE     3
const static uint16_t dd_frame_size_table[AC3_MAX_FRAME_SIZE][AC3_MAX_SAMPLE_RATE] = {
    { 64, 69, 96 },
    { 64, 70, 96 },
    { 80, 87, 120 },
    { 80, 88, 120 },
    { 96, 104, 144 },
    { 96, 105, 144 },
    { 112, 121, 168 },
    { 112, 122, 168 },
    { 128, 139, 192 },
    { 128, 140, 192 },
    { 160, 174, 240 },
    { 160, 175, 240 },
    { 192, 208, 288 },
    { 192, 209, 288 },
    { 224, 243, 336 },
    { 224, 244, 336 },
    { 256, 278, 384 },
    { 256, 279, 384 },
    { 320, 348, 480 },
    { 320, 349, 480 },
    { 384, 417, 576 },
    { 384, 418, 576 },
    { 448, 487, 672 },
    { 448, 488, 672 },
    { 512, 557, 768 },
    { 512, 558, 768 },
    { 640, 696, 960 },
    { 640, 697, 960 },
    { 768, 835, 1152 },
    { 768, 836, 1152 },
    { 896, 975, 1344 },
    { 896, 976, 1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 }
};

#define DTS_MIN_FRAME_SIZE      95

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
                if (bytes < HW_AV_SYNC_HDR_LEN)
                {
                    ALOGV("%s: av-sync header with no payload (%zu)", __FUNCTION__, bytes);
                    break;
                }

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

                // For compressed audio, apparently we assume the frame size stays constant throughout
                // the entire playback. Will make use of the status from the audio decoder once the
                // audio decoder is able to return the number decoded frames.
                if (bout->frameSize == 0) {
                    const uint8_t *syncFrame = (const uint8_t *)buffer;
                    switch (bout->config.format) {
                        case AUDIO_FORMAT_AC3:
                        case AUDIO_FORMAT_E_AC3:
                        {
                            if (bytes >= 5 && syncFrame[0] == 0x0B && syncFrame[1] == 0x77) {
                                if (bout->config.format == AUDIO_FORMAT_E_AC3) {
                                    uint32_t frmsiz = ((syncFrame[2] & 0x07) << 8) + syncFrame[3];
                                    bout->frameSize = (frmsiz + 1) * sizeof(uint16_t);
                                }
                                else { // AUDIO_FORMAT_AC3
                                    uint32_t fscod = syncFrame[4] >> 6;
                                    uint32_t frmsizcod = syncFrame[4] & 0x3F;
                                    if (fscod < AC3_MAX_SAMPLE_RATE && frmsizcod < AC3_MAX_FRAME_SIZE) {
                                        bout->frameSize = dd_frame_size_table[frmsizcod][fscod] * sizeof(uint16_t);
                                    }
                                }
                            }
                            break;
                        }
                        case AUDIO_FORMAT_DTS:
                        case AUDIO_FORMAT_DTS_HD:
                        {
                            if (bytes >= 8 && syncFrame[0] == 0x7F && syncFrame[1] == 0xFE && syncFrame[2] == 0x80 && syncFrame[3] == 0x01) {
                                uint32_t fsize = ((syncFrame[5] & 0x03) << 12) + (syncFrame[6] << 4) + (syncFrame[7] & 0xF0);
                                if (fsize >= DTS_MIN_FRAME_SIZE) {
                                    bout->frameSize = fsize + 1;
                                }
                            }
                            break;
                        }
                        default:
                        {
                            ALOGE("%s: Unexpected compressed audio format 0x%08x", __FUNCTION__, bout->config.format);
                            break;
                        }
                    }
                    ALOGV("%s: Frame size %u for audio format 0x%08x", __FUNCTION__, bout->frameSize, bout->config.format);
                }
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
            size_t spaceRequired, length;

            ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
            if (ret) {
                ALOGE("%s: get playpump buffer failed, ret=%d", __FUNCTION__, ret);
                ret = -ENOSYS;
                break;
            }

            spaceRequired = (frameBytes >= bytes) ? frameBytes : bytes;
            if ( writeHeader ) {
                spaceRequired += BMEDIA_PES_HEADER_MAX_SIZE;
                if (bout->nexus.tunnel.pcm_format) {
                    spaceRequired += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
                }
            }
            if (nexus_space >= spaceRequired) {
                used = 0;
                if ( writeHeader ) {
                    // Format PES
                    BMEDIA_PES_INFO_INIT(&pes, BRCM_AUDIO_STREAM_ID);
                    BMEDIA_PES_SET_PTS(&pes, pts);
                    length = frameBytes;
                    if (bout->nexus.tunnel.pcm_format) {
                        length += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
                    }
                    used += bmedia_pes_header_init((uint8_t *)nexus_buffer, length, &pes);
                    if (bout->nexus.tunnel.pcm_format) {
                        BKNI_Memcpy((uint8_t *)nexus_buffer + used, bmedia_frame_bcma.base, bmedia_frame_bcma.len);
                        used += bmedia_frame_bcma.len;
                        B_MEDIA_SAVE_UINT32_BE((uint8_t *)nexus_buffer + used, frameBytes);
                        used += sizeof(uint32_t);

                        // Format WAV header
                        waveformat_size = bmedia_write_waveformatex((uint8_t *)nexus_buffer + used, &bout->nexus.tunnel.wave_fmt);
                        ALOG_ASSERT(waveformat_size == BMEDIA_WAVEFORMATEX_BASE_SIZE);
                        used += waveformat_size;
                    }

                    // Header may indicate more data in frame than was passed in write()
                    if ( frameBytes > bytes ) {
                        frameBytes = bytes;
                    }
                }

                BKNI_Memcpy((uint8_t *)nexus_buffer + used, buffer, frameBytes);
                used += frameBytes;
                BDBG_ASSERT(used <= nexus_space);

                if (bout->nexus.tunnel.pes_debug != NULL) {
                    fwrite(nexus_buffer, 1, used, bout->nexus.tunnel.pes_debug);
                }
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
                    ALOG_ASSERT(bytes >= frameBytes);
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

                if (ret != BERR_SUCCESS) {
                    ALOGE("%s: playpump write timeout, ret=%d", __FUNCTION__, ret);
                    ret = -ENOSYS;
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
    if (bout->nexus.tunnel.pcm_format) {
        nsecs_t delta = systemTime(SYSTEM_TIME_MONOTONIC) - bout->nexus.tunnel.last_write_time;
        int32_t throttle_us = BRCM_AUDIO_TUNNEL_HALF_DURATION_US - (delta / 1000);
        if (throttle_us <= BRCM_AUDIO_TUNNEL_HALF_DURATION_US && throttle_us > 0) {
            ALOGV("%s: throttle %d us", __FUNCTION__, throttle_us);
            usleep(throttle_us);
        }
        bout->nexus.tunnel.last_write_time = systemTime(SYSTEM_TIME_MONOTONIC);
    }
    else {
        NEXUS_Error rc;
        NEXUS_AudioDecoderStatus status;
        uint32_t i;
        for (i = 0; i < BRCM_AUDIO_TUNNEL_COMP_DRAIN_DELAY_MAX; i++) {
            rc = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
            if (rc != NEXUS_SUCCESS) {
                break;
            }
            if (status.queuedFrames < BRCM_AUDIO_TUNNEL_COMP_FRAME_QUEUED) {
                break;
            }
            usleep(BRCM_AUDIO_TUNNEL_COMP_DRAIN_DELAY_US);
        }
        ALOGV_IF(i > 0, "%s: throttle %ld us queued %u frames", __FUNCTION__, i * BRCM_AUDIO_TUNNEL_COMP_DRAIN_DELAY_US, status.queuedFrames);
    }

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

    /* Check if sample rate is supported */
    for (i = 0; i < (int)NEXUS_OUT_SAMPLE_RATE_COUNT; i++) {
        if (config->sample_rate == nexus_out_sample_rates[i]) {
            break;
        }
    }
    if (i == NEXUS_OUT_SAMPLE_RATE_COUNT) {
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    }

    if (config->channel_mask == 0) {
        config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    }
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        config->format = NEXUS_OUT_DEFAULT_FORMAT;
    }

    bout->framesPlayed = 0;

    /* Frame size should be single byte long for compressed audio formats */
    size_t bytes_per_sample = audio_bytes_per_sample(config->format);
    bout->nexus.tunnel.pcm_format = (bytes_per_sample == 0) ? false : true;
    bout->frameSize = bout->nexus.tunnel.pcm_format ? bytes_per_sample * popcount(config->channel_mask) : 0;
    bout->buffer_size = bout->nexus.tunnel.pcm_format ?
                                get_brcm_audio_buffer_size(config->sample_rate,
                                   config->format,
                                   popcount(config->channel_mask),
                                   BRCM_AUDIO_TUNNEL_DURATION_MS) :
                                BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE;

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
    if (!bout->nexus.tunnel.audio_decoder) {
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

    if (bout->nexus.tunnel.pcm_format) {
        // Initialize WAV format parameters
        bmedia_waveformatex_header *wave_fmt = &bout->nexus.tunnel.wave_fmt;
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
    }

    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

    if (property_get_int32(BRCM_AUDIO_TUNNEL_PROPERTY_PES_DEBUG, 0)) {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique filename
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(fname, sizeof(fname), "/data/nxmedia/aout-tunnel-dev_%u-", bout->devices);
        strftime(fname+strlen(fname), sizeof(fname)-strlen(fname), "%F_%H_%M_%S.pes", timeinfo);
        ALOGD("Tunneled audio PES debug output file:%s", fname);
        bout->nexus.tunnel.pes_debug = fopen(fname, "wb+");
        if (bout->nexus.tunnel.pes_debug == NULL) {
            ALOGW("Error creating tunneled audio PES output debug file %s (%s)", fname, strerror(errno));
            // Just keep going without debug
        }
    }
    else {
        bout->nexus.tunnel.pes_debug = NULL;
    }

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
        bout->nexus.tunnel.audio_decoder = NULL;
    }

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));
    NxClient_Uninit();
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    if (bout->nexus.tunnel.pes_debug) {
        fclose(bout->nexus.tunnel.pes_debug);
        bout->nexus.tunnel.pes_debug = NULL;
    }

    return 0;
}

static char *nexus_tunnel_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    String8 rates_str, channels_str, formats_str;
    char* result_str;

    /* Get connected HDMI status */
    nexus_get_hdmi_parameters(rates_str, channels_str, formats_str);

    pthread_mutex_lock(&bout->lock);
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_HW_AV_SYNC)) {
        str_parms_add_int(result, AUDIO_PARAMETER_STREAM_HW_AV_SYNC,
                          (int)(intptr_t)bout->nexus.tunnel.stc_channel);
    }

    /* Supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_SAMPLE_RATE) : rates_str.string());
    }

    /* Supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_CHANNELS) : channels_str.string());
    }

    /* Supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          formats_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_FORMAT) : formats_str.string());
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
