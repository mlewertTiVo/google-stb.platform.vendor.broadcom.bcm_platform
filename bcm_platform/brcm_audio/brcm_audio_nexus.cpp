/******************************************************************************
 *    (c)2014 Broadcom Corporation
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
 * $brcm_Workfile: AudioHardware.cpp $
 * $brcm_Revision:  $
 * $brcm_Date: 08/08/14 12:05p $
 * $brcm_Author: zhang@broadcom.com
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log:  $
 *
 *****************************************************************************/

#include "brcm_audio.h"

#undef LOG_TAG
#define LOG_TAG "BrcmAudioNexus"

#define NEXUS_OUT_DEFAULT_SAMPLE_RATE   48000
#define NEXUS_OUT_DEFAULT_CHANNELS      AUDIO_CHANNEL_OUT_STEREO
#define NEXUS_OUT_DEFAULT_FORMAT        AUDIO_FORMAT_PCM_16_BIT
#define NEXUS_OUT_DEFAULT_BUFFER_SIZE   8192

/*
 * Utility Functions
 */

static void nexus_bout_data_callback(void *param1, int param2)
{
    UNUSED(param1);

    BKNI_SetEvent((BKNI_EventHandle)param2);
}

/*
 * Operation Functions
 */

static int nexus_bout_set_volume(struct brcm_stream_out *bout,
                                 float left, float right)
{
    NexusIPCClientBase *ipc_client = bout->nexus.ipc_client;

    ipc_client->setAudioVolume(left, right);
    return 0;
}

