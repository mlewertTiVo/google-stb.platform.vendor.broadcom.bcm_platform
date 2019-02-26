/******************************************************************************
 *    (c)2015 Broadcom Corporation
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
#include <inttypes.h>

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   44100
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

#define NEXUS_OUT_BUFFER_DURATION_MS    20
#define NEXUS_OUT_DEFAULT_LATENCY       10
#define NEXUS_OUT_MS_LATENCY            80

/* Supported stream out sample rate */
const static uint32_t nexus_out_sample_rates[] = {
    8000,
    11025,
    16000,
    22050,
    24000,
    32000,
    44100,
    48000,
    88200,
    96000,
    176400,
    192000
};
#define NEXUS_OUT_SAMPLE_RATE_COUNT                     \
    (sizeof(nexus_out_sample_rates) / sizeof(uint32_t))

#define NXCLIENT_SERVER_TIMEOUT_IN_MS (500)
#define BRCM_AUDIO_NXCLIENT_NAME "BrcmAudioOut"

/* Volume related constants */

#define AUDIO_VOLUME_SETTING_MIN (0)
#define AUDIO_VOLUME_SETTING_MAX (100)
/****************************************************
* Volume Table in dB, Mapping as linear attenuation *
****************************************************/
static uint32_t Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX+1] =
{
    0,      9,      17,     26,     35,
    45,     54,     63,     72,     82,
    92,     101,    111,    121,    131,
    141,    151,    162,    172,    183,
    194,    205,    216,    227,    239,
    250,    262,    273,    285,    297,
    310,    322,    335,    348,    361,
    374,    388,    401,    415,    429,
    444,    458,    473,    488,    504,
    519,    535,    551,    568,    585,
    602,    620,    638,    656,    674,
    694,    713,    733,    754,    774,
    796,    818,    840,    864,    887,
    912,    937,    963,    990,    1017,
    1046,   1075,   1106,   1137,   1170,
    1204,   1240,   1277,   1315,   1356,
    1398,   1442,   1489,   1539,   1592,
    1648,   1708,   1772,   1842,   1917,
    2000,   2092,   2194,   2310,   2444,
    2602,   2796,   3046,   3398,   4000,
    9000
};

/*
 * Utility Functions
 */

static void nexus_bout_data_callback(void *param1, int param2)
{
    UNUSED(param1);

    BKNI_SetEvent((BKNI_EventHandle)(intptr_t)param2);
}

NEXUS_Error brcm_audio_client_join(void)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_JoinSettings joinSettings;
    NEXUS_PlatformStatus status;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    do {
        rc = NxClient_Join(&joinSettings);
        if (rc != NEXUS_SUCCESS) {
            ALOGW("%s: NxServer is not ready, waiting...", __FUNCTION__);
            usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
        }
        else {
            ALOGD("%s: NxClient_Join succeeded.", __FUNCTION__);
        }
    } while (rc != NEXUS_SUCCESS);

    return rc;
}

void brcm_audio_set_audio_volume(float leftVol, float rightVol)
{
    NEXUS_Error rc;
    NxClient_AudioSettings settings;
    int32_t leftVolume;
    int32_t rightVolume;

    ALOGV("nexus_nx_client %s:%d left=%f right=%f\n",__PRETTY_FUNCTION__,__LINE__,leftVol,rightVol);

    leftVolume = leftVol*AUDIO_VOLUME_SETTING_MAX;
    rightVolume = rightVol*AUDIO_VOLUME_SETTING_MAX;

    /* Check for boundary */
    if (leftVolume > AUDIO_VOLUME_SETTING_MAX)
        leftVolume = AUDIO_VOLUME_SETTING_MAX;
    if (leftVolume < AUDIO_VOLUME_SETTING_MIN)
        leftVolume = AUDIO_VOLUME_SETTING_MIN;
    if (rightVolume > AUDIO_VOLUME_SETTING_MAX)
        rightVolume = AUDIO_VOLUME_SETTING_MAX;
    if (rightVolume < AUDIO_VOLUME_SETTING_MIN)
        rightVolume = AUDIO_VOLUME_SETTING_MIN;

    do {
        NxClient_GetAudioSettings(&settings);
        settings.volumeType = NEXUS_AudioVolumeType_eDecibel;
        settings.leftVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-leftVolume];
        settings.rightVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-rightVolume];
        rc = NxClient_SetAudioSettings(&settings);
    } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);
}

