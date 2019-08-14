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
#include "brcm_audio_nexus_parser.h"
#include "OMX_Types.h"
#include "bomx_utils.h"
#include <inttypes.h>
#include "nexus_base_mmap.h"
#include "nexus_platform.h"
#include "nxclient.h"

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

#define BRCM_AUDIO_STREAM_ID                    (0xC0)

#define BRCM_AUDIO_TUNNEL_DURATION_MS           (5)
#define BRCM_AUDIO_TUNNEL_HALF_DURATION_US      (BRCM_AUDIO_TUNNEL_DURATION_MS * 500)
#define BRCM_AUDIO_TUNNEL_FIFO_DURATION_MS      (200)
#define BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER       (BRCM_AUDIO_TUNNEL_FIFO_DURATION_MS / BRCM_AUDIO_TUNNEL_DURATION_MS)
#define BRCM_AUDIO_TUNNEL_NEXUS_LATENCY_MS      (128)   // Audio latency is 128ms + lipsync offset in standard latency mode

#define BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE      (2048)
#define BRCM_AUDIO_TUNNEL_COMP_DELAY_MAX_MS     (200)
#define BRCM_AUDIO_TUNNEL_COMP_EST_PERIOD_MS    (125)
#define BRCM_AUDIO_TUNNEL_COMP_EST_BYTE_MUL     (5)
#define BRCM_AUDIO_TUNNEL_COMP_THRES_MUL        (4)

#define BRCM_AUDIO_TUNNEL_STC_SYNC_INVALID      (0xFFFFFFFF)

#define PCM_STOP_FILL_TARGET                    250 // 250 ms
#define PCM_START_PLAY_TARGET                   375 // 375 ms

#define KBITRATE_TO_BYTES_PER_250MS(kbr)        ((kbr) * 32)    // 1024/8/8*2
#define KBITRATE_TO_BYTES_PER_375MS(kbr)        ((kbr) * 48)    // 1024/8/8*3

#define PCM_MUTE_TIME                           10

/*
 * Function declarations
 */
static int nexus_tunnel_sink_pause(struct brcm_stream_out *bout);
static int nexus_tunnel_sink_resume(struct brcm_stream_out *bout);
static bool nexus_tunnel_sink_standby_monitor(void *context);
static void nexus_tunnel_sink_parse_ac3(struct brcm_stream_out *bout, const void* buffer, size_t bytes);

/*
 * Operation Functions
 */
static int nexus_tunnel_sink_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;

    if (left != right) {
        BA_LOG(TUN_DBG, "%s: Left and Right volumes must be equal, cannot change volume", __FUNCTION__);
        return 0;
    }
    else if (left > 1.0) {
        BA_LOG(TUN_DBG, "%s: Volume: %f exceeds max volume of 1.0", __FUNCTION__, left);
        return 0;
    }

    if (bout->nexus.tunnel.fadeLevel != (unsigned)(left * 100)) {
        bout->nexus.tunnel.fadeLevel = (unsigned)(left * 100);

        BA_LOG(TUN_DBG, "%s: Volume: %d", __FUNCTION__, bout->nexus.tunnel.fadeLevel);
        if (bout->started) {
            if (bout->nexus.tunnel.deferred_volume) {
                struct timespec now;

                clock_gettime(CLOCK_MONOTONIC, &now);
                if (nexus_common_is_paused(audio_decoder)) {
                    // Record deferral amount if volume is set during priming
                    int32_t deferred_ms;
                    deferred_ms = (int32_t)((now.tv_sec - bout->nexus.tunnel.start_ts.tv_sec) * 1000) +
                                  (int32_t)((now.tv_nsec - bout->nexus.tunnel.start_ts.tv_nsec)/1000000);
                    bout->nexus.tunnel.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;
                } else {
                    if (timespec_greater_than(now, bout->nexus.tunnel.deferred_window)) {
                        // Deferral window over, resume normal operation
                        bout->nexus.tunnel.deferred_volume = false;
                        nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL, -1, -1);
                    } else {
                        // Push deferral window based on current time
                        bout->nexus.tunnel.deferred_window = now;
                        timespec_add_ms(bout->nexus.tunnel.deferred_window, MAX_VOLUME_DEFERRAL);

                        nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL,
                            bout->nexus.tunnel.deferred_volume_ms, -1);
                    }
                }
            } else if (!nexus_common_is_paused(audio_decoder)) {
                // Apply volume immediately if not paused
                nexus_common_set_volume(bout->bdev, audio_decoder, bout->nexus.tunnel.fadeLevel, NULL, -1, -1);
            }
        }
    }

    return 0;
}

static unsigned nexus_tunnel_sink_get_frame_multipler(struct brcm_stream_out *bout)
{
    unsigned res = 1;

    switch (bout->config.format) {
        case AUDIO_FORMAT_AC3:
        {
            res = NEXUS_PCM_FRAMES_PER_AC3_FRAME;
            break;
        }
        case AUDIO_FORMAT_E_AC3:
        {
            res = (bout->nexus.tunnel.audioblocks_per_frame > 0) ?
                                        (bout->nexus.tunnel.audioblocks_per_frame * NEXUS_PCM_FRAMES_PER_AC3_AUDIO_BLOCK) :
                                        NEXUS_PCM_FRAMES_PER_AC3_FRAME; // Assuming 6 by default
            break;
        }
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
        {
            res = (bout->nexus.tunnel.audioblocks_per_frame > 0) ?
                                        (bout->nexus.tunnel.audioblocks_per_frame * NEXUS_PCM_FRAMES_PER_DTS_SAMPLE_BLOCK) :
                                        NEXUS_PCM_FRAMES_PER_DTS_FRAME; // Assuming 16 by default
            break;
        }
        default:
        {
            // Return the default value of 1
            break;
        }
    }

    return res;
}

static int nexus_tunnel_sink_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (audio_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get render position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (!status.started) {
            *dsp_frames = 0;
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                *dsp_frames = (uint32_t)(bytes/bout->frameSize);
            }
            else {
                *dsp_frames = (uint32_t)(bout->framesPlayed * bout->nexus.tunnel.frame_multiplier);
            }
        }
    }
    else {
        *dsp_frames = 0;
    }

    BA_LOG(TUN_POS, "Render pos:%u", *dsp_frames);
    return 0;
}

