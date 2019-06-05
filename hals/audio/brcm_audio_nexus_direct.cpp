/******************************************************************************
 *    (c)2015-2018 Broadcom Corporation
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
#include <inttypes.h>
#include "vendor_bcm_props.h"

using namespace android;

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   48000
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

#define NEXUS_OUT_BUFFER_DURATION_MS    10

#define BRCM_AUDIO_STREAM_ID                    (0xC0)
#define BRCM_AUDIO_DIRECT_COMP_BUFFER_SIZE      (2048)
#define BRCM_AUDIO_DIRECT_PLAYPUMP_FIFO_SIZE    (65536)
#define BRCM_AUDIO_DIRECT_DECODER_FIFO_SIZE     (32768)
#define BRCM_AUDIO_DIRECT_EAC3_TRANS_LATENCY    (128)
#define BRCM_AUDIO_DIRECT_DEFAULT_LATENCY       (10)    // ms

#define BITRATE_TO_BYTES_PER_250_MS(bitrate)    (bitrate * 1024/8/4)

// Use a small negative seed position to stress rollover
#define SEED_POSITION -1000000

typedef struct {
    const char *key;
    int value;
} property_str_map;

static const property_str_map drc_mode_map[] = {
    {
        .key = "line",
        .value = NEXUS_AudioDecoderDolbyDrcMode_eLine,
    },
    {
        .key = "rf",
        .value = NEXUS_AudioDecoderDolbyDrcMode_eRf,
    },
    {
        .key = "customA",
        .value = NEXUS_AudioDecoderDolbyDrcMode_eCustomA,
    },
    {
        .key = "customD",
        .value = NEXUS_AudioDecoderDolbyDrcMode_eCustomD,
    },
    {
        .key = "off",
        .value = NEXUS_AudioDecoderDolbyDrcMode_eOff,
    },
    {
        .key = NULL,
        .value = -1,
    }
};

static const property_str_map stereo_downmix_mode_map[] = {
    {
        .key = "auto",
        .value = NEXUS_AudioDecoderDolbyStereoDownmixMode_eAutomatic,
    },
    {
        .key = "LtRt",
        .value = NEXUS_AudioDecoderDolbyStereoDownmixMode_eDolbySurroundCompatible,
    },
    {
        .key = "LoRo",
        .value = NEXUS_AudioDecoderDolbyStereoDownmixMode_eStandard,
    },
    {
        .key = NULL,
        .value = -1,
    }
};

/*
 * Utility Functions
 */
static int get_property_value(const char *str_value, const property_str_map *map)
{
    if (!map)
        return -1;

    while (map->key != NULL) {
        if (strncmp(str_value, map->key, strlen(map->key)) == 0)
            return map->value;
        map++;
    }
    return -1;
}

static void nexus_direct_bout_data_callback(void *context, int param)
{
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;
    ALOG_ASSERT(bout->nexus.event == (BKNI_EventHandle)(intptr_t)param);
    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param);
}

/*
 * Operation Functions
 */

static int nexus_direct_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;

    if (left != right) {
        BA_LOG(DIR_DBG, "%s: Left and Right volumes must be equal, cannot change volume", __FUNCTION__);
        return 0;
    }
    if (left > 1.0) {
        BA_LOG(DIR_DBG, "%s: Volume: %f exceeds max volume of 1.0", __FUNCTION__, left);
        return 0;
    }


    if (bout->nexus.direct.playpump_mode) {
        if (bout->nexus.direct.fadeLevel != (unsigned)(left * 100)) {
            bout->nexus.direct.fadeLevel = (unsigned)(left * 100);

            BA_LOG(DIR_DBG, "%s: Volume: %d", __FUNCTION__, bout->nexus.direct.fadeLevel);
            if (bout->started) {
                if (bout->nexus.direct.deferred_volume) {
                    struct timespec now;

                    clock_gettime(CLOCK_MONOTONIC, &now);
                    if (nexus_common_is_paused(simple_decoder)) {
                        // Record deferral amount if volume is set during priming
                        int32_t deferred_ms;
                        deferred_ms = (int32_t)((now.tv_sec - bout->nexus.direct.start_ts.tv_sec) * 1000) +
                                      (int32_t)((now.tv_nsec - bout->nexus.direct.start_ts.tv_nsec)/1000000);
                        bout->nexus.direct.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;
                    } else {
                        if (timespec_greater_than(now, bout->nexus.direct.deferred_window)) {
                            // Deferral window over, resume normal operation
                            bout->nexus.direct.deferred_volume = false;
                            nexus_common_set_volume(bout->bdev, simple_decoder, bout->nexus.direct.fadeLevel, NULL, -1, -1);
                        } else {
                            // Push deferral window based on current time
                            bout->nexus.direct.deferred_window = now;
                            timespec_add_ms(bout->nexus.direct.deferred_window, MAX_VOLUME_DEFERRAL);

                            nexus_common_set_volume(bout->bdev, simple_decoder, bout->nexus.direct.fadeLevel, NULL,
                                bout->nexus.direct.deferred_volume_ms, -1);
                        }
                    }
                } else if (!nexus_common_is_paused(simple_decoder)) {
                    // Apply volume immediately if not paused
                    nexus_common_set_volume(bout->bdev, simple_decoder, bout->nexus.direct.fadeLevel, NULL, -1, -1);
                }
            }
        }
    }

    return 0;
}

static uint32_t nexus_direct_bout_get_latency(struct brcm_stream_out *bout)
{
    (void)bout;

    // Estimated experimental value
    if (bout->nexus.direct.playpump_mode) {
        return bout->nexus.direct.transcode_latency;
    }

    return BRCM_AUDIO_DIRECT_DEFAULT_LATENCY;
}

