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

/*****************************************************************************/

typedef struct tv_input_private {
    tv_input_device_t device;

    // Callback related data
    const tv_input_callback_ops_t* callback;
    void* callback_data;
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

/*****************************************************************************/

static int tv_input_initialize(struct tv_input_device* dev,
        const tv_input_callback_ops_t* callback, void* data)
{
	ALOGE("%s: Enter", __FUNCTION__);
    if (dev == NULL || callback == NULL) {
        return -EINVAL;
    }
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv->callback != NULL) {
        return -EEXIST;
    }

    priv->callback = callback;
    priv->callback_data = data;

	tv_input_event_t tuner_event;

    tuner_event.type = TV_INPUT_EVENT_DEVICE_AVAILABLE;
    tuner_event.device_info.type = TV_INPUT_TYPE_TUNER;
	tuner_event.device_info.device_id = 0;

	// No audio data for tuner
	tuner_event.device_info.audio_type = AUDIO_DEVICE_NONE;
	tuner_event.device_info.audio_address = NULL;

	ALOGE("%s: Calling notify", __FUNCTION__);
    callback->notify(dev, &tuner_event, data);

    return 0;
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

static int tv_input_open_stream(struct tv_input_device*, int dev_id, tv_stream_t *pTVStream)
{
	ALOGE("%s: Enter", __FUNCTION__);

	ALOGE("%s: stream_id = %d", pTVStream->stream_id);

    return -EINVAL;
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