static int nexus_tunnel_sink_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (audio_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get presentation position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (!status.started) {
            *frames = bout->framesPlayedTotal;
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                *frames = bout->framesPlayedTotal + bytes/bout->frameSize;
            }
            else {
                *frames = (bout->framesPlayedTotal + bout->framesPlayed) * bout->nexus.tunnel.frame_multiplier;
            }
        }
    }
    else {
        *frames = 0;
    }

    BA_LOG(TUN_POS, "Presentation pos:%" PRIu64 "", *frames);
    return 0;
}

static uint32_t nexus_tunnel_sink_get_latency(struct brcm_stream_out *bout)
{
    UNUSED(bout);
    return BRCM_AUDIO_TUNNEL_NEXUS_LATENCY_MS;
}

static int32_t nexus_tunnel_sink_get_fifo_depth(struct brcm_stream_out *bout)
{
    NEXUS_AudioDecoderStatus decStatus;
    NEXUS_PlaypumpStatus ppStatus;
    unsigned bitrate = 0;

    NEXUS_Error rc = NEXUS_SimpleAudioDecoder_GetStatus(bout->nexus.tunnel.audio_decoder, &decStatus);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: Error retriving audio decoder status %u", __FUNCTION__, rc);
        return -1;
    }
    rc = NEXUS_Playpump_GetStatus(bout->nexus.tunnel.playpump, &ppStatus);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: Error retriving playpump status %u", __FUNCTION__, rc);
        return -1;
    }

    switch (decStatus.codec) {
        case NEXUS_AudioCodec_eAc3:
        case NEXUS_AudioCodec_eAc3Plus: {
            bitrate = decStatus.codecStatus.ac3.bitrate;
            break;
        }
        case NEXUS_AudioCodec_eDts:
        case NEXUS_AudioCodec_eDtsHd: {
            bitrate = decStatus.codecStatus.dts.bitRate;
            break;
        }
        default: {
            break;
        }
    }
    if (bitrate > 0 && bitrate != bout->nexus.tunnel.bitrate) {
        bout->nexus.tunnel.bitrate = bitrate;
        ALOGI("%s: new bitrate detected: %u", __FUNCTION__, bout->nexus.tunnel.bitrate);
    }

    BA_LOG(TUN_DBG, "%s: written ts=%" PRIu64 " pts=%" PRIu32 " -> status pts=%" PRIu32, __FUNCTION__,
                bout->tunnel_base.last_written_ts,
                BOMX_TickToPts((OMX_TICKS *)&bout->tunnel_base.last_written_ts),
                decStatus.pts);

    BA_LOG(TUN_DBG, "%s: %zu(%zu/%zu) thrs=%u/%u", __FUNCTION__,
       (size_t)(ppStatus.fifoDepth + decStatus.fifoDepth), ppStatus.fifoDepth, (size_t)decStatus.fifoDepth,
       KBITRATE_TO_BYTES_PER_250MS(bout->nexus.tunnel.bitrate), KBITRATE_TO_BYTES_PER_375MS(bout->nexus.tunnel.bitrate));

    return ppStatus.fifoDepth + decStatus.fifoDepth;
}

static bool nexus_tunnel_sink_pause_int(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);
    ALOGV_FIFO_INFO(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.playpump);

    // Stop volume deferral as soon as paused
    bout->nexus.tunnel.deferred_volume = false;

    res = nexus_common_mute_and_pause(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->tunnel_base.stc_channel,
                                     bout->nexus.tunnel.soft_muting,
                                     bout->nexus.tunnel.sleep_after_mute);

    if (res != NEXUS_SUCCESS) {
        ALOGE("%s: Error pausing %u", __FUNCTION__, res);
        return false;
    }

    ALOGV_FIFO_INFO(bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.playpump);
    return true;
}

static bool nexus_tunnel_sink_resume_int(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    BA_LOG(TUN_STATE, "%s, %p", __FUNCTION__, bout);

    if (!nexus_common_is_paused(bout->nexus.tunnel.audio_decoder)) {
        // Not paused. Nothing to resume
        return true;
    }

    int32_t fifoDepth = nexus_tunnel_sink_get_fifo_depth(bout);
    if (fifoDepth < 0) {
        return false;
    }

    if ((bout->nexus.tunnel.pcm_format && (uint32_t)fifoDepth >= get_brcm_audio_buffer_size(bout->config.sample_rate,
                                                               bout->config.format,
                                                               popcount(bout->config.channel_mask),
                                                               PCM_START_PLAY_TARGET)) ||
        (!bout->nexus.tunnel.pcm_format &&
          bout->nexus.tunnel.bitrate > 0 && (uint32_t)fifoDepth >= KBITRATE_TO_BYTES_PER_375MS(bout->nexus.tunnel.bitrate))) {
        BA_LOG(TUN_DBG, "%s: Resume without priming", __FUNCTION__);

        if (bout->nexus.tunnel.deferred_volume) {
            if (bout->nexus.tunnel.deferred_volume_ms) {
                struct timespec now;
                int32_t deferred_ms;

                clock_gettime(CLOCK_MONOTONIC, &now);

                if (bout->nexus.tunnel.pcm_format) {
                    // Resume muted first, then apply deferred volume
                    res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->tunnel_base.stc_channel,
                                                         0, 0);
                    nexus_common_set_volume(bout->bdev, bout->nexus.tunnel.audio_decoder, 0, NULL, PCM_MUTE_TIME, 1);
                    nexus_common_set_volume(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->nexus.tunnel.fadeLevel, NULL,
                                                         bout->nexus.tunnel.deferred_volume_ms, -1);
                } else {
                    // Apply existing deferred volume
                    res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->tunnel_base.stc_channel,
                                                         bout->nexus.tunnel.deferred_volume_ms, bout->nexus.tunnel.fadeLevel);
                }

                // Record delta between resume and first start as subsequent deferral amount
                deferred_ms = (int32_t)((now.tv_sec - bout->nexus.tunnel.start_ts.tv_sec) * 1000) +
                              (int32_t)((now.tv_nsec - bout->nexus.tunnel.start_ts.tv_nsec)/1000000);
                bout->nexus.tunnel.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;

                // Set deferral window
                bout->nexus.tunnel.deferred_window = now;
                timespec_add_ms(bout->nexus.tunnel.deferred_window, MAX_VOLUME_DEFERRAL);
            } else {
                // No volume was set during priming, no need to defer
                bout->nexus.tunnel.deferred_volume = false;
            }
        }

        if (!bout->nexus.tunnel.deferred_volume) {
            res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.tunnel.audio_decoder, bout->tunnel_base.stc_channel,
                                                 bout->nexus.tunnel.soft_unmuting, bout->nexus.tunnel.fadeLevel);
        }


        if (res != NEXUS_SUCCESS) {
           ALOGE("%s: Error resuming %u", __FUNCTION__, res);
           return false;
        }
        BA_LOG(TUN_DBG, "%s: Resume priming over", __FUNCTION__);
        bout->nexus.tunnel.priming = false;
    }
    else {
        BA_LOG(TUN_DBG, "%s: Resume with priming", __FUNCTION__);
        bout->nexus.tunnel.priming = true;
    }

    return true;
}