static unsigned nexus_direct_bout_get_frame_multipler(struct brcm_stream_out *bout)
{
    unsigned res = 1;

    switch (bout->config.format) {
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_E_AC3:
        {
            res = NEXUS_PCM_FRAMES_PER_AC3_FRAME;
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

static int nexus_direct_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (simple_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get render position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (bout->nexus.direct.playpump_mode) {
            bout->framesPlayed += status.framesDecoded - bout->nexus.direct.lastCount;
            bout->nexus.direct.lastCount = status.framesDecoded;
            if (bout->latencyPad) {
                if (((bout->framesPlayedTotal + bout->framesPlayed) * bout->nexus.direct.frame_multiplier) >
                        bout->latencyPad)
                    bout->latencyPad = 0;
            }
            if (bout->latencyPad)
                *dsp_frames = 0;
            else
                *dsp_frames = (uint32_t)(bout->framesPlayed * bout->nexus.direct.frame_multiplier) -
                                  bout->latencyEstimate;
        } else {
            /* numBytesDecoded for passthrough mode returns the underlying playback playedBytes, which is of type size_t */
            bout->framesPlayed += ((size_t)status.numBytesDecoded - bout->nexus.direct.lastCount) / bout->frameSize;
            bout->nexus.direct.lastCount = status.numBytesDecoded;
            if (bout->latencyPad) {
                if ((bout->framesPlayedTotal + bout->framesPlayed) > bout->latencyPad)
                    bout->latencyPad = 0;
            }
            if (bout->latencyPad)
                *dsp_frames = 0;
            else
                *dsp_frames = (uint32_t)bout->framesPlayed - bout->latencyEstimate;
        }
    } else {
       *dsp_frames = 0;
    }

    BA_LOG(DIR_POS, "Render pos:%u", *dsp_frames);
    return 0;
}


static int nexus_direct_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_AudioDecoderStatus status;
    NEXUS_Error ret;

    if (simple_decoder) {
        ret = NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
        if (ret != NEXUS_SUCCESS) {
            ALOGE("%s: Get presentation position failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        if (bout->nexus.direct.playpump_mode) {
            bout->framesPlayed += status.framesDecoded - bout->nexus.direct.lastCount;
            bout->nexus.direct.lastCount = status.framesDecoded;
            if (bout->latencyPad) {
                if (((bout->framesPlayedTotal + bout->framesPlayed) * bout->nexus.direct.frame_multiplier) >
                        bout->latencyPad)
                    bout->latencyPad = 0;
            }
            if (bout->latencyPad)
                *frames = 0;
            else
                *frames = (bout->framesPlayedTotal + bout->framesPlayed) * bout->nexus.direct.frame_multiplier -
                              bout->latencyEstimate;
        } else {
            /* numBytesDecoded for passthrough mode returns the underlying playback playedBytes, which is of type size_t */
            bout->framesPlayed += ((size_t)status.numBytesDecoded - bout->nexus.direct.lastCount) / bout->frameSize;
            bout->nexus.direct.lastCount = status.numBytesDecoded;
            if (bout->latencyPad) {
                if ((bout->framesPlayedTotal + bout->framesPlayed) > bout->latencyPad)
                    bout->latencyPad = 0;
            }
            if (bout->latencyPad)
                *frames = 0;
            else
                *frames = bout->framesPlayedTotal + bout->framesPlayed - bout->latencyEstimate;
        }
    } else {
       *frames =0;
    }

    BA_LOG(DIR_POS, "Presentation pos:%" PRIu64 "", *frames);
    return 0;
}


static char *nexus_direct_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    String8 rates_str, channels_str, formats_str;
    char* result_str;

    pthread_mutex_lock(&bout->lock);
    /* Get real parameters here!!! */
    nexus_get_hdmi_parameters(rates_str, channels_str, formats_str);

    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_str.isEmpty()?"48000":rates_str.string());
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_str.isEmpty()?"AUDIO_CHANNEL_OUT_STEREO":channels_str.string());
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        if (formats_str.contains("AUDIO_FORMAT_AC3") && !formats_str.contains("AUDIO_FORMAT_E_AC3")) {
            NEXUS_AudioCapabilities audioCaps;
            NEXUS_GetAudioCapabilities(&audioCaps);
            if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode) {
                formats_str.append("|AUDIO_FORMAT_E_AC3");
            }
        }
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          formats_str.isEmpty()?"AUDIO_FORMAT_PCM_16_BIT":formats_str.string());
    }
    pthread_mutex_unlock(&bout->lock);

    result_str = str_parms_to_str(result);
    str_parms_destroy(query);
    str_parms_destroy(result);

    BA_LOG(DIR_DBG, "%s: result = %s", __FUNCTION__, result_str);

    return result_str;
}

