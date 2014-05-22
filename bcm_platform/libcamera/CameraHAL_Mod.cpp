/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <hardware/hardware.h>

#include <utils/threads.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/camera.h>

#include "CameraDevHAL.h"


#include <EGL/egl.h>

#define RET_ERROR -1
#define RET_OK 0

static android::Mutex gCameraLock;

static android::CameraDevHAL* gCameraHAL = NULL;

/*****************************************************************************/
static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device);
static int camera_device_close(hw_device_t* device);
static int camera_get_number_of_cameras(void);
static int camera_get_camera_info(int camera_id, struct camera_info *info);

static struct hw_module_methods_t camera_module_methods = {
        open: camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "Broadcom Cameral Module",
         author: "Broadcom Inc",
         methods: &camera_module_methods
    },
    get_number_of_cameras: camera_get_number_of_cameras,
    get_camera_info: camera_get_camera_info,
};


typedef struct brcm_camera_device {
    camera_device_t base;
} brcm_camera_device_t;

int camera_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->setPreviewWindow(window);

    return rv;
}

void camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->enableMsgType(msg_type);
}

void camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->disableMsgType(msg_type);
}

int camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return 0;

    ti_dev = (brcm_camera_device*) device;

    return gCameraHAL->msgTypeEnabled(msg_type);
}

int camera_start_preview(struct camera_device * device)
{
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->startPreview();

    return rv;
}

void camera_stop_preview(struct camera_device * device)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->stopPreview();
}

int camera_preview_enabled(struct camera_device * device)
{
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->previewEnabled();
    return rv;
}


//int (*set_parameters)(struct camera_device *, const char *parms);
void camera_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}


int camera_device_close(hw_device_t* device)
{
    int ret = RET_OK;
	brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    android::Mutex::Autolock lock(gCameraLock);
	if (!device) {
        ret = -EINVAL;
        goto done;
    }

    ti_dev = (brcm_camera_device*) device;

    if (ti_dev) {
        if (gCameraHAL) {
            delete gCameraHAL;
            gCameraHAL = NULL;
        }

        if (ti_dev->base.ops) {
            free(ti_dev->base.ops);
        }
        free(ti_dev);
    }

done:

    return ret;
}

void camera_release(struct camera_device * device)
{
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->release();
}

int camera_dump(struct camera_device * device, int fd)
{
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->dump(fd);
    return rv;
}

int camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
#if 0
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    //  TODO: meta data buffer not current supported
    rv = gCameraHAL->storeMetaDataInBuffers(enable);
	
    return rv;
    //return enable ? android::INVALID_OPERATION: android::OK;
#else
   return 0;
#endif
}

int camera_start_recording(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->startRecording();
    return rv;
#else
    return 0;
#endif
}

void camera_stop_recording(struct camera_device * device)
{
#if 1
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->stopRecording();
#else
    return;
#endif
}

int camera_recording_enabled(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->recordingEnabled();
    return rv;
	#else
	return 0;
	#endif
}

void camera_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
#if 0
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->releaseRecordingFrame(opaque);
	#endif
}

int camera_auto_focus(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->autoFocus();
    return rv;
	#else
	return 0;
	#endif
}

int camera_cancel_auto_focus(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->cancelAutoFocus();
    return rv;
	#else
	return 0;
	#endif
}

int camera_take_picture(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->takePicture();
    return rv;
	#else
	return 0;
	#endif
}

int camera_cancel_picture(struct camera_device * device)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->cancelPicture();
    return rv;
	#else
	return 0;
	#endif
}

int camera_set_parameters(struct camera_device * device, const char *params)
{

    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->setParameters(params);
    return rv;
}

char* camera_get_parameters(struct camera_device * device)
{
    char* param = NULL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return NULL;

    ti_dev = (brcm_camera_device*) device;

    param = gCameraHAL->getParameters();

    return param;
}

static void camera_put_parameters(struct camera_device *device, char *parms)
{

    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    ti_dev = (brcm_camera_device*) device;

    gCameraHAL->putParameters(parms);
}

int camera_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
#if 1
    int rv = -EINVAL;
    brcm_camera_device* ti_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    ti_dev = (brcm_camera_device*) device;

    rv = gCameraHAL->sendCommand(cmd, arg1, arg2);
    return rv;
	#else
	ALOGE("_____comd:%x", cmd);
	return 0;
	#endif
}