static int nexus_tunnel_sink_start(struct brcm_stream_out *bout)
{
    UNUSED(bout);

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);
    /* Nothing to do here.  Handle the real start when the first valid PTS is detected */
    clock_gettime(CLOCK_MONOTONIC, &bout->nexus.tunnel.start_ts);

    return 0;
}

static int nexus_tunnel_sink_start_int(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_SimpleAudioDecoderStartSettings start_settings;
    NEXUS_SimpleAudioDecoderSettings settings;

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

    unsigned threshold;
    if (bout->nexus.tunnel.pcm_format) {
        threshold = get_brcm_audio_buffer_size(bout->config.sample_rate,
                                               bout->config.format,
                                               popcount(bout->config.channel_mask),
                                               PCM_START_PLAY_TARGET);
    } else {
        threshold = bout->buffer_size * BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER;
    }
    NEXUS_SimpleAudioDecoder_GetSettings(audio_decoder, &settings);
    BA_LOG(TUN_DBG, "Primary fifoThreshold %u->%u",
              settings.primary.fifoThreshold, threshold);
    settings.primary.fifoThreshold = threshold;
    NEXUS_SimpleAudioDecoder_SetSettings(audio_decoder, &settings);

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);
    start_settings.primary.codec = brcm_audio_get_codec_from_format(bout->config.format);
    start_settings.primary.pidChannel = bout->nexus.tunnel.pid_channel;

    if (bout->bdev->dolby_ms == 12) {
        start_settings.primary.mixingMode = NEXUS_AudioDecoderMixingMode_eStandalone;

        if ((start_settings.primary.codec == NEXUS_AudioCodec_eAc3) ||
            (start_settings.primary.codec == NEXUS_AudioCodec_eAc3Plus)) {
            NEXUS_AudioDecoderCodecSettings codecSettings;
            NEXUS_AudioDecoderDolbySettings *dolbySettings;

            // Get default settings
            NEXUS_SimpleAudioDecoder_GetCodecSettings(audio_decoder,
                    NEXUS_SimpleAudioDecoderSelector_ePrimary, start_settings.primary.codec, &codecSettings);
            if (codecSettings.codec != start_settings.primary.codec) {
                ALOGE("%s: Codec mismatch %d != %d", __FUNCTION__,
                        codecSettings.codec, start_settings.primary.codec);
                NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
                return -ENOSYS;
            }

            dolbySettings = (codecSettings.codec == NEXUS_AudioCodec_eAc3)?
                                &codecSettings.codecSettings.ac3:
                                &codecSettings.codecSettings.ac3Plus;

            dolbySettings->enableAtmosProcessing = true;
            if (property_get_bool(BCM_RO_AUDIO_DISABLE_ATMOS, false) ||
                property_get_bool(BCM_PERSIST_AUDIO_DISABLE_ATMOS, false)) {
                dolbySettings->enableAtmosProcessing = false;
            }

            ALOGI("%s: %s Dolby ATMOS", __FUNCTION__,
                dolbySettings->enableAtmosProcessing?"Enabling":"Disabling");

            ret = NEXUS_SimpleAudioDecoder_SetCodecSettings(audio_decoder,
                    NEXUS_SimpleAudioDecoderSelector_ePrimary, &codecSettings);
            if (ret) {
                ALOGE("%s: Unable to set codec %d settings, ret = %d", __FUNCTION__,
                        start_settings.primary.codec, ret);
                NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
                return -ENOSYS;
            }
        }
    }

    ret = NEXUS_SimpleAudioDecoder_Start(audio_decoder, &start_settings);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Start audio decoder failed, ret = %d", __FUNCTION__, ret);
        NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
        return -ENOSYS;
    }
    BA_LOG(TUN_DBG, "%s: Audio decoder started (0x%x:%d)",
                     __FUNCTION__, bout->config.format, start_settings.primary.codec);

    bout->nexus.tunnel.priming = false;
    ret = nexus_common_mute_and_pause(bout->bdev, audio_decoder, bout->tunnel_base.stc_channel, 0, 0);
    if (ret != NEXUS_SUCCESS) {
        ALOGE("%s: Error pausing audio decoder for priming %u", __FUNCTION__, ret);
    } else {
        bout->nexus.tunnel.priming = true;
    }

    bout->nexus.tunnel.last_write_time = 0;
    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_sink_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;
    bout->framesPlayed = 0;
    bout->tunnel_base.started = true;

    return 0;
}

static int nexus_tunnel_sink_stop(struct brcm_stream_out *bout)
{
    NEXUS_Error ret;

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);

    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    ALOGV_FIFO_INFO(audio_decoder, playpump);

    if (audio_decoder) {
        NEXUS_SimpleAudioDecoder_Stop(audio_decoder);

        NEXUS_AudioDecoderStatus status;
        ret = NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Update frame played failed, ret=%d", __FUNCTION__, ret);
        }
        else {
            bout->framesPlayed += status.framesDecoded - bout->nexus.tunnel.lastCount;
            bout->nexus.tunnel.lastCount = status.framesDecoded;

            if (bout->nexus.tunnel.pcm_format) {
                uint64_t bytes = status.numBytesDecoded -
                   (bout->framesPlayed * (BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4));
                bout->framesPlayedTotal += bytes/bout->frameSize;
            }
            else {
                bout->framesPlayedTotal += bout->framesPlayed;
            }
        }
    }

    if (playpump) {
        NEXUS_Playpump_Stop(playpump);
    }

    // In case stc channel was left frozen by the previous output stream (during seeking)
    ret = NEXUS_SimpleStcChannel_Freeze(bout->tunnel_base.stc_channel, false);
    if (ret != NEXUS_SUCCESS) {
       ALOGE("%s: Error starting STC %d", __FUNCTION__, ret);
       NEXUS_Playpump_Stop(bout->nexus.tunnel.playpump);
       return -ENOSYS;
    }

    BA_LOG(TUN_DBG, "%s: setting framesPlayedTotal to %" PRIu64 "", __FUNCTION__, bout->framesPlayedTotal);
    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_sink_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;

    return 0;
}

