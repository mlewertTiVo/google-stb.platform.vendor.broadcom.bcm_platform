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

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   48000
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT

#define NEXUS_OUT_BUFFER_DURATION_MS    10

#define BRCM_AUDIO_DIRECT_NXCLIENT_NAME "BrcmAudioOutDirect"


/*
 * Utility Functions
 */

static void nexus_direct_bout_data_callback(void *param1, int param2)
{
    UNUSED(param1);

    BKNI_SetEvent((BKNI_EventHandle)param2);
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

static int nexus_direct_bout_get_render_position(struct brcm_stream_out *bout, uint32_t *dsp_frames)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.simple_decoder;
    NEXUS_AudioDecoderStatus status;

    if (simple_decoder) {
       NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
       *dsp_frames = status.numBytesDecoded/bout->frameSize;
    } else {
       *dsp_frames = 0;
    }
    return 0;
}


static int nexus_direct_bout_get_presentation_position(struct brcm_stream_out *bout, uint64_t *frames)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.simple_decoder;
    NEXUS_AudioDecoderStatus status;

    if (simple_decoder) {
       NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
       *frames = (uint64_t)(bout->framesPlayed + status.numBytesDecoded/bout->frameSize);
    } else {
       *frames =0;
    }
    return 0;
}
static char *nexus_direct_bout_get_parameters (struct brcm_stream_out *bout, const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();
    char* result_str;

    pthread_mutex_lock(&bout->lock);
    /* Get real parameters here!!! */
    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          "48000");
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          "AUDIO_CHANNEL_OUT_STEREO");
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          "AUDIO_FORMAT_PCM_16_BIT");
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
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.simple_decoder;
    NEXUS_SimpleAudioDecoderStartSettings start_settings;
    BKNI_EventHandle event = bout->nexus.event;
    struct audio_config *config = &bout->config;
    int ret = 0;

    if (bout->suspended || !simple_decoder) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&start_settings);

    start_settings.passthroughBuffer.enabled = true;
    start_settings.passthroughBuffer.sampleRate = 48000;
    start_settings.passthroughBuffer.dataCallback.callback = nexus_direct_bout_data_callback;
    start_settings.passthroughBuffer.dataCallback.context = bout;
    start_settings.passthroughBuffer.dataCallback.param = (int)event;

    ret = NEXUS_SimpleAudioDecoder_Start(simple_decoder,
                                          &start_settings);
    if (ret) {
        ALOGE("%s: at %d, start decoder failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }
    return 0;
}

static int nexus_direct_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.simple_decoder;

    if (simple_decoder) {
        NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
        NEXUS_AudioDecoderStatus status;
        NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
        bout->framesPlayed += status.numBytesDecoded/bout->frameSize;
        ALOGV("%s: setting framesPlayed to %u", __FUNCTION__, bout->framesPlayed);
    }
    return 0;
}

static int nexus_direct_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioDecoderHandle simple_decoder = bout->nexus.simple_decoder;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;

    if (bout->suspended || !simple_decoder) {
        ALOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;

        ret = NEXUS_SimpleAudioDecoder_GetPassthroughBuffer(simple_decoder,
                                                  &nexus_buffer, &nexus_space);
        if (ret) {
            ALOGE("%s: at %d, get decoder passthrough buffer failed, ret = %d\n",
                 __FUNCTION__, __LINE__, ret);
            break;
        }

        if (nexus_space) {
            size_t bytes_to_copy;

            bytes_to_copy = (bytes <= nexus_space) ? bytes : nexus_space;
            memcpy(nexus_buffer,
                   (void *)((int)buffer + bytes_written),
                   bytes_to_copy);

            ret = NEXUS_SimpleAudioDecoder_PassthroughWriteComplete(simple_decoder,
                                                          bytes_to_copy);
            if (ret) {
                ALOGE("%s: at %d, commit decoder passthrough buffer failed, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;
        }
        else {
            NEXUS_SimpleAudioDecoderHandle prev_simple_decoder = simple_decoder;

            pthread_mutex_unlock(&bout->lock);
            ret = BKNI_WaitForEvent(event, 500);
            pthread_mutex_lock(&bout->lock);

            // Sanity check when relocking
            simple_decoder = bout->nexus.simple_decoder;
            ALOG_ASSERT(simple_decoder == prev_simple_decoder);
            ALOG_ASSERT(!bout->suspended);

            if (ret) {
                ALOGE("%s: at %d, decoder timeout, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);

                /* Stop decoder */
                NEXUS_SimpleAudioDecoder_Stop(simple_decoder);
                bout->started = false;
                break;
            }
        }
    }

    /* Return error if no bytes written */
    if (bytes_written == 0) {
        return ret;
    }

    /* Remove audio delay */
    for (;;) {
        NEXUS_AudioDecoderStatus status;

        NEXUS_SimpleAudioDecoder_GetStatus(simple_decoder, &status);
        if (status.queuedFrames == 0) {
            break;
        }
        BKNI_Sleep(10);
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
    NEXUS_Error rc;
    android::status_t status;
    NxClient_AllocSettings allocSettings;
    NEXUS_SurfaceComposition surfSettings;
    NxClient_ConnectSettings connectSettings;
    BKNI_EventHandle event;
    uint32_t audioDecoderId;
    int i, ret = 0;

    /* Check if sample rate is supported */
    config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    config->format = NEXUS_OUT_DEFAULT_FORMAT;

    bout->framesPlayed = 0;
    bout->frameSize = audio_bytes_per_sample(config->format) * popcount(config->channel_mask);
    bout->buffer_size =
        get_brcm_audio_buffer_size(config->sample_rate,
                                   config->format,
                                   popcount(config->channel_mask),
                                   NEXUS_OUT_BUFFER_DURATION_MS);

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
        ALOGE("%s: at %d, acquire Nexus simple decoder handle failed\n",
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
        ALOGE("%s: at %d, create event failed, ret = %d\n",
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

    bout->nexus.simple_decoder = simple_decoder;
    bout->nexus.event = event;
    bout->nexus.state = BRCM_NEXUS_STATE_CREATED;

    return 0;

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

    nexus_direct_bout_stop(bout);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    bout->bdev->standbyThread->UnregisterCallback(bout->standbyCallback);
    bout->nexus.simple_decoder = NULL;

    NxClient_Disconnect(bout->nexus.connectId);
    NxClient_Free(&(bout->nexus.allocResults));
    NxClient_Uninit();
    bout->nexus.state = BRCM_NEXUS_STATE_DESTROYED;

    return 0;
}

struct brcm_stream_out_ops nexus_direct_bout_ops = {
    .do_bout_open = nexus_direct_bout_open,
    .do_bout_close = nexus_direct_bout_close,
    .do_bout_start = nexus_direct_bout_start,
    .do_bout_stop = nexus_direct_bout_stop,
    .do_bout_write = nexus_direct_bout_write,
    .do_bout_set_volume = nexus_direct_bout_set_volume,
    .do_bout_get_render_position = nexus_direct_bout_get_render_position,
    .do_bout_get_presentation_position = nexus_direct_bout_get_presentation_position,
    .do_bout_dump = NULL,
    .do_bout_get_parameters = nexus_direct_bout_get_parameters,
    .do_bout_pause = NULL,
    .do_bout_resume = NULL,
    .do_bout_drain = NULL,
    .do_bout_flush = NULL,
};
