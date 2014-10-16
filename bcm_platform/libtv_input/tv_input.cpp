/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/native_handle.h>

#include <hardware/tv_input.h>

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "nxclient.h"
#include "nexus_frontend.h"
#include "nexus_parser_band.h"
#include "nexus_simple_video_decoder.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************/
typedef struct tv_input_private {
    tv_input_device_t device;

    // Callback related data
    const tv_input_callback_ops_t* callback;
    void* callback_data;

    NexusIPCClientBase *ipcclient;
    NexusClientContext *nexus_client;
    NEXUS_FrontendHandle frontend;
    NEXUS_ParserBand parserBand;
    NEXUS_SimpleStcChannelHandle stcChannel;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
} tv_input_private_t;

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device);

static struct hw_module_methods_t tv_input_module_methods = {
    open: tv_input_device_open
};

tv_input_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 0,
        version_minor: 1,
        id: TV_INPUT_HARDWARE_MODULE_ID,
        name: "Sample TV input module",
        author: "The Android Open Source Project",
        methods: &tv_input_module_methods,
    }
};

// Turn this ON for TUNER support
//#define ENABLE_TUNER

#ifdef ENABLE_TUNER
// native_handle_t related
#define NUM_FD      0
#define NUM_INT     2

// Some tuner values
#define FREQ        577
#define VIDEO_PID   0x100

static int tv_input_tuner_initialize(tv_input_private_t *priv);
static int tv_input_tuner_start(tv_input_private_t *priv);
#endif
/*****************************************************************************/

static int tv_input_initialize(struct tv_input_device* dev, const tv_input_callback_ops_t* callback, void* data)
{
	tv_input_event_t tuner_event;
    NEXUS_Error rc;

	ALOGE("%s: Enter", __FUNCTION__);

    if (dev == NULL || callback == NULL) {
        return -EINVAL;
    }

    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv->callback != NULL) {
        return -EEXIST;
    }

    // Setup the callback stuff
    priv->callback = callback;
    priv->callback_data = data;

    tuner_event.type = TV_INPUT_EVENT_DEVICE_AVAILABLE;
    tuner_event.device_info.type = TV_INPUT_TYPE_TUNER;
	tuner_event.device_info.device_id = 0;

	// No audio data for tuner
	tuner_event.device_info.audio_type = AUDIO_DEVICE_NONE;
	tuner_event.device_info.audio_address = NULL;

	ALOGE("%s: Calling notify", __FUNCTION__);
    callback->notify(dev, &tuner_event, data);

#ifdef ENABLE_TUNER
    rc = tv_input_tuner_initialize(priv);
    ALOGE("%s: Exit (rc = %d)", __FUNCTION__, rc);
    return rc;
#else
    return 0;
#endif
}

static int tv_input_get_stream_configurations(const struct tv_input_device *dev, int dev_id, int *numConfigs, const tv_stream_config_t **s_out)
{
    tv_stream_config_t *s_cfg;

	ALOGE("%s: Enter", __FUNCTION__);

    *numConfigs = 1;

    s_cfg = (tv_stream_config_t *) malloc(sizeof (tv_stream_config_t));

    s_cfg->stream_id = 1001;
    s_cfg->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
    s_cfg->max_video_width = 1920;
    s_cfg->max_video_height = 1080;

    *s_out = s_cfg;

    return -0;
}

static int tv_input_open_stream(struct tv_input_device *dev, int dev_id, tv_stream_t *pTVStream)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;

	ALOGE("%s: stream_id = %d", __FUNCTION__, pTVStream->stream_id);

#ifdef ENABLE_TUNER
    // Create a native handle
    pTVStream->sideband_stream_source_handle = native_handle_create(NUM_FD, NUM_INT);

    // Setup the native handle data
    pTVStream->sideband_stream_source_handle->data[0] = 1;
    pTVStream->sideband_stream_source_handle->data[1] = (int)(priv->nexus_client);
    pTVStream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;

    ALOGE("%s: nexus_client = %p", __FUNCTION__, priv->nexus_client);

    return tv_input_tuner_start(priv);
#else
    return 0;
#endif
}

static int tv_input_close_stream(struct tv_input_device*, int, int)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return -EINVAL;
}

static int tv_input_request_capture(
        struct tv_input_device*, int, int, buffer_handle_t, uint32_t)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return -EINVAL;
}

static int tv_input_cancel_capture(struct tv_input_device*, int, int, uint32_t)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return -EINVAL;
}

/*****************************************************************************/

static int tv_input_device_close(struct hw_device_t *dev)
{
	ALOGE("%s: Enter", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv) {
        free(priv);
    }
    return 0;
}