static int nexus_tunnel_sink_pause(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->tunnel_base.stc_channel) {
       return -ENOENT;
    }

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);
    if (!bout->nexus.tunnel.priming) {
        if (!nexus_tunnel_sink_pause_int(bout)) {
            return -ENOMEM;
        }
    }
    else {
        BA_LOG(TUN_DBG, "%s: Stop priming", __FUNCTION__);
        bout->nexus.tunnel.priming = false;
    }

    return 0;
}

static int nexus_tunnel_sink_resume(struct brcm_stream_out *bout)
{
    NEXUS_Error res;

    if (!bout->nexus.tunnel.audio_decoder || !bout->tunnel_base.stc_channel) {
       return -ENOENT;
    }

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);

    if (!bout->nexus.tunnel.priming) {
        BA_LOG(TUN_DBG, "%s: priming decoder for resume", __FUNCTION__);
        bout->nexus.tunnel.priming = true;
    }

    return 0;
}

static int nexus_tunnel_sink_drain(struct brcm_stream_out *bout, int action)
{
    (void)bout;
    (void)action;
    // TBD
    return 0;
}

static int nexus_tunnel_sink_flush(struct brcm_stream_out *bout)
{
    NEXUS_Error res;
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    if (!audio_decoder || !playpump) {
        return -ENOENT;
    }

    BA_LOG(TUN_STATE, "%s, %p, started=%s", __FUNCTION__, bout, bout->tunnel_base.started?"true":"false");
    if (bout->started) {
        res = NEXUS_Playpump_Flush(playpump);
        if (res != NEXUS_SUCCESS) {
            ALOGE("%s: Error flushing playpump %u", __FUNCTION__, res);
            return -ENOMEM;
        }

        NEXUS_SimpleAudioDecoder_Flush(audio_decoder);
        bout->nexus.tunnel.priming = false;
        res = nexus_common_mute_and_pause(bout->bdev, audio_decoder, bout->tunnel_base.stc_channel, 0, 0);
        if (res != NEXUS_SUCCESS) {
            ALOGE("%s: Error pausing audio decoder for priming %u", __FUNCTION__, res);
        } else {
            bout->nexus.tunnel.priming = true;
        }
    }

    bout->nexus.tunnel.lastCount = 0;
    bout->nexus.tunnel.audioblocks_per_frame = 0;
    bout->nexus.tunnel.frame_multiplier = nexus_tunnel_sink_get_frame_multipler(bout);
    bout->nexus.tunnel.bitrate = 0;

    return 0;
}

static void nexus_tunnel_sink_playpump_callback(void *context, int param)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;
    ALOG_ASSERT(bout->nexus.event == (BKNI_EventHandle)(intptr_t)param);
    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param);
}