static int brcm_audio_lookup_db_ix(uint32_t volume, int index, int min, int max)
{
    ALOGV("%s: volume=%d index=%d min=%d max=%d", __FUNCTION__, volume, index, min, max);

    if (index < min || index > max)
    {
        ALOGV("%s: exceeded range!", __FUNCTION__);
        return 0;
    }

    if (volume == Gemini_VolTable[index])
    {
        ALOGV("%s: found volume at index=%d", __FUNCTION__, index);
        return index;
    }
    else if (volume > Gemini_VolTable[index])
    {
        int new_index = index + (max - index)/2 + 1;
        return brcm_audio_lookup_db_ix(volume, new_index, index + 1, max);
    }
    else
    {
        int new_index = index - (index - min)/2 - 1;
        return brcm_audio_lookup_db_ix(volume, new_index, min, index - 1);
    }
}

void brcm_audio_set_mute_state(bool mute)
{
    NEXUS_Error rc;
    NxClient_AudioSettings settings;

    ALOGV("nexus_nx_client %s:%d mute=%s\n",__PRETTY_FUNCTION__,__LINE__,mute ? "true":"false");

    do {
        NxClient_GetAudioSettings(&settings);
        settings.muted = mute;
        rc = NxClient_SetAudioSettings(&settings);
    } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);
}

bool brcm_audio_get_mute_state(void)
{
    NxClient_AudioSettings settings;
    bool mute = false;

    NxClient_GetAudioSettings(&settings);
    mute = settings.muted;

    return mute;
}

void brcm_audio_set_master_volume(float volume)
{
    brcm_audio_set_audio_volume(volume, volume);
}

float brcm_audio_get_master_volume(void)
{
    NxClient_AudioSettings settings;
    float master_volume = 0;
    int volume_index;

    NxClient_GetAudioSettings(&settings);

    /* volume type must be decibel and left/right volume must be equal */
    if (settings.volumeType == NEXUS_AudioVolumeType_eDecibel &&
        settings.leftVolume == settings.rightVolume)
    {
        /* convert from decibel to range from 0-99 */
        volume_index = brcm_audio_lookup_db_ix(-settings.leftVolume, AUDIO_VOLUME_SETTING_MAX/2, 0, AUDIO_VOLUME_SETTING_MAX);

        /* normalize between 0 to 1.0 */
        master_volume = ((float)(AUDIO_VOLUME_SETTING_MAX - volume_index))/AUDIO_VOLUME_SETTING_MAX;
    }

    ALOGV("%s: master_volume=%f", __FUNCTION__, master_volume);
    return master_volume;
}

NEXUS_AudioCodec brcm_audio_get_codec_from_format(audio_format_t format)
{
    switch (format & AUDIO_FORMAT_MAIN_MASK) {
        case AUDIO_FORMAT_PCM:      return NEXUS_AudioCodec_ePcmWav;
        case AUDIO_FORMAT_AC3:      return NEXUS_AudioCodec_eAc3;
        case AUDIO_FORMAT_E_AC3:    return NEXUS_AudioCodec_eAc3Plus;
        case AUDIO_FORMAT_DTS:      return NEXUS_AudioCodec_eDts;
        case AUDIO_FORMAT_DTS_HD:   return NEXUS_AudioCodec_eDtsHd;
        default:                    break;
    }
    return NEXUS_AudioCodec_eUnknown;
}

void brcm_audio_set_audio_clock_accuracy(void)
{
    NEXUS_Error rc;
    NxClient_AudioSettings settings;
    int32_t value;
    value = property_get_int32(BCM_RO_AUDIO_OUTPUT_CLOCK_ACCURACY, 0);

    ALOGV("nexus_nx_client %s:%d clock_acc=%d\n",__PRETTY_FUNCTION__,__LINE__,value);

    do {
        NxClient_GetAudioSettings(&settings);
        if ((settings.hdmi.channelStatusInfo.clockAccuracy == (NEXUS_AudioChannelStatusClockAccuracy)value) &&
            (settings.spdif.channelStatusInfo.clockAccuracy == (NEXUS_AudioChannelStatusClockAccuracy)value))
            return;
        settings.hdmi.channelStatusInfo.clockAccuracy = (NEXUS_AudioChannelStatusClockAccuracy)value;
        settings.spdif.channelStatusInfo.clockAccuracy = (NEXUS_AudioChannelStatusClockAccuracy)value;
        rc = NxClient_SetAudioSettings(&settings);
    } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);
}

