/******************************************************************************
 *    (c)2015-2017 Broadcom Corporation
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

using namespace android;

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   48000
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

#define NEXUS_OUT_BUFFER_DURATION_MS    10

#define BRCM_AUDIO_DIRECT_NXCLIENT_NAME "BrcmAudioOutDirect"

#define BRCM_AUDIO_STREAM_ID                    (0xC0)
#define BRCM_AUDIO_DIRECT_COMP_BUFFER_SIZE      (2048)
#define BRCM_AUDIO_DIRECT_PLAYPUMP_FIFO_SIZE    (32768)
#define BRCM_AUDIO_DIRECT_DECODER_FIFO_SIZE     (16384)
#define BRCM_AUDIO_DIRECT_EAC3_TRANS_LATENCY    (128)

/*
 * Runtime Properties
 */
#define BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DRC_MODE ("media.brcm.direct_drc_mode")
#define BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_STEREO_DOWNMIX_MODE ("media.brcm.direct_stereo_mode")

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
    (void)bout;
    (void)left;
    (void)right;
    return 0;
}

static uint32_t nexus_direct_bout_get_latency(struct brcm_stream_out *bout)
{
    (void)bout;

    // Estimated experimental value
    if (bout->nexus.direct.playpump_mode) {
        return bout->nexus.direct.transcode_latency;
    }

    return 0;
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
            *dsp_frames = (uint32_t)(bout->framesPlayed * NEXUS_PCM_FRAMES_PER_EAC3_FRAME);
            if (bout->framesPlayed)
                *dsp_frames += (NEXUS_PCM_FRAMES_PER_EAC3_FRAME / 2);

        } else {
            /* numBytesDecoded for passthrough mode returns the underlying playback playedBytes, which is of type size_t */
            bout->framesPlayed += ((size_t)status.numBytesDecoded - bout->nexus.direct.lastCount) / bout->frameSize;
            bout->nexus.direct.lastCount = status.numBytesDecoded;
            *dsp_frames = (uint32_t)bout->framesPlayed;
        }
    } else {
       *dsp_frames = 0;
    }
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
            *frames = (bout->framesPlayedTotal + bout->framesPlayed) * NEXUS_PCM_FRAMES_PER_EAC3_FRAME;
            if (*frames)
                *frames += (NEXUS_PCM_FRAMES_PER_EAC3_FRAME / 2);
        } else {
            /* numBytesDecoded for passthrough mode returns the underlying playback playedBytes, which is of type size_t */
            bout->framesPlayed += ((size_t)status.numBytesDecoded - bout->nexus.direct.lastCount) / bout->frameSize;
            bout->nexus.direct.lastCount = status.numBytesDecoded;
            *frames = bout->framesPlayedTotal + bout->framesPlayed;
        }
    } else {
       *frames =0;
    }
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
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          formats_str.isEmpty()?"AUDIO_FORMAT_PCM_16_BIT":formats_str.string());
    }
    pthread_mutex_unlock(&bout->lock);

    result_str = str_parms_to_str(result);
    str_parms_destroy(query);
    str_parms_destroy(result);

    ALOGV("%s: result = %s", __FUNCTION__, result_str);

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

    ALOGV("%s, %p", __FUNCTION__, bout);

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

            if (property_get(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DRC_MODE, property, "")) {
                value = get_property_value(property, drc_mode_map);
                if (value != -1) {
                    ALOGI("%s: Setting Dolby DRC mode to %d", __FUNCTION__, value);
                    dolbySettings->drcMode = (NEXUS_AudioDecoderDolbyDrcMode)value;
                    dolbySettings->drcModeDownmix = (NEXUS_AudioDecoderDolbyDrcMode)value;
                    changed = true;
                }
            }
            if (property_get(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_STEREO_DOWNMIX_MODE, property, "")) {
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
        NEXUS_AudioDecoderTrickState trickState;
        NEXUS_Error res;

        ALOGV("%s: pausing to prime decoder", __FUNCTION__);

        NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.direct.simple_decoder, &trickState);
        trickState.rate = 0;
        res = NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.direct.simple_decoder, &trickState);
        if (res == NEXUS_SUCCESS) {
            bout->nexus.direct.priming = true;
        } else {
            bout->nexus.direct.priming = false;
            ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, res);
        }
    }

    bout->nexus.direct.lastCount = 0;
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_direct_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;

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
        ALOGV("%s: setting framesPlayedTotal to %u", __FUNCTION__, bout->framesPlayedTotal);
    }

    if (bout->nexus.direct.playpump_mode && playpump) {
        NEXUS_Playpump_Stop(playpump);
    }

    bout->nexus.direct.lastCount = 0;
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

    if (bout->nexus.direct.playpump_mode && playpump) {
        if (!bout->nexus.direct.priming) {
            NEXUS_AudioDecoderTrickState trickState;
            NEXUS_Error res;

            ALOGV("%s, %p", __FUNCTION__, bout);

            NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.direct.simple_decoder, &trickState);
            if (trickState.rate != 0) {
                trickState.rate = 0;
                ALOGV("%s, %p pause decoder", __FUNCTION__, bout);
                res = NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.direct.simple_decoder, &trickState);
                if (res != NEXUS_SUCCESS) {
                    ALOGE("%s: Error pausing audio decoder %u", __FUNCTION__, res);
                }
            }
        } else {
            ALOGV("%s, %p stop priming", __FUNCTION__, bout);
            bout->nexus.direct.priming = false;
        }
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

    if (bout->nexus.direct.playpump_mode && playpump) {
        /* Prime again only if not yet resumed */
        if (!bout->nexus.direct.priming) {
            NEXUS_AudioDecoderTrickState trickState;

            ALOGV("%s, %p", __FUNCTION__, bout);

            NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.direct.simple_decoder, &trickState);
            if (trickState.rate == 0) {
                ALOGV("%s: priming decoder for resume", __FUNCTION__);
                bout->nexus.direct.priming = true;
            }
        }
    }
    return 0;
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

    ALOGV("%s, %p, started=%s", __FUNCTION__, bout, bout->started?"true":"false");
    if (bout->started) {
        nexus_direct_bout_stop(bout);
        nexus_direct_bout_start(bout);
    }

    bout->framesPlayed = 0;
    bout->framesPlayedTotal = 0;
    bout->nexus.direct.lastCount = 0;

    return 0;
}