static int nexus_tunnel_sink_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_Error rc;
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

    bout->framesPlayedTotal = 0;
    bout->tunnel_base.started = false;

    size_t bytes_per_sample = audio_bytes_per_sample(config->format);
    bout->nexus.tunnel.pcm_format = (bytes_per_sample == 0) ? false : true;
    if (bout->nexus.tunnel.pcm_format) {
        bout->frameSize = bytes_per_sample * popcount(config->channel_mask);
        bout->buffer_size = get_brcm_audio_buffer_size(config->sample_rate,
                                                       config->format,
                                                       popcount(config->channel_mask),
                                                       BRCM_AUDIO_TUNNEL_DURATION_MS);
    }
    else {
        /* Frame size should be single byte long for compressed audio formats */
        bout->frameSize = 1;
        bout->buffer_size = BRCM_AUDIO_TUNNEL_COMP_BUFFER_SIZE;
    }

    BA_LOG(TUN_STATE, "%s: sample_rate=%" PRIu32 " frameSize=%" PRIu32 " buffer_size=%zu",
            __FUNCTION__, config->sample_rate, bout->frameSize, bout->buffer_size);
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

    if (bout->bdev->dolby_ms == 12) {
        connectSettings.simpleAudioDecoder.decoderCapabilities.type = NxClient_AudioDecoderType_ePersistent;

        NEXUS_AudioCodec nxcodec = brcm_audio_get_codec_from_format(config->format);
        if ((nxcodec == NEXUS_AudioCodec_eAc3) || (nxcodec == NEXUS_AudioCodec_eAc3Plus)) {
           NxClient_AudioProcessingSettings aps;
           NxClient_GetAudioProcessingSettings(&aps);
           if (aps.dolby.ddre.profile == NEXUS_DolbyDigitalReencodeProfile_eNoCompression) {
              aps.dolby.ddre.profile = NEXUS_DolbyDigitalReencodeProfile_eFilmStandardCompression;
              NxClient_SetAudioProcessingSettings(&aps);
           }
        }
    }

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
    bout->standbyCallback = bout->bdev->standbyThread->RegisterCallback(nexus_tunnel_sink_standby_monitor, bout);
    if (bout->standbyCallback < 0) {
        ALOGE("Error registering standby callback");
        ret = -ENOSYS;
        goto err_callback;
    }

    size_t fifoSize;
    NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    fifoSize = bout->buffer_size * BRCM_AUDIO_TUNNEL_FIFO_MULTIPLIER;
    BA_LOG(TUN_DBG,  "Playpump fifo size %zu->%zu",
              playpumpOpenSettings.fifoSize, fifoSize);
    playpumpOpenSettings.fifoSize = fifoSize;
    bout->nexus.tunnel.playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
    if (!bout->nexus.tunnel.playpump) {
        ALOGE("%s: Error opening playpump", __FUNCTION__);
        ret = -ENOMEM;
        goto err_playpump;
    }

    NEXUS_Playpump_GetSettings(bout->nexus.tunnel.playpump, &playpumpSettings);
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.dataCallback.callback = nexus_tunnel_sink_playpump_callback;
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

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
    pidSettings.pidType = NEXUS_PidType_eAudio;
    bout->nexus.tunnel.pid_channel = NEXUS_Playpump_OpenPidChannel(bout->nexus.tunnel.playpump, BRCM_AUDIO_STREAM_ID, &pidSettings);
    if (!bout->nexus.tunnel.pid_channel) {
        ALOGE("%s: Error openning pid channel", __FUNCTION__);
        ret = -ENOMEM;
        goto err_pid;
    }

    ALOG_ASSERT(bout->tunnel_base.stc_channel != NULL);
    NEXUS_SimpleAudioDecoder_SetStcChannel(bout->nexus.tunnel.audio_decoder, bout->tunnel_base.stc_channel);

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
        BA_LOG(TUN_DBG, "%s: wave_fmt channel=%d sample_rate=%" PRId32 " bit_per_sample=%d align=%d avg_bytes_rate=%" PRId32 "",
                __FUNCTION__, wave_fmt->nChannels, wave_fmt->nSamplesPerSec, wave_fmt->wBitsPerSample,
                wave_fmt->nBlockAlign, wave_fmt->nAvgBytesPerSec);
    }

    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

    bout->nexus.tunnel.soft_muting = property_get_int32(BCM_RO_AUDIO_SOFT_MUTING, 10);
    bout->nexus.tunnel.soft_unmuting = property_get_int32(BCM_RO_AUDIO_SOFT_UNMUTING, 30);
    bout->nexus.tunnel.sleep_after_mute = property_get_int32(BCM_RO_AUDIO_SLEEP_AFTER_MUTE, 30);

    if (property_get_int32(BCM_RO_AUDIO_TUNNEL_PROPERTY_PES_DEBUG, 0)) {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        // Generate unique filename
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(fname, sizeof(fname), "/data/vendor/nxmedia/aout-tunnel-dev_%u-", bout->devices);
        strftime(fname+strlen(fname), sizeof(fname)-strlen(fname), "%F_%H_%M_%S.pes", timeinfo);
        ALOGD("Tunneled audio PES debug output file:%s", fname);
        bout->tunnel_base.pes_debug = fopen(fname, "wb+");
        if (bout->tunnel_base.pes_debug == NULL) {
            ALOGW("Error creating tunneled audio PES output debug file %s (%s)", fname, strerror(errno));
            // Just keep going without debug
        }
    }
    else {
        bout->tunnel_base.pes_debug = NULL;
    }

    // Restore auto mode for MS11
    if ((bout->bdev->dolby_ms == 11) && !bout->nexus.tunnel.pcm_format) {
        NxClient_AudioSettings audioSettings;

        ALOGI("Force auto output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_eAuto;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_eAuto;
        ret = NxClient_SetAudioSettings(&audioSettings);
        if (ret) {
            ALOGE("%s: Error setting auto mode, ret = %d", __FUNCTION__, ret);
        }
    }


    bout->nexus.tunnel.fadeLevel = 100;
    bout->nexus.tunnel.deferred_volume = true;
    bout->nexus.tunnel.deferred_volume_ms = 0;

    return 0;

err_pid:
    bout->tunnel_base.stc_channel = NULL;
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
    return ret;
}

static int nexus_tunnel_sink_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

    if (bout->nexus.state == BRCM_NEXUS_STATE_DESTROYED) {
       return 0;
    }

    BA_LOG(TUN_STATE, "%s", __FUNCTION__);

    if (bout->bdev->dolby_ms == 12) {
       NxClient_AudioProcessingSettings aps;
       NxClient_GetAudioProcessingSettings(&aps);
       if (aps.dolby.ddre.profile != NEXUS_DolbyDigitalReencodeProfile_eNoCompression) {
          aps.dolby.ddre.profile = NEXUS_DolbyDigitalReencodeProfile_eNoCompression;
          NxClient_SetAudioProcessingSettings(&aps);
       }
    }

    // Force PCM mode for MS11
    if ((bout->bdev->dolby_ms == 11) && !bout->nexus.tunnel.pcm_format) {
        NxClient_AudioSettings audioSettings;

        ALOGI("Force PCM output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_ePcm;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_ePcm;
        NxClient_SetAudioSettings(&audioSettings);
    }

    nexus_tunnel_sink_stop(bout);

    if (bout->nexus.tunnel.pid_channel) {
        ALOG_ASSERT(bout->nexus.tunnel.playpump != NULL);
        NEXUS_Playpump_ClosePidChannel(bout->nexus.tunnel.playpump, bout->nexus.tunnel.pid_channel);
        bout->nexus.tunnel.pid_channel = NULL;
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
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    if (bout->tunnel_base.pes_debug) {
        fclose(bout->tunnel_base.pes_debug);
        bout->tunnel_base.pes_debug = NULL;
    }

    return 0;
}

static char *nexus_tunnel_sink_get_parameters (struct brcm_stream_out *bout, const char *keys)
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
                          (int)(intptr_t)bout->tunnel_base.stc_channel);
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
        if (formats_str.contains("AUDIO_FORMAT_AC3") && !formats_str.contains("AUDIO_FORMAT_E_AC3")) {
            NEXUS_AudioCapabilities audioCaps;
            NEXUS_GetAudioCapabilities(&audioCaps);
            if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode) {
                formats_str.append("|AUDIO_FORMAT_E_AC3");
            }
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          formats_str.isEmpty() ? TOSTRING(NEXUS_OUT_DEFAULT_FORMAT) : formats_str.string());
    }
    pthread_mutex_unlock(&bout->lock);

    result_str = str_parms_to_str(result);
    str_parms_destroy(query);
    str_parms_destroy(result);

    BA_LOG(TUN_DBG, "%s: result = %s", __FUNCTION__, result_str);

    return result_str;
}

static int nexus_tunnel_sink_dump(struct brcm_stream_out *bout, int fd)
{
   dprintf(fd, "\ntunnel_base_bout_dump:\n"
            "\tstc-channel = %p\n"
            "\tpid-channel = %p\n"
            "\tplaypump    = %p\n"
            "\tdecoder     = %p\n",
            (void *)bout->tunnel_base.stc_channel,
            (void *)bout->nexus.tunnel.pid_channel,
            (void *)bout->nexus.tunnel.playpump,
            (void *)bout->nexus.tunnel.audio_decoder);

   return 0;
}