/*****************************************************************************/

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device)
{
	ALOGE("%s: Enter", __FUNCTION__);

    int status = -EINVAL;
    if (!strcmp(name, TV_INPUT_DEFAULT_DEVICE)) {
        tv_input_private_t* dev = (tv_input_private_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = TV_INPUT_DEVICE_API_VERSION_0_1;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = tv_input_device_close;

        dev->device.initialize = tv_input_initialize;
        dev->device.get_stream_configurations =
                tv_input_get_stream_configurations;
        dev->device.open_stream = tv_input_open_stream;
        dev->device.close_stream = tv_input_close_stream;
        dev->device.request_capture = tv_input_request_capture;
        dev->device.cancel_capture = tv_input_cancel_capture;

        *device = &dev->device.common;
        status = 0;
    }
    return status;
}

#ifdef ENABLE_TUNER
static int tv_input_tuner_initialize(tv_input_private_t *priv)
{
    NEXUS_FrontendAcquireSettings frontendAcquireSettings;

    ALOGE("%s: Enter", __FUNCTION__);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;

    priv->ipcclient= NexusIPCClientFactory::getClient("tv-input_hal");

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string), "tv-input_hal");

    config.resources.screen.required = true;
    config.resources.audioDecoder = false;
    config.resources.audioPlayback = false;
    config.resources.videoDecoder = true;

    priv->nexus_client = priv->ipcclient->createClientContext(&config);
    if (priv->nexus_client == NULL)
        ALOGE("%s: createClientContext failed", __FUNCTION__);

    priv->videoDecoder = priv->ipcclient->acquireVideoDecoderHandle();

    priv->ipcclient->getClientInfo(priv->nexus_client, &client_info);

    // Now connect the client resources
    priv->ipcclient->getDefaultConnectClientSettings(&connectSettings);

    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = 0;

    connectSettings.simpleVideoDecoder[0].windowCaps.type = eVideoWindowType_eMain;

    if (true != priv->ipcclient->connectClientResources(priv->nexus_client, &connectSettings))
        ALOGE("%s: connectClientResources failed", __FUNCTION__);

    b_video_window_settings window_settings;
    priv->ipcclient->getVideoWindowSettings(priv->nexus_client, 0, &window_settings);
    window_settings.visible = true;
    priv->ipcclient->setVideoWindowSettings(priv->nexus_client, 0, &window_settings);

    priv->stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    priv->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
    if (!priv->stcChannel || !priv->parserBand)
    {
        ALOGE("%s: Unable to create stcChannel or parserBand", __FUNCTION__);
        return -1;
    }

    NEXUS_Frontend_GetDefaultAcquireSettings(&frontendAcquireSettings);
    frontendAcquireSettings.capabilities.ofdm = true;
    priv->frontend = NEXUS_Frontend_Acquire(&frontendAcquireSettings);
    if (!priv->frontend)
    {
        ALOGE("%s: Unable to find OFDM-capable frontend", __FUNCTION__);
        return -1;
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

static int tv_input_tuner_start(tv_input_private_t *priv)
{
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_FrontendOfdmSettings ofdmSettings;
    NEXUS_FrontendUserParameters userParams;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_VideoCodec video_codec = NEXUS_VideoCodec_eMpeg2;
    NEXUS_Error rc;

    // Enable the tuner
    ALOGE("%s: Tuning on freqency %d...", __FUNCTION__, FREQ);

    NEXUS_Frontend_GetDefaultOfdmSettings(&ofdmSettings);
    ofdmSettings.frequency = FREQ * 1000000;
    ofdmSettings.acquisitionMode = NEXUS_FrontendOfdmAcquisitionMode_eAuto;
    ofdmSettings.terrestrial = true;
    ofdmSettings.spectrum = NEXUS_FrontendOfdmSpectrum_eAuto;
    ofdmSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;
    NEXUS_Frontend_GetUserParameters(priv->frontend, &userParams);
    NEXUS_ParserBand_GetSettings(priv->parserBand, &parserBandSettings);

    if (userParams.isMtsif)
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eMtsif;
        parserBandSettings.sourceTypeSettings.mtsif = NEXUS_Frontend_GetConnector(priv->frontend); /* NEXUS_Frontend_TuneXyz() will connect this frontend to this parser band */
    }

    else
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
        parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
    }

    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    rc = NEXUS_ParserBand_SetSettings(priv->parserBand, &parserBandSettings);

    if (rc)
    {
        ALOGE("%s: ParserBand Setting failed", __FUNCTION__);
        return -1;
    }

    rc = NEXUS_Frontend_TuneOfdm(priv->frontend, &ofdmSettings);
    if (rc)
    {
        ALOGE("%s: Frontend TuneOfdm failed", __FUNCTION__);
        return -1;
    }

    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);
    videoProgram.settings.pidChannel = NEXUS_PidChannel_Open(priv->parserBand, VIDEO_PID, NULL);
    videoProgram.settings.codec = video_codec;

    if (videoProgram.settings.pidChannel)
    {
        rc = NEXUS_SimpleVideoDecoder_SetStcChannel(priv->videoDecoder, priv->stcChannel);
        if (rc)
        {
            ALOGE("%s: SetStcChannel failed", __FUNCTION__);
            return -1;
        }
    }

    if (videoProgram.settings.pidChannel)
    {
        rc = NEXUS_SimpleVideoDecoder_Start(priv->videoDecoder, &videoProgram);
        if (rc)
        {
            ALOGE("%s: VideoDecoderStart failed", __FUNCTION__);
            return -1;
        }
    }

    ALOGE("%s: Tuner has started streaming!!", __FUNCTION__);
    return 0;
}
#endif