static int nexus_direct_bout_start(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;
    NEXUS_SimpleAudioDecoderStartSettings start_settings;
    BKNI_EventHandle event = bout->nexus.event;
    struct audio_config *config = &bout->config;
    int ret = 0;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: at %d, device not open",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    BA_LOG(DIR_STATE, "%s, %p", __FUNCTION__, bout);

    clock_gettime(CLOCK_MONOTONIC, &bout->nexus.direct.start_ts);

    if (bout->nexus.direct.playpump_mode) {
        NEXUS_SimpleAudioDecoderSettings settings;

        // Start play pump
        ret = NEXUS_Playpump_Start(playpump);
        if (ret) {
            ALOGE("%s: Start playpump failed, ret = %d", __FUNCTION__, ret);
            return -ENOSYS;
        }

        // Configure audio decoder fifo threshold and pid channel
        NEXUS_SimpleAudioDecoder_GetSettings(simple_decoder, &settings);
        ALOGI("Primary fifoThreshold %d", settings.primary.fifoThreshold);
        settings.primary.fifoThreshold = BRCM_AUDIO_DIRECT_DECODER_FIFO_SIZE;
        ALOGI("Primary fifoThreshold set to %d", settings.primary.fifoThreshold);

        NEXUS_SimpleAudioDecoder_SetSettings(simple_decoder, &settings);

        NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);
        start_settings.primary.codec = brcm_audio_get_codec_from_format(bout->config.format);
        start_settings.primary.pidChannel = bout->nexus.direct.pid_channel;

        if (bout->bdev->dolby_ms == 12) {
            start_settings.primary.mixingMode = NEXUS_AudioDecoderMixingMode_eStandalone;
        }

        // Set Dolby DRC and downmix modes
        if ((start_settings.primary.codec == NEXUS_AudioCodec_eAc3) ||
            (start_settings.primary.codec == NEXUS_AudioCodec_eAc3Plus)) {
            NEXUS_AudioDecoderCodecSettings codecSettings;
            NEXUS_AudioDecoderDolbySettings *dolbySettings;
            char property[PROPERTY_VALUE_MAX];
            int value;
            bool changed = false;

            // Get default settings
            NEXUS_SimpleAudioDecoder_GetCodecSettings(simple_decoder,
                    NEXUS_SimpleAudioDecoderSelector_ePrimary, start_settings.primary.codec, &codecSettings);
            if (codecSettings.codec != start_settings.primary.codec) {
                ALOGE("%s: Codec mismatch %d != %d", __FUNCTION__,
                        codecSettings.codec, start_settings.primary.codec);
                NEXUS_Playpump_Stop(playpump);
                return -ENOSYS;
            }

            // Override settings
            dolbySettings = (codecSettings.codec == NEXUS_AudioCodec_eAc3)?
                                &codecSettings.codecSettings.ac3:
                                &codecSettings.codecSettings.ac3Plus;

            if (bout->bdev->dolby_ms == 12) {
                dolbySettings->enableAtmosProcessing = true;

                if (property_get_bool(BCM_RO_AUDIO_DISABLE_ATMOS, false) ||
                    property_get_bool(BCM_PERSIST_AUDIO_DISABLE_ATMOS, false)) {
                    dolbySettings->enableAtmosProcessing = false;
                }

                ALOGI("%s: %s Dolby ATMOS", __FUNCTION__,
                    dolbySettings->enableAtmosProcessing?"Enabling":"Disabling");
                changed = true;
            }

            if (property_get(BCM_RO_AUDIO_DIRECT_DOLBY_DRC_MODE, property, "")) {
                value = get_property_value(property, drc_mode_map);
                if (value != -1) {
                    ALOGI("%s: Setting Dolby DRC mode to %d", __FUNCTION__, value);
                    dolbySettings->drcMode = (NEXUS_AudioDecoderDolbyDrcMode)value;
                    dolbySettings->drcModeDownmix = (NEXUS_AudioDecoderDolbyDrcMode)value;
                    changed = true;
                }
            }
            if (property_get(BCM_RO_AUDIO_DIRECT_DOLBY_STEREO_DOWNMIX_MODE, property, "")) {
                value = get_property_value(property, stereo_downmix_mode_map);
                if (value != -1) {
                    ALOGI("%s: Setting Dolby stereo downmix mode to %d", __FUNCTION__, value);
                    dolbySettings->stereoDownmixMode = (NEXUS_AudioDecoderDolbyStereoDownmixMode)value;
                    changed = true;
                }
            }

            // Apply settings
            if (changed) {
                ret = NEXUS_SimpleAudioDecoder_SetCodecSettings(simple_decoder,
                        NEXUS_SimpleAudioDecoderSelector_ePrimary, &codecSettings);
                if (ret) {
                    ALOGE("%s: Unable to set codec %d settings, ret = %d", __FUNCTION__,
                            start_settings.primary.codec, ret);
                    NEXUS_Playpump_Stop(playpump);
                    return -ENOSYS;
                }
            }
        }
    } else {
        NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);

        start_settings.passthroughBuffer.enabled = true;
        start_settings.passthroughBuffer.sampleRate = bout->config.sample_rate;
        NEXUS_CallbackDesc_Init(&start_settings.passthroughBuffer.dataCallback);
        start_settings.passthroughBuffer.dataCallback.callback = nexus_direct_bout_data_callback;
        start_settings.passthroughBuffer.dataCallback.context = bout;
        start_settings.passthroughBuffer.dataCallback.param = (int)(intptr_t)event;
    }

    // Start decoder
    ret = NEXUS_SimpleAudioDecoder_Start(simple_decoder, &start_settings);
    if (ret) {
        ALOGE("%s: at %d, start decoder failed, ret = %d",
             __FUNCTION__, __LINE__, ret);
        NEXUS_Playpump_Stop(playpump);
        return -ENOSYS;
    }

    if (bout->nexus.direct.playpump_mode) {
        NEXUS_Error res;

        BA_LOG(DIR_DBG, "%s: pausing to prime decoder", __FUNCTION__);
        res = nexus_common_mute_and_pause(bout->bdev, bout->nexus.direct.simple_decoder, NULL, 0, 0);

        if (res == NEXUS_SUCCESS) {
            bout->nexus.direct.priming = true;
        } else {
            bout->nexus.direct.priming = false;
            ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, res);
        }
    }

    bout->nexus.direct.lastCount = 0;
    bout->nexus.direct.bitrate = 0;
    bout->nexus.direct.current_pos = SEED_POSITION;
    bout->nexus.direct.last_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.next_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.last_info.valid = false;
    bout->nexus.direct.frame_multiplier = nexus_direct_bout_get_frame_multipler(bout);
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_direct_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;

    ALOGV_FIFO_INFO(simple_decoder, playpump);

    if (simple_decoder) {
        NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
        NEXUS_AudioDecoderStatus status;
        NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
        if (bout->nexus.direct.playpump_mode) {
            bout->framesPlayed += status.framesDecoded - bout->nexus.direct.lastCount;
            bout->nexus.direct.lastCount = status.framesDecoded;
        } else {
            /* numBytesDecoded for passthrough mode returns the underlying playback playedBytes, which is of type size_t */
            bout->framesPlayed += ((size_t)status.numBytesDecoded - bout->nexus.direct.lastCount) / bout->frameSize;
            bout->nexus.direct.lastCount = status.numBytesDecoded;
        }
        bout->framesPlayedTotal += bout->framesPlayed;
        BA_LOG(DIR_DBG, "%s: setting framesPlayedTotal to %" PRIu64 "", __FUNCTION__, bout->framesPlayedTotal);
    }

    if (bout->nexus.direct.playpump_mode && playpump) {
        NEXUS_Playpump_Stop(playpump);
    }

    bout->nexus.direct.lastCount = 0;
    bout->nexus.direct.bitrate = 0;
    bout->nexus.direct.current_pos = SEED_POSITION;
    bout->nexus.direct.last_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.next_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.last_info.valid = false;
    bout->nexus.direct.frame_multiplier = nexus_direct_bout_get_frame_multipler(bout);
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_direct_bout_pause(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: at %d, device not open",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    BA_LOG(DIR_STATE, "%s, %p", __FUNCTION__, bout);
    if (bout->nexus.direct.playpump_mode && playpump) {
        NEXUS_Error rc;

        if (!bout->nexus.direct.priming) {
            // Stop volume deferral as soon as paused
            bout->nexus.direct.deferred_volume = false;

            ALOGV_FIFO_INFO(simple_decoder, playpump);
            rc = nexus_common_mute_and_pause(bout->bdev, bout->nexus.direct.simple_decoder, NULL,
                                             bout->nexus.direct.soft_muting,
                                             bout->nexus.direct.sleep_after_mute);

            if (rc != NEXUS_SUCCESS) {
                ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, rc);
            }
            ALOGV_FIFO_INFO(simple_decoder, playpump);
        } else {
            BA_LOG(DIR_DBG, "%s, %p stop priming", __FUNCTION__, bout);
            bout->nexus.direct.priming = false;
        }
        bout->nexus.direct.paused = true;
    }

    return 0;
}

static int nexus_direct_bout_resume(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: at %d, device not open",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    BA_LOG(DIR_STATE, "%s, %p", __FUNCTION__, bout);

    if (bout->nexus.direct.playpump_mode && playpump) {
        bout->nexus.direct.paused = false;
        /* Prime again only if not yet resumed */
        if (!bout->nexus.direct.priming) {
            ALOGV_FIFO_INFO(simple_decoder, playpump);

            BA_LOG(DIR_DBG, "%s: priming decoder for resume", __FUNCTION__);
            bout->nexus.direct.priming = true;
        }
    }
    return 0;
}

