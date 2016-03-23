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

#define LOG_TAG "tv_input"
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

#define DEVICE_ID_TUNER 0
#define DEVICE_ID_HDMI 1
extern tv_input_module_t hdmi_module;

using namespace android;

// Binder class for interaction with Tuner-HAL
class BpNexusTunerClient: public BpInterface<INexusTunerClient>
{
public:
    BpNexusTunerClient(const sp<IBinder>& impl)
        : BpInterface<INexusTunerClient>(impl)
    {
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

    tv_input_device_t      *hdmi_dev;
    struct tv_input_device *hdmi_ops;
} tv_input_private_t;

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device);

static struct hw_module_methods_t tv_input_module_methods = {
   .open = tv_input_device_open
};

tv_input_module_t HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = TV_INPUT_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = TV_INPUT_HARDWARE_MODULE_ID,
      .name               = "tv_input for set-top-box platforms",
      .author             = "Broadcom",
      .methods            = &tv_input_module_methods,
      .dso                = 0,
      .reserved           = {0},
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

    ALOGV("%s: Enter", __FUNCTION__);

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
    tuner_event.device_info.device_id = DEVICE_ID_TUNER;

    // No audio data for tuner
    tuner_event.device_info.audio_type = AUDIO_DEVICE_NONE;
    tuner_event.device_info.audio_address = NULL;

    // Config data
    priv->iNumConfigs = 1;
    priv->s_config =  (tv_stream_config_t *) malloc(sizeof (tv_stream_config_t));

    ALOGI("%s: Calling notify", __FUNCTION__);
    callback->notify(dev, &tuner_event, data);

    if (priv->hdmi_dev)
        return priv->hdmi_ops->initialize(priv->hdmi_dev, callback, data);

    return 0;
}

static int tv_input_get_stream_configurations(const struct tv_input_device *dev, int dev_id, int *numConfigs, const tv_stream_config_t **s_out)
{
    int i;
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    if (dev_id == DEVICE_ID_HDMI)
        return priv->hdmi_ops->get_stream_configurations(priv->hdmi_dev, dev_id, numConfigs, s_out);

    ALOGV("%s: Enter", __FUNCTION__);

    // Initialize the # of channels
    *numConfigs = priv->iNumConfigs;

    // Just update 1 stream (represents main video, no PIP for now)
    if (priv->s_config != NULL)
    {
        ALOGI("%s: s_config = %p", __FUNCTION__, priv->s_config);

        priv->s_config->stream_id = 1;
        priv->s_config->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        priv->s_config->max_video_width = 1920;
        priv->s_config->max_video_height = 1080;
    }

    *s_out = (priv->s_config);

    ALOGV("%s: Exit", __FUNCTION__);

    return 0;
}

static int tv_input_open_stream(struct tv_input_device *dev, int dev_id, tv_stream_t *pTVStream)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    if (dev_id == DEVICE_ID_HDMI)
        return priv->hdmi_ops->open_stream(priv->hdmi_dev, dev_id, pTVStream);

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder;
    do {
        binder = sm->getService(String16(TUNER_INTERFACE_NAME));
        if (binder != 0) {
            break;
        }

        ALOGI("%s: TV-HAL is waiting for TunerService...", __FUNCTION__);
        usleep(500000);
    } while(true);

    ALOGI("%s: TV-HAL acquired TunerService...", __FUNCTION__);
    priv->mNTC = interface_cast<INexusTunerClient>(binder);

    Parcel data, reply;
    data.writeInterfaceToken(android::String16(TUNER_INTERFACE_NAME));
    priv->mNTC->get_remote()->transact(GET_TUNER_CONTEXT, data, &reply);
    priv->nexus_client = (NexusClientContext *)reply.readInt32();
    ALOGI("%s: nexus_client = %p", __FUNCTION__, priv->nexus_client);

    // Create a native handle
    pTVStream->sideband_stream_source_handle = native_handle_create(NUM_FD, NUM_INT);

    // Setup the native handle data
    pTVStream->sideband_stream_source_handle->data[0] = 1;
    pTVStream->sideband_stream_source_handle->data[1] = (int)(priv->nexus_client);
    pTVStream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;

    return 0;
}

static int tv_input_close_stream(struct tv_input_device *dev, int dev_id, int stream_id)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (dev_id == DEVICE_ID_HDMI)
        return priv->hdmi_ops->close_stream(priv->hdmi_dev, dev_id, stream_id);
    ALOGV("%s: Enter", __FUNCTION__);
    return 0;
}

static int tv_input_request_capture(struct tv_input_device *dev, int dev_id, int stream_id, buffer_handle_t buffer, uint32_t seq)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (dev_id == DEVICE_ID_HDMI)
        return priv->hdmi_ops->request_capture(priv->hdmi_dev, dev_id, stream_id, buffer, seq);
    ALOGV("%s: Enter", __FUNCTION__);
    return 0;
}

static int tv_input_cancel_capture(struct tv_input_device *dev, int dev_id, int stream_id, uint32_t seq)
{
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (dev_id == DEVICE_ID_HDMI)
        return priv->hdmi_ops->cancel_capture(priv->hdmi_dev, dev_id, stream_id, seq);
    ALOGV("%s: Enter", __FUNCTION__);
    return 0;
}

/*****************************************************************************/

static int tv_input_device_close(struct hw_device_t *dev)
{
    ALOGV("%s: Enter", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv) {
        if (priv->hdmi_dev)
            priv->hdmi_ops->common.close((struct hw_device_t*)priv->hdmi_dev);
        free(priv->s_config);
        free(priv);
    }
    return 0;
}

/*****************************************************************************/

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device)
{
    ALOGV("%s: Enter", __FUNCTION__);

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

        // Use a fake tv_input_module_t to keep the code separate while using
        // the standard API. Use the last argument for an exchange of pointers:
        // * out: Send a pointer to the tuner device (required for callbacks)
        // *  in: Receive a pointer to the hdmi device
        struct hw_device_t* in_out_ptr = (struct hw_device_t*)dev;
        status = hdmi_module.common.methods->open(module, name, &in_out_ptr);
        dev->hdmi_dev = (tv_input_device_t *)in_out_ptr;
        dev->hdmi_ops = (struct tv_input_device *)in_out_ptr;
    }
    return status;
}