static bool nexus_tunnel_sink_is_ready(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;

    return !(bout->suspended || !audio_decoder || !playpump);
}

static int nexus_tunnel_sink_pace(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;
    BKNI_EventHandle event = bout->nexus.event;
    NEXUS_AudioDecoderTrickState trickState;
    NEXUS_Error ret = 0;

    if (bout->tunnel_base.started) {
        if (bout->nexus.tunnel.pcm_format) {
            NEXUS_SimpleAudioDecoder_GetTrickState(audio_decoder, &trickState);
            if (!bout->nexus.tunnel.priming && trickState.rate != 0) {
                int32_t fifoDepth;
                int32_t threshold = get_brcm_audio_buffer_size(bout->config.sample_rate,
                                                               bout->config.format,
                                                               popcount(bout->config.channel_mask),
                                                               PCM_STOP_FILL_TARGET);

                fifoDepth = nexus_tunnel_sink_get_fifo_depth(bout);
                if (fifoDepth > threshold) {
                    /* Enough data accumulated in playpump and decoder */
                    BKNI_ResetEvent(event);
                    pthread_mutex_unlock(&bout->lock);
                    ret = BKNI_WaitForEvent(event, 5);
                    pthread_mutex_lock(&bout->lock);
                    // Suspend check when relocking
                    if (bout->suspended) {
                        ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                        return -ENOSYS;
                    }
                    if (ret == BERR_TIMEOUT) {
                        return -ETIMEDOUT;
                    } else {
                        /* Check again */
                        return -EAGAIN;
                    }
                }
            }
            else if (bout->nexus.tunnel.priming) {
                nexus_tunnel_sink_resume_int(bout);
            }
        } else {
            NEXUS_SimpleAudioDecoder_GetTrickState(audio_decoder, &trickState);
            if (!bout->nexus.tunnel.priming && trickState.rate != 0) {
                /* Limit from accumulating too much data */
                if (bout->nexus.tunnel.bitrate > 0) {
                    unsigned bitrate;
                    int32_t fifoDepth;
                    fifoDepth = nexus_tunnel_sink_get_fifo_depth(bout);
                    // Not supposed to happen but if it does, we just exit the function
                    // quietly and wait for the next run to try again.
                    if (fifoDepth < 0) {
                        return -ETIMEDOUT;
                    }
                    if (bout->nexus.tunnel.bitrate > 0 &&
                        (uint32_t)fifoDepth >= KBITRATE_TO_BYTES_PER_250MS(bout->nexus.tunnel.bitrate)) {
                        /* Enough data accumulated in playpump and decoder */
                        BKNI_ResetEvent(event);
                        pthread_mutex_unlock(&bout->lock);
                        ret = BKNI_WaitForEvent(event, 5);
                        pthread_mutex_lock(&bout->lock);
                        // Suspend check when relocking
                        if (bout->suspended) {
                            ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                            return -ENOSYS;
                        }
                        if (ret == BERR_TIMEOUT) {
                            return -ETIMEDOUT;
                        }
                        /* Check again */
                        return -EAGAIN;
                    }
                }
            }
            else if (bout->nexus.tunnel.priming) {
                nexus_tunnel_sink_resume_int(bout);
            }
        }
    }

    return 0;
}

static int nexus_tunnel_sink_set_start_pts(struct brcm_stream_out *bout, unsigned pts)
{
    // start sink once first frame is written
    int ret = nexus_tunnel_sink_start_int(bout);
    if (ret != 0)
        return ret;
    // init stc with the first audio timestamp
    NEXUS_SimpleStcChannel_SetStc(bout->tunnel_base.stc_channel_sync, pts);
    BA_LOG(TUN_WRITE, "%s: stc-sync initialized to:%u", __FUNCTION__, pts);
    return 0;
}

static int nexus_tunnel_sink_write(struct brcm_stream_out *bout, const void* buffer, size_t frame_bytes,
                 struct brcm_frame_header *frame_header)
{
    bool write_header = frame_header != NULL;
    void *nexus_buffer;
    size_t nexus_space;
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.tunnel.playpump;
    BKNI_EventHandle event = bout->nexus.event;
    NEXUS_PlaypumpStatus ppStatus;
    size_t bytes = frame_bytes;
    int ret = 0;

    if (write_header) {
        // attempt to parse ac3/eac3 content knowing that the data starts with the beginning of a frame
        nexus_tunnel_sink_parse_ac3(bout, buffer, frame_bytes);
    }