/*
 * Operation Functions
 */

static int nexus_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    (void)bout;
    brcm_audio_set_audio_volume(left, right);
    return 0;
}

static int nexus_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;
    NEXUS_SimpleAudioPlaybackStatus status;
    NEXUS_SimpleAudioPlayback_GetStatus(simple_playback, &status);

    if(status.started) {
        bout->framesPlayed += (status.playedBytes - bout->nexus.primary.lastPlayedBytes) / bout->frameSize;
        bout->nexus.primary.lastPlayedBytes = status.playedBytes;

        if (bout->latencyPad) {
            if ((bout->framesPlayedTotal + bout->framesPlayed) > bout->latencyPad)
                bout->latencyPad = 0;
        }
        if (bout->latencyPad)
            *dsp_frames = 0;
        else
            *dsp_frames = (uint32_t)bout->framesPlayed - bout->latencyEstimate;
    }
    else {
        *dsp_frames = 0;
    }
    return 0;
}


static int nexus_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;
    NEXUS_SimpleAudioPlaybackStatus status;

    NEXUS_SimpleAudioPlayback_GetStatus(simple_playback, &status);

    if(status.started) {
        bout->framesPlayed += (status.playedBytes - bout->nexus.primary.lastPlayedBytes) / bout->frameSize;
        bout->nexus.primary.lastPlayedBytes = status.playedBytes;

        if (bout->latencyPad) {
            if ((bout->framesPlayedTotal + bout->framesPlayed) > bout->latencyPad)
                bout->latencyPad = 0;
        }
        if (bout->latencyPad)
            *frames = 0;
        else
            *frames = bout->framesPlayedTotal + bout->framesPlayed - bout->latencyEstimate;
    }
    else {
        *frames = 0;
    }
    return 0;
}