static NEXUS_Error nexus_direct_finish_priming(struct brcm_stream_out *bout) {
    NEXUS_Error res;

    BA_LOG(DIR_DBG, "%s, %p", __FUNCTION__, bout);

    if (!bout->nexus.direct.playpump_mode || !bout->nexus.direct.playpump)
        return NEXUS_SUCCESS;

    if (bout->nexus.direct.deferred_volume) {
        if (bout->nexus.direct.deferred_volume_ms) {
            struct timespec now;
            int32_t deferred_ms;

            clock_gettime(CLOCK_MONOTONIC, &now);

            // Apply existing deferred volume
            res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.direct.simple_decoder, NULL,
                                                 bout->nexus.direct.deferred_volume_ms, bout->nexus.direct.fadeLevel);

            // Record delta between resume and first start as subsequent deferral amount
            deferred_ms = (int32_t)((now.tv_sec - bout->nexus.direct.start_ts.tv_sec) * 1000) +
                          (int32_t)((now.tv_nsec - bout->nexus.direct.start_ts.tv_nsec)/1000000);
            bout->nexus.direct.deferred_volume_ms = (deferred_ms > MAX_VOLUME_DEFERRAL) ? MAX_VOLUME_DEFERRAL : deferred_ms;

            // Set deferral window
            bout->nexus.direct.deferred_window = now;
            timespec_add_ms(bout->nexus.direct.deferred_window, MAX_VOLUME_DEFERRAL);
        } else {
            // No volume was set during priming, no need to defer
            bout->nexus.direct.deferred_volume = false;
        }
    }

    if (!bout->nexus.direct.deferred_volume) {
        res = nexus_common_resume_and_unmute(bout->bdev, bout->nexus.direct.simple_decoder, NULL,
                                             bout->nexus.direct.soft_unmuting, bout->nexus.direct.fadeLevel);
    }

    if (res != NEXUS_SUCCESS) {
       ALOGE("%s: Error resuming %u", __FUNCTION__, res);
       return res;
    }

    bout->nexus.direct.priming = false;
    return NEXUS_SUCCESS;
}

static int nexus_direct_bout_flush(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: at %d, device not open",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    BA_LOG(DIR_STATE, "%s, %p, started=%s", __FUNCTION__, bout, bout->started?"true":"false");
    if (bout->started) {
        nexus_direct_bout_stop(bout);
        bout->started = false;
        bout->framesPlayedTotal = 0;
    } else {
        bout->framesPlayed = 0;
        bout->framesPlayedTotal = 0;
        bout->nexus.direct.lastCount = 0;
        bout->nexus.direct.bitrate = 0;
        bout->nexus.direct.current_pos = SEED_POSITION;
        bout->nexus.direct.last_syncframe_pos = SEED_POSITION;
        bout->nexus.direct.next_syncframe_pos = SEED_POSITION;
        bout->nexus.direct.last_info.valid = false;
        bout->nexus.direct.frame_multiplier = nexus_direct_bout_get_frame_multipler(bout);
    }

    return 0;
}

static int nexus_direct_bout_write_passthrough(struct brcm_stream_out *bout,
                                               const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;

        BA_LOG(VERB, "%s: at %d, getPassthroughBuffer", __FUNCTION__, __LINE__);
        ret = NEXUS_SimpleAudioDecoder_GetPassthroughBuffer(simple_decoder,
                                                            &nexus_buffer, &nexus_space);
        BA_LOG(VERB, "%s: at %d, getPassthroughBuffer got %d bytes", __FUNCTION__, __LINE__, nexus_space);
        if (ret) {
            ALOGE("%s: at %d, get buffer failed, ret = %d",
                 __FUNCTION__, __LINE__, ret);
            ret = -ENOSYS;
            break;
        }

        if (nexus_space) {
            size_t bytes_to_copy;

            bytes_to_copy = (bytes <= nexus_space) ? bytes : nexus_space;
            memcpy(nexus_buffer,
                   (void *)((int)(intptr_t)buffer + bytes_written),
                   bytes_to_copy);

            ret = NEXUS_SimpleAudioDecoder_PassthroughWriteComplete(simple_decoder,
                                                                    bytes_to_copy);
            if (ret) {
                ALOGE("%s: at %d, commit buffer failed, ret = %d",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;

        } else {
            NEXUS_SimpleAudioDecoderHandle prev_simple_decoder = simple_decoder;

            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 750);
            pthread_mutex_lock(&bout->lock);

            // Suspend check when relocking
            if (bout->suspended) {
                ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                return -ENOSYS;
            }
            // Sanity check when relocking
            simple_decoder = bout->nexus.direct.simple_decoder;
            ALOG_ASSERT(simple_decoder == prev_simple_decoder);
            if (ret != BERR_SUCCESS) {
                ALOGE("%s: at %d, decoder timeout, ret = %d",
                     __FUNCTION__, __LINE__, ret);

                if (ret != BERR_TIMEOUT) {
                    /* Stop decoder */
                    NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
                    bout->started = false;
                    ret = -ENOSYS;
                }
                else {
                    /* Give decoder more time to free space */
                    ret = 0;
                }
                break;
            }
        }
    }

    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    return bytes_written;
}