    while ( frame_bytes > 0 )
    {
        bmedia_pes_info pes;
        size_t used, waveformat_size;
        size_t space_required, length;

        ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
        if (ret) {
            ALOGE("%s: get playpump buffer failed, ret=%d", __FUNCTION__, ret);
            ret = -ENOSYS;
            break;
        }

        space_required = frame_bytes;
        if ( write_header ) {
            space_required += BMEDIA_PES_HEADER_MAX_SIZE;
            if (bout->nexus.tunnel.pcm_format) {
                space_required += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
            }
        }
        if (nexus_space >= space_required) {
            used = 0;
            if ( write_header ) {
                // Format PES
                BMEDIA_PES_INFO_INIT(&pes, BRCM_AUDIO_STREAM_ID);
                BMEDIA_PES_SET_PTS(&pes, frame_header->pts);
                length = frame_header->frame_length;
                if (bout->nexus.tunnel.pcm_format) {
                    length += BMEDIA_WAVEFORMATEX_BASE_SIZE + bmedia_frame_bcma.len + 4;
                }
                used += bmedia_pes_header_init((uint8_t *)nexus_buffer, length, &pes);
                if (bout->nexus.tunnel.pcm_format) {
                    BKNI_Memcpy((uint8_t *)nexus_buffer + used, bmedia_frame_bcma.base, bmedia_frame_bcma.len);
                    used += bmedia_frame_bcma.len;
                    B_MEDIA_SAVE_UINT32_BE((uint8_t *)nexus_buffer + used, frame_header->frame_length);
                    used += sizeof(uint32_t);

                    // Format WAV header
                    waveformat_size = bmedia_write_waveformatex((uint8_t *)nexus_buffer + used, &bout->nexus.tunnel.wave_fmt);
                    ALOG_ASSERT(waveformat_size == BMEDIA_WAVEFORMATEX_BASE_SIZE);
                    used += waveformat_size;
                }
            }

            BKNI_Memcpy((uint8_t *)nexus_buffer + used, buffer, frame_bytes);
            used += frame_bytes;
            BDBG_ASSERT(used <= nexus_space);

            if (bout->tunnel_base.pes_debug != NULL) {
                fwrite(nexus_buffer, 1, used, bout->tunnel_base.pes_debug);
            }
            ret = NEXUS_Playpump_WriteComplete(playpump, 0, used);
            if (ret) {
                ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                break;
            }
            else {
                // data written, exit the function
                frame_bytes = 0;
                ret = 0;
            }
        }
        else if ((uint8_t *)nexus_buffer + nexus_space >= bout->nexus.tunnel.pp_buffer_end) {
            if (nexus_space > 0) {
                // Look for a complete contiguous buffer
                ret = NEXUS_Playpump_WriteComplete(playpump, nexus_space, 0);
                if (ret) {
                    ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                    break;
                }
            }
        }
        else {
            NEXUS_SimpleAudioDecoderHandle prev_audio_decoder = audio_decoder;
            NEXUS_PlaypumpHandle prev_playpump = playpump;

            if (nexus_space > 0) {
                // Look for a complete contiguous buffer
                ret = NEXUS_Playpump_WriteComplete(playpump, nexus_space, 0);
                if (ret) {
                    ALOGE("%s:%d playpump write failed, ret=%d", __FUNCTION__, __LINE__, ret);
                    break;
                }
            }

            BKNI_ResetEvent(event);
            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 500);
            pthread_mutex_lock(&bout->lock);
            // Suspend check when relocking
            if (bout->suspended) {
                ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                return -ENOSYS;
            }
            // Sanity check when relocking
            audio_decoder = bout->nexus.tunnel.audio_decoder;
            playpump = bout->nexus.tunnel.playpump;
            ALOG_ASSERT(audio_decoder == prev_audio_decoder);
            ALOG_ASSERT(playpump == prev_playpump);

            if (ret != BERR_SUCCESS) {
                NEXUS_Playpump_GetStatus(playpump, &ppStatus);
                ALOGE("%s: playpump write timeout, ret=%d, ns:%zu, fifoS:%zu fifoD:%zu",
                        __FUNCTION__, ret, nexus_space, ppStatus.fifoSize, ppStatus.fifoDepth);
                ret = -ENOSYS;
                break;
            }
        }
    }

    if (ret == 0)
        return bytes;
    return ret;
}

static void nexus_tunnel_sink_throttle(struct brcm_stream_out *bout)
{
    // For PCM, throttle the output to prevent audio underruns
    if (bout->nexus.tunnel.pcm_format) {
        nsecs_t delta = systemTime(SYSTEM_TIME_MONOTONIC) - bout->nexus.tunnel.last_write_time;
        int32_t throttle_us = BRCM_AUDIO_TUNNEL_HALF_DURATION_US - (delta / 1000);
        if (throttle_us <= BRCM_AUDIO_TUNNEL_HALF_DURATION_US && throttle_us > 0) {
            BA_LOG(TUN_WRITE, "%s: pcm throttle %d us", __FUNCTION__, throttle_us);
            usleep(throttle_us);
        }
        bout->nexus.tunnel.last_write_time = systemTime(SYSTEM_TIME_MONOTONIC);
    }
}