static int nexus_direct_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.direct.simple_decoder;
    NEXUS_PlaypumpHandle playpump = bout->nexus.direct.playpump;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;

    if (bout->suspended || !simple_decoder || (bout->nexus.direct.playpump_mode && !playpump)) {
        ALOGE("%s: not ready to output audio samples", __FUNCTION__);
        return -ENOSYS;
    }

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;

        if (bout->nexus.direct.playpump_mode) {
            ALOGVV("%s: at %d, Playpump_GetBuffer", __FUNCTION__, __LINE__);
            ret = NEXUS_Playpump_GetBuffer(playpump, &nexus_buffer, &nexus_space);
            ALOGVV("%s: at %d, Playpump_GetBuffer got %d bytes", __FUNCTION__, __LINE__, nexus_space);
        } else {
            ALOGVV("%s: at %d, getPassthroughBuffer", __FUNCTION__, __LINE__);
            ret = NEXUS_SimpleAudioDecoder_GetPassthroughBuffer(simple_decoder,
                                                      &nexus_buffer, &nexus_space);
            ALOGVV("%s: at %d, getPassthroughBuffer got %d bytes", __FUNCTION__, __LINE__, nexus_space);
        }
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
                   (void *)((int)buffer + bytes_written),
                   bytes_to_copy);

            if (bout->nexus.direct.playpump_mode)
                ret = NEXUS_Playpump_WriteComplete(playpump, 0, bytes_to_copy);
            else
                ret = NEXUS_SimpleAudioDecoder_PassthroughWriteComplete(simple_decoder,
                                                              bytes_to_copy);
            if (ret) {
                ALOGE("%s: at %d, commit buffer failed, ret = %d",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;
        }
        else {
            /* Unpause decoder after priming is done */
            if (bout->nexus.direct.playpump_mode && bout->nexus.direct.priming) {
                NEXUS_Error res;
                NEXUS_AudioDecoderTrickState trickState;

                ALOGV("%s: finished priming decoder", __FUNCTION__);
                NEXUS_SimpleAudioDecoder_GetTrickState(bout->nexus.direct.simple_decoder, &trickState);
                trickState.rate = NEXUS_NORMAL_DECODE_RATE;
                res = NEXUS_SimpleAudioDecoder_SetTrickState(bout->nexus.direct.simple_decoder, &trickState);
                if (res == NEXUS_SUCCESS) {
                    bout->nexus.direct.priming = false;
                    continue;
                } else {
                    ALOGE("%s: Error resuming audio decoder %u", __FUNCTION__, res);
                }
            }
            NEXUS_SimpleAudioDecoderHandle prev_simple_decoder = simple_decoder;
            NEXUS_PlaypumpHandle prev_playpump = playpump;

            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 500);
            pthread_mutex_lock(&bout->lock);

            // Sanity check when relocking
            simple_decoder = bout->nexus.direct.simple_decoder;
            ALOG_ASSERT(simple_decoder == prev_simple_decoder);
            if (bout->nexus.direct.playpump_mode) {
                playpump = bout->nexus.direct.playpump;
                ALOG_ASSERT(playpump == prev_playpump);
            }
            ALOG_ASSERT(!bout->suspended);

            if (ret != BERR_SUCCESS) {
                ALOGE("%s: at %d, decoder timeout, ret = %d",
                     __FUNCTION__, __LINE__, ret);

                /* Stop decoder */
                if (bout->nexus.direct.playpump_mode)
                    NEXUS_Playpump_Stop(playpump);
                NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
                bout->started = false;
                ret = -ENOSYS;
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

// returns true if nexus audio can enter standby
static bool nexus_direct_bout_standby_monitor(void *context)
{
    bool standby = true;
    struct brcm_stream_out *bout = (struct brcm_stream_out *)context;
    bool started;

    if (bout != NULL) {
        pthread_mutex_lock(&bout->lock);
        started = bout->started;
        pthread_mutex_unlock(&bout->lock);
        if (started) {
            bout->aout.common.standby(&bout->aout.common);
            bout->suspended = true;
        }
        else {
            standby = (started == false);
        }
    }
    ALOGV("%s: standby=%d", __FUNCTION__, standby);
    return standby;
}

static int nexus_direct_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_SimpleAudioDecoderHandle simple_decoder;
    NEXUS_PlaypumpHandle playpump;
    NEXUS_Error rc;
    android::status_t status;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    BKNI_EventHandle event;
    uint32_t audioDecoderId;
    int i, ret = 0;

    if (config->sample_rate == 0)
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    if (config->channel_mask == 0)
        config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    if (config->format == 0)
        config->format = NEXUS_OUT_DEFAULT_FORMAT;

    bout->nexus.direct.playpump_mode = false;
    bout->nexus.direct.transcode_latency = 0;

    switch (config->format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        /* Check if sample rate is supported */
        if ((config->sample_rate != 48000) && (config->sample_rate != 192000) &&
            (config->sample_rate != 44100) && (config->sample_rate != 176400))
            return -EINVAL;

        if (config->channel_mask != AUDIO_CHANNEL_OUT_STEREO)
            return -EINVAL;
        break;
    case AUDIO_FORMAT_AC3:
        if (property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE, false) ||
            property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE_PERSIST, false)) {
            NEXUS_AudioCapabilities audioCaps;

            NEXUS_GetAudioCapabilities(&audioCaps);
            if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3].decode) {
                ALOGI("Enable play pump mode");
                bout->nexus.direct.playpump_mode = true;
            }
        }

        if (!bout->nexus.direct.playpump_mode) {
            /* Suggest PCM wrapped format */
            config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            config->format = AUDIO_FORMAT_PCM_16_BIT;
            return -EINVAL;
        }
        break;
    case AUDIO_FORMAT_E_AC3:
        if (property_get_bool(BRCM_PROPERTY_AUDIO_OUTPUT_ENABLE_SPDIF_DOLBY, false) ||
            property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE, false) ||
            property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE_PERSIST, false)) {
            NEXUS_AudioCapabilities audioCaps;

            NEXUS_GetAudioCapabilities(&audioCaps);
            if (audioCaps.dsp.codecs[NEXUS_AudioCodec_eAc3Plus].decode) {
                ALOGI("Enable play pump mode");
                bout->nexus.direct.playpump_mode = true;
                bout->nexus.direct.transcode_latency = property_get_int32(
                                BRCM_PROPERTY_AUDIO_OUTPUT_EAC3_TRANS_LATENCY,
                                BRCM_AUDIO_DIRECT_EAC3_TRANS_LATENCY);
            }
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

    /* Open Nexus simple decoder */
    rc = brcm_audio_client_join(BRCM_AUDIO_DIRECT_NXCLIENT_NAME);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("%s: brcm_audio_client_join error, rc:%d", __FUNCTION__, rc);
        return -ENOSYS;
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
        if (property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_FORCE_PCM, false) ||
            property_get_bool(BRCM_PROPERTY_AUDIO_DIRECT_FORCE_PCM_PERSIST, false)) {
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
    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

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
    NxClient_Uninit();

    return ret;
}

static int nexus_direct_bout_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

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
    NxClient_Uninit();
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

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
    .do_bout_dump = NULL,
    .do_bout_get_parameters = nexus_direct_bout_get_parameters,
    .do_bout_pause = nexus_direct_bout_pause,
    .do_bout_resume = nexus_direct_bout_resume,
    .do_bout_drain = NULL,
    .do_bout_flush = nexus_direct_bout_flush,
};