static bool nexus_direct_bout_get_bitrate(struct brcm_stream_out *bout,
                                          const void* buffer, size_t bytes)
{
    bool changed = false;

    // For playpump mode, parse the frame header to determine the bitrate
    if (bout->config.format == AUDIO_FORMAT_AC3) {
        // For AC3, get bitrate from the first header, do not adapt bitrate
        if (bout->nexus.direct.bitrate == 0) {
            const uint8_t *syncframe;
            eac3_frame_hdr_info info;

            syncframe = nexus_find_ac3_sync_frame((uint8_t *)buffer, bytes, &info);
            if (syncframe == NULL) {
                ALOGE("%s: Sync frame not found", __FUNCTION__);
            } else {
                if (syncframe != buffer) {
                    ALOGE("%s: Stream not starting with sync frame", __FUNCTION__);
                }
                bout->nexus.direct.bitrate = info.bitrate;
                changed = true;
                ALOGI("%s: %u Kbps AC3 detected, mpy:%u", __FUNCTION__,
                    bout->nexus.direct.bitrate, bout->nexus.direct.frame_multiplier);
            }
        }
    } else if (bout->config.format == AUDIO_FORMAT_E_AC3) {
        // For E-AC3, get bitrate from frame sizes of all dependent frames

        // Make sure next posistion is valid
        ALOG_ASSERT((((signed)bout->nexus.direct.next_syncframe_pos - (signed)bout->nexus.direct.current_pos) + EAC3_SYNCFRAM_HEADER_SIZE) > 0);

        // Look for all available syncframes in buffer
        while (((bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos) + EAC3_SYNCFRAME_HEADER_SIZE) <= bytes) {
            eac3_frame_hdr_info info;
            bool header_valid = false;

            if (((signed)bout->nexus.direct.current_pos - (signed)bout->nexus.direct.next_syncframe_pos) > 0) {
                // Handle partial header from previous buffer
                size_t bytes_in_header = bout->nexus.direct.current_pos - bout->nexus.direct.next_syncframe_pos;
                size_t bytes_to_copy = EAC3_SYNCFRAME_HEADER_SIZE - bytes_in_header;

                memcpy(&bout->nexus.direct.partial_header[bytes_in_header], buffer, bytes_to_copy);
                header_valid = nexus_parse_eac3_frame_hdr(bout->nexus.direct.partial_header, EAC3_SYNCFRAME_HEADER_SIZE, &info);
            } else {
                size_t start_pos = bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos;
                header_valid = nexus_parse_eac3_frame_hdr((const uint8_t *)buffer + start_pos, bytes - start_pos, &info);
            }

            if (header_valid) {
                BA_LOG(DIR_DBG, "%s: eac3 at %u: blks %u rate %u bitrate %u framesize %u type %u id %u", __FUNCTION__,
                           bout->nexus.direct.next_syncframe_pos,
                           info.num_audio_blks, info.sample_rate, info.bitrate,
                           info.frame_size, info.stream_type, info.substream_id);
                // Gather all framesizes until the next independent substream 0 to determine bitrate
                if (((info.stream_type == EAC3_STRMTYP_0) || (info.stream_type == EAC3_STRMTYP_2)) && (info.substream_id == 0)) {
                    if (bout->nexus.direct.last_info.valid && (bout->nexus.direct.next_syncframe_pos != bout->nexus.direct.last_syncframe_pos)) {
                        size_t frame_size = bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.last_syncframe_pos;
                        unsigned bitrate = (frame_size * 8 * bout->nexus.direct.last_info.sample_rate) /
                                               (bout->nexus.direct.last_info.num_audio_blks * 256 * 1000);
                        if (bitrate != bout->nexus.direct.bitrate) {
                            ALOGI("%s: %u -> %u Kbps EAC3 detected @ %u/%u", __FUNCTION__, bout->nexus.direct.bitrate, bitrate,
                                  bout->nexus.direct.next_syncframe_pos, bout->nexus.direct.current_pos);
                            bout->nexus.direct.bitrate = bitrate;
                            changed = true;
                        }
                    }
                    bout->nexus.direct.last_syncframe_pos = bout->nexus.direct.next_syncframe_pos;
                    bout->nexus.direct.last_info = info;
                }
                // Move to next syncframe
                bout->nexus.direct.next_syncframe_pos += info.frame_size;
            } else {
                // Where did header go?!?
                const uint8_t *syncframe = NULL;
                size_t start_pos;

                ALOGE("%s: Sync frame not found at %u [%u..%u]", __FUNCTION__,
                    bout->nexus.direct.next_syncframe_pos, bout->nexus.direct.current_pos, bout->nexus.direct.current_pos + bytes);

                // Search buffer for first sync frame
                if (((signed)bout->nexus.direct.current_pos - (signed)bout->nexus.direct.next_syncframe_pos) > 0) {
                    // Search entire buffer if the last check was in partial buffer
                    start_pos = 0;
                } else {
                    // Search the byte after the last predicted syncframe
                    start_pos = bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos + B_AC3_SYNC_LEN;
                }
                if (bytes > start_pos) {
                    syncframe = nexus_find_eac3_independent_frame((const uint8_t *)buffer + start_pos, bytes - start_pos, 0, &info);
                }

                if (syncframe) {
                    bout->nexus.direct.last_syncframe_pos = bout->nexus.direct.current_pos + (syncframe - (const uint8_t *)buffer);
                    bout->nexus.direct.last_info = info;
                    bout->nexus.direct.next_syncframe_pos = bout->nexus.direct.last_syncframe_pos + info.frame_size;
                    ALOGI("%s: Reset header position to %u -> %u", __FUNCTION__,
                          bout->nexus.direct.last_syncframe_pos, bout->nexus.direct.next_syncframe_pos);
                } else {
                    // No syncframes in this buffer, start looking at next one
                    bout->nexus.direct.last_syncframe_pos = bout->nexus.direct.current_pos + bytes;
                    bout->nexus.direct.last_info.valid = false;
                    bout->nexus.direct.next_syncframe_pos = bout->nexus.direct.last_syncframe_pos;
                    ALOGI("%s: Look for header at %u", __FUNCTION__, bout->nexus.direct.next_syncframe_pos);
                }
            }
        }

        if (((signed)bout->nexus.direct.next_syncframe_pos - (signed)bout->nexus.direct.current_pos) >= 0) {
            // Save partial header at end of buffer
            if ((bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos) < bytes) {
                size_t bytes_to_copy = bytes - (bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos);
                BA_LOG(DIR_WRITE, "%s: Partial header at %u [%u..%u] %u", __FUNCTION__,
                    bout->nexus.direct.next_syncframe_pos, bout->nexus.direct.current_pos, bout->nexus.direct.current_pos + bytes,
                    bytes_to_copy);
                memcpy(bout->nexus.direct.partial_header,
                       (const uint8_t *)buffer + (bout->nexus.direct.next_syncframe_pos - bout->nexus.direct.current_pos),
                       bytes_to_copy);
            }
        } else {
            // This case only happens if a header is broken into more than 2 writes.
            size_t bytes_in_header = bout->nexus.direct.current_pos - bout->nexus.direct.next_syncframe_pos;
            size_t bytes_to_copy = bytes;

            ALOG_ASSERT(bytes_in_header < EAC3_SYNCFRAME_HEADER_SIZE);
            ALOG_ASSERT(bytes < (EAC3_SYNCFRAME_HEADER_SIZE - bytes_in_header));

            ALOGW("%s: Saving %u bytes of hader at %u to %u",  __FUNCTION__, bytes, bout->nexus.direct.current_pos, bytes_in_header);
            memcpy(&bout->nexus.direct.partial_header[bytes_in_header], buffer, bytes);
        }

    } else {
        // Should never get here
        ALOG_ASSERT(0);
    }

    return changed;
}