static int nexus_bout_start(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;
    NEXUS_SimpleAudioPlaybackStartSettings start_settings;
    BKNI_EventHandle event = bout->nexus.event;
    struct audio_config *config = &bout->config;
    int ret = 0;

    if (bout->suspended || !simple_playback) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    NEXUS_SimpleAudioPlayback_GetDefaultStartSettings(&start_settings);

    start_settings.bitsPerSample =
        (config->format == AUDIO_FORMAT_PCM_8_BIT) ? 8 : 16;
    start_settings.signedData =
        (config->format == AUDIO_FORMAT_PCM_8_BIT) ? false : true;
    start_settings.stereo =
        (config->channel_mask == AUDIO_CHANNEL_OUT_MONO) ? false : true;
    start_settings.sampleRate = config->sample_rate;

    NEXUS_CallbackDesc_Init(&start_settings.dataCallback);
    start_settings.dataCallback.callback = nexus_bout_data_callback;
    start_settings.dataCallback.context = bout;
    start_settings.dataCallback.param = (int)(intptr_t)event;

    start_settings.startThreshold = property_get_int32(BCM_RO_NX_AP_START_THRESHOLD, 8192);

    ret = NEXUS_SimpleAudioPlayback_Start(simple_playback,
                                          &start_settings);
    if (ret) {
        ALOGE("%s: at %d, start playback failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }
    bout->nexus.primary.lastPlayedBytes = 0;
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;

    if (simple_playback) {
        NEXUS_SimpleAudioPlayback_Stop(simple_playback);
        NEXUS_SimpleAudioPlaybackStatus status;
        NEXUS_SimpleAudioPlayback_GetStatus(simple_playback, &status);
        bout->framesPlayed += (status.playedBytes - bout->nexus.primary.lastPlayedBytes) / bout->frameSize;
        bout->nexus.primary.lastPlayedBytes = status.playedBytes;
        bout->framesPlayedTotal += bout->framesPlayed;
        ALOGV("%s: setting framesPlayedTotal to %" PRIu64 "", __FUNCTION__, bout->framesPlayedTotal);
    }

    bout->nexus.primary.lastPlayedBytes = 0;
    bout->framesPlayed = 0;

    return 0;
}

static int nexus_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;

    if (bout->suspended || !simple_playback) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;

        ret = NEXUS_SimpleAudioPlayback_GetBuffer(simple_playback,
                                                  &nexus_buffer, &nexus_space);
        if (ret) {
            ALOGE("%s: at %d, get playback buffer failed, ret = %d\n",
                 __FUNCTION__, __LINE__, ret);
            break;
        }

        if (nexus_space) {
            size_t bytes_to_copy;

            bytes_to_copy = (bytes <= nexus_space) ? bytes : nexus_space;
            memcpy(nexus_buffer,
                   (void *)((int)(intptr_t)buffer + bytes_written),
                   bytes_to_copy);

            ret = NEXUS_SimpleAudioPlayback_WriteComplete(simple_playback,
                                                          bytes_to_copy);
            if (ret) {
                ALOGE("%s: at %d, commit playback buffer failed, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);
                ret = -ENOSYS;
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;
        }
        else {
            NEXUS_SimpleAudioPlaybackHandle prev_simple_playback = simple_playback;

            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 500);
            pthread_mutex_lock(&bout->lock);

            // Sanity check when relocking
            simple_playback = bout->nexus.primary.simple_playback;
            ALOG_ASSERT(simple_playback == prev_simple_playback);
            ALOG_ASSERT(!bout->suspended);

            if (ret != BERR_SUCCESS) {
                ALOGE("%s: at %d, playback timeout, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);

                /* Stop playback */
                NEXUS_SimpleAudioPlayback_Stop(simple_playback);
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
static bool nexus_bout_standby_monitor(void *context)
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

static int nexus_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NEXUS_SimpleAudioPlaybackHandle simple_playback;
    NEXUS_Error rc;
    android::status_t status;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    BKNI_EventHandle event;
    uint32_t audioPlaybackId;
    int i, ret = 0;

    /* Check if sample rate is supported */
    for (i = 0; i < (int)NEXUS_OUT_SAMPLE_RATE_COUNT; i++) {
        if (config->sample_rate == nexus_out_sample_rates[i]) {
            break;
        }
    }
    if (i == NEXUS_OUT_SAMPLE_RATE_COUNT) {
        config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    }

    if (config->channel_mask == 0 ||
       ((config->channel_mask != AUDIO_CHANNEL_OUT_STEREO) && (config->channel_mask != AUDIO_CHANNEL_OUT_MONO))) {
        config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    }

    config->format = NEXUS_OUT_DEFAULT_FORMAT;

    bout->framesPlayedTotal = 0;
    bout->latencyEstimate = (property_get_int32(BCM_RO_AUDIO_OUTPUT_MIXER_LATENCY,
                                                ((bout->bdev->dolby_ms == 11) || (bout->bdev->dolby_ms == 12)) ?
                                                    NEXUS_DEFAULT_MS_MIXER_LATENCY : 0)
                             * bout->config.sample_rate) / 1000;
    bout->latencyPad = bout->latencyEstimate;

    bout->frameSize = audio_bytes_per_sample(config->format) * popcount(config->channel_mask);

    bout->buffer_size =
        get_brcm_audio_buffer_size(config->sample_rate,
                                   config->format,
                                   popcount(config->channel_mask),
                                   property_get_int32(BCM_PERSIST_AUDIO_BUFFER_DURATION, NEXUS_OUT_BUFFER_DURATION_MS));

    /* Force PCM mode for MS11 */
    if (bout->bdev->dolby_ms == 11) {
        NxClient_AudioSettings audioSettings;

        ALOGI("Force PCM output");
        NxClient_GetAudioSettings(&audioSettings);
        audioSettings.hdmi.outputMode = NxClient_AudioOutputMode_ePcm;
        audioSettings.spdif.outputMode = NxClient_AudioOutputMode_ePcm;
        ret = NxClient_SetAudioSettings(&audioSettings);
        if (ret) {
            ALOGE("%s: Error setting PCM mode, ret = %d", __FUNCTION__, ret);
        }
    }

    /* Allocate simpleAudioPlayback */
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleAudioPlayback = 1;
    rc = NxClient_Alloc(&allocSettings, &(bout->nexus.allocResults));
    if (rc) {
        ALOGE("%s: Cannot allocate NxClient resources, rc:%d", __FUNCTION__, rc);
        ret = -ENOSYS;
        goto err_alloc;
    }

    audioPlaybackId = bout->nexus.allocResults.simpleAudioPlayback[0].id;
    simple_playback = NEXUS_SimpleAudioPlayback_Acquire(audioPlaybackId);
    if (!simple_playback) {
        ALOGE("%s: at %d, acquire Nexus simple plackback handle failed\n",
             __FUNCTION__, __LINE__);
        ret = -ENOSYS;
        goto err_acquire;
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleAudioPlayback[0].id = audioPlaybackId;
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
        ret = -ENOSYS;
        goto err_event;
    }

    // register standby callback
    bout->standbyCallback = bout->bdev->standbyThread->RegisterCallback(nexus_bout_standby_monitor, bout);
    if (bout->standbyCallback < 0) {
        ALOGE("Error registering standby callback");
        ret = -ENOSYS;
        goto err_callback;
    }

    bout->nexus.primary.simple_playback = simple_playback;
    bout->nexus.primary.lastPlayedBytes = 0;
    bout->nexus.event = event;

    return 0;

err_callback:
    BKNI_DestroyEvent(event);
err_event:
    NxClient_Disconnect(bout->nexus.connectId);
err_acquire:
    NxClient_Free(&(bout->nexus.allocResults));
err_alloc:
    return ret;
}

static int nexus_bout_close(struct brcm_stream_out *bout)
{
    BKNI_EventHandle event = bout->nexus.event;

    nexus_bout_stop(bout);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));

    return 0;
}

static int nexus_bout_dump(struct brcm_stream_out *bout, int fd)
{
    (void)bout;
    (void)fd;

    return 0;
}

static uint32_t nexus_bout_get_latency(struct brcm_stream_out *bout)
{
    /* The reported latency affects default buffer sizes used in AudioFlinger's audio tracks.
     * Most applications specify sizes based on application needs.  However if OpenSL is used,
     * the default size is used.  For MS enabled devices, the buffer size needs to be increased
     * to prevent the AudioTrack from underrunning. */
    return property_get_int32(BCM_RO_AUDIO_OUTPUT_LATENCY,
                              ((bout->bdev->dolby_ms == 11) || (bout->bdev->dolby_ms == 12)) ?
                                  NEXUS_OUT_MS_LATENCY :
                                  NEXUS_OUT_DEFAULT_LATENCY);
}

static int nexus_bout_get_next_write_timestamp(struct brcm_stream_out *bout, int64_t *timestamp)
{
    NEXUS_SimpleAudioPlaybackStatus status;
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.primary.simple_playback;
    NEXUS_SimpleAudioPlayback_GetStatus(simple_playback, &status);

    if (status.started) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        *timestamp = (int64_t)ts.tv_sec * 1000000ll + (ts.tv_nsec)/1000ll;
    }
    else {
        *timestamp = 0;
    }
    return 0;
}

static char *nexus_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    char* result_str;
    (void) bout;

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS, "AUDIO_FORMAT_PCM_16_BIT");
    }

    result_str = str_parms_to_str(result);
    ALOGV("%s: result = %s", __FUNCTION__, result_str);
    return result_str;
}

struct brcm_stream_out_ops nexus_bout_ops = {
    .do_bout_open = nexus_bout_open,
    .do_bout_close = nexus_bout_close,
    .do_bout_get_latency = nexus_bout_get_latency,
    .do_bout_start = nexus_bout_start,
    .do_bout_stop = nexus_bout_stop,
    .do_bout_write = nexus_bout_write,
    .do_bout_set_volume = nexus_bout_set_volume,
    .do_bout_get_render_position = nexus_bout_get_render_position,
    .do_bout_get_presentation_position = nexus_bout_get_presentation_position,
    .do_bout_dump = nexus_bout_dump,
    .do_bout_get_parameters = nexus_bout_get_parameters,
    .do_bout_pause = NULL,
    .do_bout_resume = NULL,
    .do_bout_drain = NULL,
    .do_bout_flush = NULL,
    .do_bout_get_next_write_timestamp = nexus_bout_get_next_write_timestamp,
};
