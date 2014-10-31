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

// Binder related headers
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <TunerInterface.h>

using namespace android;

// Binder class for interaction with Tuner-HAL
class BpNexusTunerClient: public BpInterface<INexusTunerClient>
{
public:
    BpNexusTunerClient(const sp<IBinder>& impl)
        : BpInterface<INexusTunerClient>(impl)
    {
    }

    void api_over_binder(api_data *cmd)
    {
        Parcel data, reply;
        data.writeInterfaceToken(android::String16(TUNER_INTERFACE_NAME));
        data.write(cmd, sizeof(api_data));
        ALOGE("api_over_binder: cmd->api = 0x%x", cmd->api);

        remote()->transact(API_OVER_BINDER, data, &reply);
        reply.read(&cmd->param, sizeof(cmd->param));
    }

    IBinder* get_remote()
    {
        return remote();
    }
};
android_IMPLEMENT_META_INTERFACE(NexusTunerClient, TUNER_INTERFACE_NAME)

/*****************************************************************************/
typedef struct tv_input_private {
    tv_input_device_t device;

    // Callback related data
    const tv_input_callback_ops_t* callback;
    void* callback_data;

    int iNumConfigs;
    tv_stream_config_t *s_config;

    sp<INexusTunerClient> mNTC;
    NexusClientContext *nexus_client;
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

// native_handle_t related
#define NUM_FD      0
#define NUM_INT     2

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

    // Config data
    priv->iNumConfigs = 1;
    priv->s_config =  (tv_stream_config_t *) malloc(sizeof (tv_stream_config_t));

	ALOGE("%s: Calling notify", __FUNCTION__);
    callback->notify(dev, &tuner_event, data);

    return 0;
}

static int tv_input_get_stream_configurations(const struct tv_input_device *dev, int dev_id, int *numConfigs, const tv_stream_config_t **s_out)
{
    int i;
    tv_input_private_t* priv = (tv_input_private_t*)dev;

	ALOGE("%s: Enter", __FUNCTION__);

    // Initialize the # of channels
    *numConfigs = priv->iNumConfigs;

    // Just update 1 stream (represents main video, no PIP for now)
    if (priv->s_config != NULL)
    {
        ALOGE("%s: s_config = %p", __FUNCTION__, priv->s_config);

        priv->s_config->stream_id = 1;
        priv->s_config->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        priv->s_config->max_video_width = 1920;
        priv->s_config->max_video_height = 1080;
    }

    *s_out = (priv->s_config);

	ALOGE("%s: Exit", __FUNCTION__);

    return 0;
}

static int tv_input_open_stream(struct tv_input_device *dev, int dev_id, tv_stream_t *pTVStream)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder;
    do {
        binder = sm->getService(String16(TUNER_INTERFACE_NAME));
        if (binder != 0) {
            break;
        }

        ALOGE("%s: TV-HAL is waiting for TunerService...", __FUNCTION__);
        usleep(500000);
    } while(true);

    ALOGE("%s: TV-HAL acquired TunerService...", __FUNCTION__);
    priv->mNTC = interface_cast<INexusTunerClient>(binder);

    api_data cmd;
//    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_get_client_context;
    priv->mNTC->api_over_binder(&cmd);
    priv->nexus_client = cmd.param.clientContext.out.nexus_client;
    ALOGE("%s: nexus_client = %p", __FUNCTION__, priv->nexus_client);

    // Create a native handle
    pTVStream->sideband_stream_source_handle = native_handle_create(NUM_FD, NUM_INT);

    // Setup the native handle data
    pTVStream->sideband_stream_source_handle->data[0] = 1;
//    pTVStream->sideband_stream_source_handle->data[1] = (int)(priv->nexus_client);
    pTVStream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;

    return 0;
}

static int tv_input_close_stream(struct tv_input_device*, int, int)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return 0;
}

static int tv_input_request_capture(struct tv_input_device*, int, int, buffer_handle_t, uint32_t)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return 0;
}

static int tv_input_cancel_capture(struct tv_input_device*, int, int, uint32_t)
{
	ALOGE("%s: Enter", __FUNCTION__);
    return 0;
}

/*****************************************************************************/

static int tv_input_device_close(struct hw_device_t *dev)
{
	ALOGE("%s: Enter", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv) {
        free(priv->s_config);
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