static int nexus_direct_bout_write_playpump(struct brcm_stream_out *bout,
                                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;
    bool bitrate_changed;

    // determine the bitrate
    bitrate_changed = nexus_direct_bout_get_bitrate(bout, buffer, bytes);

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;
        NEXUS_AudioDecoderStatus decoderStatus;
        NEXUS_PlaypumpStatus playpumpStatus;
        NEXUS_AudioDecoderTrickState trickState;

        NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &decoderStatus);
        NEXUS_Playpump_GetStatus(playpump, &playpumpStatus);
        NEXUS_SimpleAudioDecoder_GetTrickState(simple_decoder, &trickState);
        BA_LOG(VERB, "%s: AC3 bitrate = %u, decoder = %u/%u, playpump = %u/%u, trick=%u", __FUNCTION__,
            decoderStatus.codecStatus.ac3.bitrate,
            decoderStatus.fifoDepth,
            decoderStatus.fifoSize,
            playpumpStatus.fifoDepth,
            playpumpStatus.fifoSize,
            trickState.rate);

        if ( !bout->nexus.direct.priming && trickState.rate ) {
            /* If not priming nor paused, do not buffer too much data unless bitrate change pending */
            if (!bitrate_changed && bout->nexus.direct.bitrate &&
                (decoderStatus.fifoDepth + playpumpStatus.fifoDepth) >=
                     BITRATE_TO_BYTES_PER_250_MS(bout->nexus.direct.bitrate)) {
                BA_LOG(VERB, "%s: at %d, Already have enough data %zu/%u.", __FUNCTION__, __LINE__,
                      (decoderStatus.fifoDepth + playpumpStatus.fifoDepth),
                       BITRATE_TO_BYTES_PER_250_MS(decoderStatus.codecStatus.ac3.bitrate));
                pthread_mutex_unlock(&bout->lock);
                ret = BKNI_WaitForEvent(event, 50);
                pthread_mutex_lock(&bout->lock);
                // Suspend check when relocking
                if (bout->suspended) {
                    ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                    return -ENOSYS;
                }
                if (ret == BERR_TIMEOUT) {
                    return bytes_written;
                } else {
                    BA_LOG(VERB, "%s: at %d, Check for more room.", __FUNCTION__, __LINE__);
                    continue;
                }
            }
        }

        BA_LOG(VERB, "%s: at %d, Playpump_GetBuffer", __FUNCTION__, __LINE__);
        ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
        BA_LOG(VERB, "%s: at %d, Playpump_GetBuffer got %d bytes", __FUNCTION__, __LINE__, nexus_space);

        if (ret) {
            ALOGE("%s: at %d, get buffer failed, ret = %d",
                 __FUNCTION__, __LINE__, ret);
            ret = -ENOSYS;
            break;
        }

        if (nexus_space) {
            size_t bytes_to_copy;

            bytes_to_copy = (bytes <= nexus_space) ? bytes : nexus_space;
            memcpy(nexus_buffer,
                   (void *)((int)(intptr_t)buffer + bytes_written),
                   bytes_to_copy);

            ret = NEXUS_Playpump_WriteComplete(playpump, 0, bytes_to_copy);
            if (ret) {
                ALOGE("%s: at %d, commit buffer failed, ret = %d",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;
        } else {
            NEXUS_SimpleAudioDecoderHandle prev_simple_decoder = simple_decoder;
            NEXUS_PlaypumpHandle prev_playpump = playpump;

            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 750);
            pthread_mutex_lock(&bout->lock);

            // Suspend check when relocking
            if (bout->suspended) {
                ALOGE("%s: at %d, device already suspended\n", __FUNCTION__, __LINE__);
                return -ENOSYS;
            }
            // Sanity check when relocking
            simple_decoder = bout->nexus.direct.simple_decoder;
            ALOG_ASSERT(simple_decoder == prev_simple_decoder);
            playpump = bout->nexus.direct.playpump;
            ALOG_ASSERT(playpump == prev_playpump);
            if (ret != BERR_SUCCESS) {
                ALOGE("%s: at %d, decoder timeout, ret = %d",
                     __FUNCTION__, __LINE__, ret);

                if (ret != BERR_TIMEOUT) {
                    /* Stop decoder */
                    NEXUS_Playpump_Stop(playpump);
                    NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
                    bout->started = false;
                    ret = -ENOSYS;
                }
                else {
                    /* Give decoder more time to free space */
                    ret = 0;
                }
                break;
            }
        }

        /* Finish priming playpump if enough data */
        if (bout->nexus.direct.priming && bout->nexus.direct.bitrate) {
            NEXUS_AudioDecoderStatus decoderStatus;
            NEXUS_PlaypumpStatus playpumpStatus;
            NEXUS_AudioDecoderTrickState trickState;

            NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &decoderStatus);
            NEXUS_Playpump_GetStatus(playpump, &playpumpStatus);
            NEXUS_SimpleAudioDecoder_GetTrickState(simple_decoder, &trickState);
            BA_LOG(VERB, "%s: AC3 bitrate = %u, decoder = %u/%u, playpump = %u/%u, trick=%u", __FUNCTION__,
                decoderStatus.codecStatus.ac3.bitrate,
                decoderStatus.fifoDepth,
                decoderStatus.fifoSize,
                playpumpStatus.fifoDepth,
                playpumpStatus.fifoSize,
                trickState.rate);

            if ((decoderStatus.fifoDepth + playpumpStatus.fifoDepth) >=
                    BITRATE_TO_BYTES_PER_250_MS(bout->nexus.direct.bitrate)) {
                NEXUS_Error res;

                BA_LOG(DIR_WRITE, "%s: at %d, Already have enough data.  Finished priming.", __FUNCTION__, __LINE__);
                res = nexus_direct_finish_priming(bout);
                if (res == NEXUS_SUCCESS) {
                    continue;
                } else {
                    ALOGE("%s: Error resuming audio decoder %u", __FUNCTION__, res);
                }
            }
        }
    }

    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    return bytes_written;
}

static int nexus_direct_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;
    int ret;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: not ready to output audio samples", __FUNCTION__);
        return -ENOSYS;
    }

    // For playpump mode, parse the frame header to determine the bitrate
    if (bout->nexus.direct.playpump_mode)
        ret = nexus_direct_bout_write_playpump(bout, buffer, bytes);
    else
        ret = nexus_direct_bout_write_passthrough(bout, buffer, bytes);

    if (ret > 0)
        bout->nexus.direct.current_pos += ret;

    return ret;
}