/*******************************************************************
 * implementation of camera_module functions
 *******************************************************************/

/* open device handle to one of the cameras
 *
 * assume camera service will keep singleton of each camera
 * so this function will always only be called once per camera instance
 */

int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
	int rv = RET_OK;
	int cameraid;
	android::CameraDevHAL* camera = NULL;
	camera_device_t* camera_device = NULL;
	camera_device_ops_t* camera_ops = NULL;

	android::Mutex::Autolock lock(gCameraLock);

	ALOGD("CameraHardware: camera_device_open  module %d, name %s", module, name);


	if (name != NULL) {
		cameraid = atoi(name);

		camera_device = (camera_device_t*)malloc(sizeof(*camera_device));
		if(!camera_device)
		{
		    ALOGE("camera_device allocation fail");
		    rv = RET_ERROR;
		    goto fail;
		}

		camera_ops = (camera_device_ops_t*)malloc(sizeof(*camera_ops));
		if(!camera_ops)
		{
		    ALOGE("camera_ops allocation fail");
		    rv = RET_ERROR;
		    goto fail;
		}

		memset(camera_device, 0, sizeof(*camera_device));
		memset(camera_ops, 0, sizeof(*camera_ops));

		camera_device->common.tag = HARDWARE_DEVICE_TAG;
		camera_device->common.version = 0;
		camera_device->common.module = (hw_module_t *)(module);
		camera_device->common.close = camera_device_close;
		camera_device->ops = camera_ops;

		camera_ops->set_preview_window = camera_set_preview_window;
		camera_ops->set_callbacks = camera_set_callbacks;
 		camera_ops->enable_msg_type = camera_enable_msg_type;
		camera_ops->disable_msg_type = camera_disable_msg_type;
		camera_ops->msg_type_enabled = camera_msg_type_enabled;
		camera_ops->start_preview = camera_start_preview;
		camera_ops->stop_preview = camera_stop_preview;
		camera_ops->preview_enabled = camera_preview_enabled;
		camera_ops->store_meta_data_in_buffers = camera_store_meta_data_in_buffers;
		camera_ops->start_recording = camera_start_recording;
		camera_ops->stop_recording = camera_stop_recording;
		camera_ops->recording_enabled = camera_recording_enabled;
		camera_ops->release_recording_frame = camera_release_recording_frame;
		camera_ops->auto_focus = camera_auto_focus;
		camera_ops->cancel_auto_focus = camera_cancel_auto_focus;
		camera_ops->take_picture = camera_take_picture;
		camera_ops->cancel_picture = camera_cancel_picture;
		camera_ops->set_parameters = camera_set_parameters;
		camera_ops->get_parameters = camera_get_parameters;
		camera_ops->put_parameters = camera_put_parameters;
		camera_ops->send_command = camera_send_command;
		camera_ops->release = camera_release;
		camera_ops->dump = camera_dump;

		*device = &camera_device->common;

	//	 sp<CameraHardware> camerahardware = new CameraHardware();
         camera = new android::CameraDevHAL();
	     if(!camera)
         {
            ALOGE("Couldn't create instance of CameraHal class");
            rv = -ENOMEM;
            goto fail;
         }
		 /*if(camera->initialize() != android::NO_ERROR)
         {
            ALOGE("Couldn't initialize camera instance");
            rv = -ENODEV;
            goto fail;
         }*/
	     gCameraHAL = camera;
	}
	
	
    return rv;

fail:
	if(camera_device) {
	    free(camera_device);
	    camera_device = NULL;
	}
	if(camera_ops) {
	    free(camera_ops);
	    camera_ops = NULL;
	}

	*device = NULL;
	return rv;
}

int camera_get_number_of_cameras(void)
{
    int num_cameras = 1; // max camera

	ALOGD("CameraHardware:: camera_get_number_of_cameras %d ", num_cameras);	
    return num_cameras;
}

int camera_get_camera_info(int camera_id, struct camera_info *info)
{
    int rv = RET_OK;
    info->facing = CAMERA_FACING_FRONT;
	info->orientation = 0;
    return rv;
}