static int nexus_bout_start(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.simple_playback;
    NEXUS_SimpleAudioPlaybackStartSettings start_settings;
    BKNI_EventHandle event = bout->nexus.event;
    struct audio_config *config = &bout->config;
    int ret = 0;

    if (bout->suspended || !simple_playback) {
        LOGE("%s: at %d, device not open\n",
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

    start_settings.dataCallback.callback = nexus_bout_data_callback;
    start_settings.dataCallback.context = bout;
    start_settings.dataCallback.param = (int)event;

    start_settings.startThreshold = 128;

    ret = NEXUS_SimpleAudioPlayback_Start(simple_playback,
                                          &start_settings);
    if (ret) {
        LOGE("%s: at %d, start playback failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        return -ENOSYS;
    }
    return 0;
}

static int nexus_bout_stop(struct brcm_stream_out *bout)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.simple_playback;

    if (simple_playback) {
        NEXUS_SimpleAudioPlayback_Stop(simple_playback);
    }
    return 0;
}

static int nexus_bout_write(struct brcm_stream_out *bout,
                            const void* buffer, size_t bytes)
{
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.simple_playback;
    BKNI_EventHandle event = bout->nexus.event;
    size_t bytes_written = 0;
    int ret = 0;

    if (bout->suspended || !simple_playback) {
        LOGE("%s: at %d, device not open\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    while (bytes > 0) {
        void *nexus_buffer;
        size_t nexus_space;

        ret = NEXUS_SimpleAudioPlayback_GetBuffer(simple_playback,
                                                  &nexus_buffer, &nexus_space);
        if (ret) {
            LOGE("%s: at %d, get playback buffer failed, ret = %d\n",
                 __FUNCTION__, __LINE__, ret);
            break;
        }

        if (nexus_space) {
            size_t bytes_to_copy;

            bytes_to_copy = (bytes <= nexus_space) ? bytes : nexus_space;
            memcpy(nexus_buffer,
                   (void *)((int)buffer + bytes_written),
                   bytes_to_copy);

            ret = NEXUS_SimpleAudioPlayback_WriteComplete(simple_playback,
                                                          bytes_to_copy);
            if (ret) {
                LOGE("%s: at %d, commit playback buffer failed, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);
                break;
            }
            bytes_written += bytes_to_copy;
            bytes -= bytes_to_copy;
        }
        else {
            ret = BKNI_WaitForEvent(event, 500);
            if (ret) {
                LOGE("%s: at %d, playback timeout, ret = %d\n",
                     __FUNCTION__, __LINE__, ret);

                /* Stop playback */
                NEXUS_SimpleAudioPlayback_Stop(simple_playback);
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
        NEXUS_SimpleAudioPlaybackStatus status;

        NEXUS_SimpleAudioPlayback_GetStatus(simple_playback, &status);
        if (status.queuedBytes < bout->buffer_size * 4) {
            break;
        }
        BKNI_Sleep(10);
    }
    return bytes_written;
}

static int nexus_bout_open(struct brcm_stream_out *bout)
{
    struct audio_config *config = &bout->config;
    NexusIPCClientBase *ipc_client = NULL;
    NexusClientContext *nexus_client = NULL;
    NEXUS_SimpleAudioPlaybackHandle simple_playback;
    b_refsw_client_client_configuration client_config;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings client_settings;
    BKNI_EventHandle event;
    int ret = 0;

    /* Only allow default config */
    config->sample_rate = NEXUS_OUT_DEFAULT_SAMPLE_RATE;
    config->channel_mask = NEXUS_OUT_DEFAULT_CHANNELS;
    config->format = NEXUS_OUT_DEFAULT_FORMAT;
    bout->buffer_size = NEXUS_OUT_DEFAULT_BUFFER_SIZE;

    /* Open Nexus simple playback */
    ipc_client = NexusIPCClientFactory::getClient("AudioStreamOut");
    if (!ipc_client) {
        LOGE("%s: at %d, get Nexus IPC client failed\n",
             __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    memset(&client_config, 0, sizeof(client_config));
    strncpy(client_config.name.string, "AudioStreamOut",
            sizeof(client_config.name.string));
    client_config.resources.audioPlayback = true;

    nexus_client = ipc_client->createClientContext(&client_config);
    if (!nexus_client) {
        LOGE("%s: at %d, get Nexus client failed\n",
             __FUNCTION__, __LINE__);
        ret = -ENOSYS;
        goto err_ipc;
    }

    ipc_client->getClientInfo(nexus_client, &client_info);
    ipc_client->getDefaultConnectClientSettings(&client_settings);
    client_settings.simpleAudioPlayback[0].id = client_info.audioPlaybackId;

    ret = ipc_client->connectClientResources(nexus_client, &client_settings);
    if (ret == 0 /* return 0 on error */) {
        LOGE("%s: at %d, connet Nexus client resources failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -ENOSYS;
        goto err_client;
    }

    simple_playback = ipc_client->acquireAudioPlaybackHandle();
    if (!simple_playback) {
        LOGE("%s: at %d, acquire Nexus simple plackback handle failed\n",
             __FUNCTION__, __LINE__);
        ret = -ENOSYS;
        goto err_client;
    }


    ret = BKNI_CreateEvent(&event);
    if (ret) {
        LOGE("%s: at %d, create event failed, ret = %d\n",
             __FUNCTION__, __LINE__, ret);
        ret = -ENOSYS;
        goto err_playback;
    }

    bout->nexus.ipc_client = ipc_client;
    bout->nexus.nexus_client = nexus_client;
    bout->nexus.simple_playback = simple_playback;
    bout->nexus.event = event;

    return 0;

 err_playback:
    ipc_client->releaseAudioPlaybackHandle(simple_playback);

 err_client:
    ipc_client->disconnectClientResources(nexus_client);
    ipc_client->destroyClientContext(nexus_client);

 err_ipc:
    delete ipc_client;
    return ret;
}

static int nexus_bout_close(struct brcm_stream_out *bout)
{
    NexusIPCClientBase *ipc_client = bout->nexus.ipc_client;
    NexusClientContext *nexus_client = bout->nexus.nexus_client;
    NEXUS_SimpleAudioPlaybackHandle simple_playback = bout->nexus.simple_playback;
    BKNI_EventHandle event = bout->nexus.event;

    nexus_bout_stop(bout);

    if (event) {
        BKNI_DestroyEvent(event);
    }

    if (simple_playback) {
        ipc_client->releaseAudioPlaybackHandle(simple_playback);
    }

    if (nexus_client) {
        ipc_client->disconnectClientResources(nexus_client);
        ipc_client->destroyClientContext(nexus_client);
    }

    if (ipc_client) {
        delete ipc_client;
    }

    return 0;
}

struct brcm_stream_out_ops nexus_bout_ops = {
    .do_bout_open = nexus_bout_open,
    .do_bout_close = nexus_bout_close,
    .do_bout_start = nexus_bout_start,
    .do_bout_stop = nexus_bout_stop,
    .do_bout_write = nexus_bout_write,
    .do_bout_set_volume = nexus_bout_set_volume,
    .do_bout_dump = NULL
};