// returns true if nexus audio can enter standby
static bool nexus_direct_bout_standby_monitor(void *context)
{
    bool standby = true;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;

    if (bout != NULL) {
        pthread_mutex_lock(&bout->lock);
        standby = (bout->started == false);
        pthread_mutex_unlock(&bout->lock);
    }
    BA_LOG(DIR_DBG, "%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

static int nexus_direct_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_SimpleAudioDecoderHandle simple_decoder;
    NEXUS_PlaypumpHandle playpump;
    NEXUS_Error rc;
    android::status_t status;
    NEXUS_AudioCapabilities audioCaps;
    bool disable_ac3_passthrough;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    BKNI_EventHandle event;
    uint32_t audioDecoderId;
    String8 rates_str, channels_str, formats_str;
    char config_rate_str[11];
    int i, ret = 0;

    BA_LOG_INIT();
    if (config->sample_rate == 0)
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    if (config->channel_mask == 0)
        config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    if (config->format == 0)
        config->format = NEXUS_OUT_DEFAULT_FORMAT;

    bout->nexus.direct.playpump_mode = false;
    bout->nexus.direct.transcode_latency = 0;
    bout->nexus.direct.paused = false;

    BA_LOG(DIR_STATE, "%s, format:%u", __FUNCTION__, config->format);
    switch (config->format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        nexus_get_hdmi_parameters(rates_str, channels_str, formats_str);
        snprintf(config_rate_str, 11, "%u", config->sample_rate);

        /* Check if sample rate is supported */
        if (!rates_str.contains(config_rate_str)) {
            return -EINVAL;
        }

        if (config->channel_mask != AUDIO_CHANNEL_OUT_STEREO)
            return -EINVAL;
        break;
    case AUDIO_FORMAT_AC3:
        nexus_get_hdmi_parameters(rates_str, channels_str, formats_str);
        NEXUS_GetAudioCapabilities(&audioCaps);
        disable_ac3_passthrough = property_get_bool(BCM_RO_AUDIO_DIRECT_DISABLE_AC3_PASSTHROUGH,
                                                    formats_str.contains("AUDIO_FORMAT_AC3")?false:true);
        if (disable_ac3_passthrough && audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode) {
            ALOGI("Enable play pump mode");
            bout->nexus.direct.playpump_mode = true;
            bout->nexus.direct.transcode_latency = property_get_int32(
                            BCM_RO_AUDIO_OUTPUT_EAC3_TRANS_LATENCY,
                            BRCM_AUDIO_DIRECT_EAC3_TRANS_LATENCY);
        }

        if (!bout->nexus.direct.playpump_mode) {
            /* Suggest PCM wrapped format */
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            config->format = AUDIO_FORMAT_PCM_16_BIT;
            return -EINVAL;
        }
        break;
    case AUDIO_FORMAT_E_AC3:
        NEXUS_GetAudioCapabilities(&audioCaps);
        if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode) {
            ALOGI("Enable play pump mode");
            bout->nexus.direct.playpump_mode = true;
            bout->nexus.direct.transcode_latency = property_get_int32(
                            BCM_RO_AUDIO_OUTPUT_EAC3_TRANS_LATENCY,
                            BRCM_AUDIO_DIRECT_EAC3_TRANS_LATENCY);
        }

        if (!bout->nexus.direct.playpump_mode) {
            /* Suggest PCM wrapped format */
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            config->format = AUDIO_FORMAT_PCM_16_BIT;
            config->sample_rate = config->sample_rate * 4;
            return -EINVAL;
        }
        break;
    default:
        return -EINVAL;
    }

    bout->framesPlayedTotal = 0;
    if (bout->nexus.direct.playpump_mode) {
        bout->latencyEstimate = (property_get_int32(BCM_RO_AUDIO_OUTPUT_MIXER_LATENCY,
                                                    ((bout->bdev->dolby_ms == 11) || (bout->bdev->dolby_ms == 12)) ?
                                                        NEXUS_DEFAULT_MS_MIXER_LATENCY : 0)
                                 * bout->config.sample_rate) / 1000;
    } else {
        bout->latencyEstimate = 0;
    }

    bout->latencyPad = bout->latencyEstimate;

    if (config->format == AUDIO_FORMAT_PCM_16_BIT) {
        bout->frameSize = audio_bytes_per_sample(config->format) * popcount(config->channel_mask);
        bout->buffer_size =
            get_brcm_audio_buffer_size(config->sample_rate,
                                       config->format,
                                       popcount(config->channel_mask),
                                       NEXUS_OUT_BUFFER_DURATION_MS);
    } else {
        /* Frame size should be single byte long for compressed audio formats */
        bout->frameSize = 1;
        bout->buffer_size = BRCM_AUDIO_DIRECT_COMP_BUFFER_SIZE;
    }

    /* Allocate simpleAudioDecoder */
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleAudioDecoder = 1;
    rc = NxClient_Alloc(&allocSettings, &(bout->nexus.allocResults));
    if (rc) {
        ALOGE("%s: Cannot allocate NxClient resources, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_alloc;
    }

    audioDecoderId = bout->nexus.allocResults.simpleAudioDecoder.id;
    simple_decoder = NEXUS_SimpleAudioDecoder_Acquire(audioDecoderId);
    if (!simple_decoder) {
        ALOGE("%s: at %d, acquire Nexus simple decoder handle failed",
             __FUNCTION__, __LINE__);
        ret = -ENOSYS;
        goto err_acquire;
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleAudioDecoder.id = audioDecoderId;

    if (bout->nexus.direct.playpump_mode && (bout->bdev->dolby_ms == 12)) {
        connectSettings.simpleAudioDecoder.decoderCapabilities.type = NxClient_AudioDecoderType_ePersistent;
    }

    rc = NxClient_Connect(&connectSettings, &(bout->nexus.connectId));
    if (rc) {
        ALOGE("%s: error calling NxClient_Connect, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_acquire;
    }

    ret = BKNI_CreateEvent(&event);
    if (ret) {
        ALOGE("%s: at %d, create event failed, ret = %d",
             __FUNCTION__, __LINE__, ret);
        ret = -ENOSYS;
        goto err_event;
    }

    // register standby callback
    bout->standbyCallback = bout->bdev->standbyThread->RegisterCallback(nexus_direct_bout_standby_monitor, bout);
    if (bout->standbyCallback < 0) {
        ALOGE("Error registering standby callback");
        ret = -ENOSYS;
        goto err_callback;
    }

    bout->nexus.direct.savedHDMIOutputMode = NxClient_AudioOutputMode_eMax;
    bout->nexus.direct.savedSPDIFOutputMode = NxClient_AudioOutputMode_eMax;

    // Open play pump
    if (bout->nexus.direct.playpump_mode) {
        NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
        NEXUS_PlaypumpSettings playpumpSettings;
        NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
        NEXUS_PidChannelHandle pid_channel;

        NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
        playpumpOpenSettings.fifoSize = BRCM_AUDIO_DIRECT_PLAYPUMP_FIFO_SIZE;
        playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);

        if (!playpump) {
            ALOGE("%s: Error opening playpump", __FUNCTION__);
            ret = -ENOMEM;
            goto err_playpump;
        }

        NEXUS_Playpump_GetSettings(playpump, &playpumpSettings);
        playpumpSettings.transportType = NEXUS_TransportType_eEs;
        playpumpSettings.dataCallback.callback = nexus_direct_bout_data_callback;
        playpumpSettings.dataCallback.context = bout;
        playpumpSettings.dataCallback.param = (int)(intptr_t)event;
        ret = NEXUS_Playpump_SetSettings(playpump, &playpumpSettings);
        if (ret) {
            ALOGE("%s: Error setting playpump settings, ret = %d", __FUNCTION__, ret);
            ret = -ENOSYS;
            goto err_playpump_set;
        }

        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
        pidSettings.pidType = NEXUS_PidType_eAudio;
        pid_channel = NEXUS_Playpump_OpenPidChannel(playpump, BRCM_AUDIO_STREAM_ID, &pidSettings);
        if (!pid_channel) {
            ALOGE("%s: Error openning pid channel", __FUNCTION__);
            ret = -ENOMEM;
            goto err_pid;
        }

        bout->nexus.direct.playpump = playpump;
        bout->nexus.direct.pid_channel = pid_channel;

        // Force PCM mode if necessary
        if (property_get_bool(BCM_RO_AUDIO_DIRECT_FORCE_PCM, false) ||
            property_get_bool(BCM_PERSIST_AUDIO_DIRECT_FORCE_PCM, false)) {
            NxClient_AudioSettings audioSettings;

            ALOGI("Force PCM output");
            NxClient_GetAudioSettings(&audioSettings);
            bout->nexus.direct.savedHDMIOutputMode = audioSettings.hdmi.outputMode;
            bout->nexus.direct.savedSPDIFOutputMode = audioSettings.spdif.outputMode;
            audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_ePcm;
            audioSettings.spdif.outputMode = NxClient_AudioOutputMode_ePcm;
            ret = NxClient_SetAudioSettings(&audioSettings);
            if (ret) {
                ALOGE("%s: Error setting PCM mode, ret = %d", __FUNCTION__, ret);
            }
        }
    }

    bout->nexus.direct.simple_decoder = simple_decoder;
    bout->nexus.direct.lastCount = 0;
    bout->nexus.direct.bitrate = 0;
    bout->nexus.direct.current_pos = SEED_POSITION;
    bout->nexus.direct.last_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.next_syncframe_pos = SEED_POSITION;
    bout->nexus.direct.last_info.valid = false;
    bout->nexus.direct.frame_multiplier = nexus_direct_bout_get_frame_multipler(bout);
    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

    bout->nexus.direct.soft_muting = property_get_int32(BCM_RO_AUDIO_SOFT_MUTING, 10);
    bout->nexus.direct.soft_unmuting = property_get_int32(BCM_RO_AUDIO_SOFT_UNMUTING, 30);
    bout->nexus.direct.sleep_after_mute = property_get_int32(BCM_RO_AUDIO_SLEEP_AFTER_MUTE, 30);

    // Restore auto mode for MS11
    if ((bout->bdev->dolby_ms == 11) &&
        !(property_get_bool(BCM_RO_AUDIO_DIRECT_FORCE_PCM, false) ||
          property_get_bool(BCM_PERSIST_AUDIO_DIRECT_FORCE_PCM, false))) {
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

    if (bout->nexus.direct.playpump_mode) {
        bout->nexus.direct.fadeLevel = 100;
        bout->nexus.direct.deferred_volume = true;
        bout->nexus.direct.deferred_volume_ms = 0;
    } else {
        bout->nexus.direct.deferred_volume = false;
        bout->nexus.direct.deferred_volume_ms = 0;
    }

    return 0;

err_pid:
err_playpump_set:
    if (bout->nexus.direct.playpump_mode) {
        NEXUS_Playpump_Close(playpump);
    }
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

static int nexus_direct_bout_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

    BA_LOG(DIR_STATE, "%s", __FUNCTION__);
    if (bout->nexus.state == BRCM_NEXUS_STATE_DESTROYED) {
        return 0;
    }

    // Restore output  mode if necessary
    if ((bout->nexus.direct.savedHDMIOutputMode != NxClient_AudioOutputMode_eMax) ||
        (bout->nexus.direct.savedSPDIFOutputMode != NxClient_AudioOutputMode_eMax))
    {
        NxClient_AudioSettings audioSettings;

        NxClient_GetAudioSettings(&audioSettings);
        if (bout->nexus.direct.savedHDMIOutputMode != NxClient_AudioOutputMode_eMax)
            audioSettings.hdmi.outputMode = bout->nexus.direct.savedHDMIOutputMode;
        if (bout->nexus.direct.savedSPDIFOutputMode != NxClient_AudioOutputMode_eMax)
            audioSettings.spdif.outputMode = bout->nexus.direct.savedSPDIFOutputMode;

        ALOGI("Restore audio output mode");
        NxClient_SetAudioSettings(&audioSettings);
    } else if (bout->bdev->dolby_ms == 11) { // Force PCM mode for MS11
        NxClient_AudioSettings audioSettings;

        ALOGI("Force PCM output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_ePcm;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_ePcm;
        NxClient_SetAudioSettings(&audioSettings);
    }

    if (bout->nexus.direct.playpump_mode) {
        if ( bout->nexus.direct.pid_channel) {
            ALOG_ASSERT(bout->nexus.direct.playpump != NULL);
            NEXUS_Playpump_ClosePidChannel(bout->nexus.direct.playpump, bout->nexus.direct.pid_channel);
            bout->nexus.direct.pid_channel = NULL;
        }

        if (bout->nexus.direct.playpump) {
            NEXUS_Playpump_Close(bout->nexus.direct.playpump);
            bout->nexus.direct.playpump = NULL;
        }
    }

    nexus_direct_bout_stop(bout);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);
    bout->nexus.direct.simple_decoder = NULL;

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    return 0;
}

static int nexus_direct_bout_dump(struct brcm_stream_out *bout, int fd)
{
   dprintf(fd, "\nnexus_direct_bout_dump:\n"
            "\tpid-channel = %p\n"
            "\tplaypump    = %p (%s)\n"
            "\tdecoder     = %p\n"
            "\tbitrate     = %u\n",
            (void *)bout->nexus.direct.pid_channel,
            (void *)bout->nexus.direct.playpump,
            bout->nexus.direct.playpump_mode?"enabled":"disabled",
            (void *)bout->nexus.direct.simple_decoder,
            bout->nexus.direct.bitrate);

   return 0;
}

struct brcm_stream_out_ops nexus_direct_bout_ops = {
    .do_bout_open = nexus_direct_bout_open,
    .do_bout_close = nexus_direct_bout_close,
    .do_bout_get_latency = nexus_direct_bout_get_latency,
    .do_bout_start = nexus_direct_bout_start,
    .do_bout_stop = nexus_direct_bout_stop,
    .do_bout_write = nexus_direct_bout_write,
    .do_bout_set_volume = nexus_direct_bout_set_volume,
    .do_bout_get_render_position = nexus_direct_bout_get_render_position,
    .do_bout_get_presentation_position = nexus_direct_bout_get_presentation_position,
    .do_bout_dump = nexus_direct_bout_dump,
    .do_bout_get_parameters = nexus_direct_bout_get_parameters,
    .do_bout_pause = nexus_direct_bout_pause,
    .do_bout_resume = nexus_direct_bout_resume,
    .do_bout_drain = NULL,
    .do_bout_flush = nexus_direct_bout_flush,
};