// returns true if nexus audio can enter standby
static bool nexus_tunnel_sink_standby_monitor(void *context)
{
    bool standby = true;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;

    if (bout != NULL) {
        pthread_mutex_lock(&bout->lock);
        standby = (bout->started == false);
        pthread_mutex_unlock(&bout->lock);
    }
    BA_LOG(TUN_DBG, "%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

struct brcm_stream_sink_ops nexus_tunnel_sink_ops = {
    .open = nexus_tunnel_sink_open,
    .close = nexus_tunnel_sink_close,
    .get_latency = nexus_tunnel_sink_get_latency,
    .start = nexus_tunnel_sink_start,
    .stop = nexus_tunnel_sink_stop,
    .pause = nexus_tunnel_sink_pause,
    .resume = nexus_tunnel_sink_resume,
    .drain = nexus_tunnel_sink_drain,
    .flush = nexus_tunnel_sink_flush,
    .set_volume = nexus_tunnel_sink_set_volume,
    .get_render_position = nexus_tunnel_sink_get_render_position,
    .get_presentation_position = nexus_tunnel_sink_get_presentation_position,
    .dump = nexus_tunnel_sink_dump,
    .get_parameters = nexus_tunnel_sink_get_parameters,
    .set_parameters = NULL,
    .is_ready = nexus_tunnel_sink_is_ready,
    .pace = nexus_tunnel_sink_pace,
    .set_start_pts = nexus_tunnel_sink_set_start_pts,
    .write = nexus_tunnel_sink_write,
    .throttle = nexus_tunnel_sink_throttle
};


// Utility functions
static void nexus_tunnel_sink_parse_ac3(struct brcm_stream_out *bout, const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle audio_decoder = bout->nexus.tunnel.audio_decoder;

    // For E-AC3, DTS, and DTS-HD, parse the frame header to determine the number of PCM samples per frame
    if (bout->nexus.tunnel.audioblocks_per_frame == 0) {
        if (bout->config.format == AUDIO_FORMAT_AC3 || bout->config.format == AUDIO_FORMAT_E_AC3) {
            const uint8_t *syncframe = (const uint8_t *)memmem(buffer, bytes, g_nexus_parse_eac3_syncword,
                                                                sizeof(g_nexus_parse_eac3_syncword));
            if (syncframe != NULL) {
                eac3_frame_hdr_info info;
                if (nexus_parse_eac3_frame_hdr(syncframe, bytes - (syncframe - (const uint8_t *)buffer), &info)) {
                    if (bout->config.format == AUDIO_FORMAT_E_AC3) {
                        bout->nexus.tunnel.audioblocks_per_frame = info.num_audio_blks;
                        bout->nexus.tunnel.frame_multiplier = nexus_tunnel_sink_get_frame_multipler(bout);
                        bout->nexus.tunnel.bitrate = info.bitrate;
                    }
                    else { // AC3
                        bout->nexus.tunnel.audioblocks_per_frame = NEXUS_PCM_FRAMES_PER_AC3_FRAME / NEXUS_PCM_FRAMES_PER_AC3_AUDIO_BLOCK;
                        bout->nexus.tunnel.frame_multiplier = NEXUS_PCM_FRAMES_PER_AC3_FRAME;
                        bout->nexus.tunnel.bitrate = info.bitrate;
                    }
                }
                else {
                    BA_LOG(TUN_WRITE, "%s: Error parsing E-/AC3 sync frame", __FUNCTION__);
                }
            }
        }
        else if (bout->config.format == AUDIO_FORMAT_DTS || bout->config.format == AUDIO_FORMAT_DTS_HD) {
            NEXUS_AudioDecoderStatus status;
            if (NEXUS_SimpleAudioDecoder_GetStatus(audio_decoder, &status) == NEXUS_SUCCESS && status.codecStatus.dts.numPcmBlocks >= 5 && status.codecStatus.dts.numPcmBlocks <= 127) {
                bout->nexus.tunnel.audioblocks_per_frame = status.codecStatus.dts.numPcmBlocks;
                bout->nexus.tunnel.frame_multiplier = nexus_tunnel_sink_get_frame_multipler(bout);
            }
        }

        ALOGI_IF(bout->nexus.tunnel.audioblocks_per_frame > 0 && BA_LOG_ON(TUN_WRITE),
            "%s: %u blocks detected, fmt:%x mpy:%u bkps:%u",
            __FUNCTION__, bout->nexus.tunnel.audioblocks_per_frame, bout->config.format,
            bout->nexus.tunnel.frame_multiplier, bout->nexus.tunnel.bitrate);
    }
}




NEXUS_Error nexus_tunnel_alloc_stc_mem_hdl(NEXUS_MemoryBlockHandle *hdl)
{
    NEXUS_Error nx_rc;
    NEXUS_ClientConfiguration client_config;
    NEXUS_HeapHandle global_heap;
    NEXUS_MemoryBlockHandle mem_block_hdl = NULL;
    void *memory_ptr;
    stc_channel_st *channel_st_ptr;
    NEXUS_SimpleStcChannelSettings stc_channel_settings;

    NEXUS_Platform_GetClientConfiguration(&client_config);
    global_heap = client_config.heap[NXCLIENT_FULL_HEAP];
    if (global_heap == NULL) {
        global_heap = client_config.heap[NXCLIENT_DEFAULT_HEAP];
        if (global_heap == NULL) {
           ALOGE("%s: error accessing main heap", __FUNCTION__);
           return NEXUS_OUT_OF_DEVICE_MEMORY;
        }
    }
    mem_block_hdl = NEXUS_MemoryBlock_Allocate(global_heap, sizeof(stc_channel_st), 16, NULL);
    if (mem_block_hdl == NULL) {
        ALOGE("%s: unable to allocate memory block", __FUNCTION__);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    memory_ptr = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(mem_block_hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGE("%s: unable to lock global memory block", __FUNCTION__);
        if (!nx_rc)
            nx_rc = NEXUS_OUT_OF_DEVICE_MEMORY;
        NEXUS_MemoryBlock_Free(mem_block_hdl);
        return nx_rc;
    }

    channel_st_ptr = (stc_channel_st*)memory_ptr;
    memset(channel_st_ptr, 0, sizeof(*channel_st_ptr));

    // create stc channels
    NEXUS_SimpleStcChannel_GetDefaultSettings(&stc_channel_settings);
    stc_channel_settings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eAudioMaster;
    stc_channel_settings.sync = NEXUS_SimpleStcChannelSyncMode_eAudioAdjustmentConcealment;
    channel_st_ptr->stc_channel = NEXUS_SimpleStcChannel_Create(&stc_channel_settings);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel, true);

    NEXUS_SimpleStcChannel_GetDefaultSettings(&stc_channel_settings);
    stc_channel_settings.mode = NEXUS_StcChannelMode_eHost;
    channel_st_ptr->stc_channel_sync = NEXUS_SimpleStcChannel_Create(&stc_channel_settings);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel_sync, true);
    NEXUS_SimpleStcChannel_SetStc(channel_st_ptr->stc_channel_sync, BRCM_AUDIO_TUNNEL_STC_SYNC_INVALID);
    BA_LOG(TUN_DBG, "%s: stc-channels: [%p %p]", __FUNCTION__,
            channel_st_ptr->stc_channel, channel_st_ptr->stc_channel_sync);

    *hdl = mem_block_hdl;
    NEXUS_MemoryBlock_Unlock(mem_block_hdl);
    return NEXUS_SUCCESS;
}

void nexus_tunnel_release_stc_mem_hdl(NEXUS_MemoryBlockHandle *hdl)
{
    void *memory_ptr;
    NEXUS_Error nx_rc;
    stc_channel_st *channel_st_ptr;

    if (*hdl == NULL)
        return;

    memory_ptr = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(*hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGW("%s: unable to lock handle", __FUNCTION__);
        NEXUS_MemoryBlock_Free(*hdl);
        *hdl = NULL;
        return;
    }
    channel_st_ptr = (stc_channel_st*)memory_ptr;
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel, false);
    NEXUS_SimpleStcChannel_Destroy(channel_st_ptr->stc_channel);
    NEXUS_Platform_SetSharedHandle(channel_st_ptr->stc_channel_sync, false);
    NEXUS_SimpleStcChannel_Destroy(channel_st_ptr->stc_channel_sync);

    NEXUS_MemoryBlock_Unlock(*hdl);
    NEXUS_MemoryBlock_Free(*hdl);
    *hdl = NULL;
}

void nexus_tunnel_lock_stc_mem_hdl(NEXUS_MemoryBlockHandle hdl, stc_channel_st **stc_st)
{
    void *memory_ptr;
    NEXUS_Error nx_rc;
    stc_channel_st *channel_st_ptr;

    if (hdl == NULL)
        return;

    memory_ptr = NULL;
    *stc_st = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(hdl, &memory_ptr);
    if (nx_rc || memory_ptr == NULL) {
        ALOGW("%s: unable to lock handle", __FUNCTION__);
        return;
    }
    channel_st_ptr = (stc_channel_st*)memory_ptr;
    *stc_st = channel_st_ptr;
}

void nexus_tunnel_unlock_stc_mem_hdl(NEXUS_MemoryBlockHandle hdl)
{
    NEXUS_MemoryBlock_Unlock(hdl);
}
