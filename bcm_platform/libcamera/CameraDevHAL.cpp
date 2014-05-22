/******************************************************************************
 * $brcm_Workfile: CameraHardware.cpp $
 * $brcm_Revision: 2 $
 * $brcm_Date: 8/12/11 3:19p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/thirdparty/google/honeycomb/src/broadcom/honeycomb/vendor/broadcom/bcm_platform/libcamera/CameraHardware.cpp $
 * 
 * 2   8/12/11 3:19p fzhang
 * SW7425-1081:Add Camera support to HoneyComb
 * 
 * 2   4/13/11 4:11p ttrammel
 * SW7420-1813: Support GoogleTV camera in Android.
 * 
 *****************************************************************************/
/*
**
** Copyright 2009, The Android-x86 Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** Author: Niels Keeman <nielskeeman@gmail.com>
**
*/
#define LOG_TAG "CameraDevHAL"
#include <utils/Log.h>
#include "CameraDevHAL.h"
#include <fcntl.h>
#include <sys/mman.h>

#include <binder/IMemory.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/RefBase.h>
#include <gui/ISurface.h>
#include <system/window.h>
#include <ui/GraphicBuffer.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <system/window.h>
#include <hardware/camera.h>
#include <utils/threads.h>

#define CAMHAL_GRALLOC_USAGE GRALLOC_USAGE_HW_TEXTURE | \
                             GRALLOC_USAGE_HW_RENDER | \
                             GRALLOC_USAGE_SW_READ_RARELY | \
                             GRALLOC_USAGE_SW_WRITE_NEVER
#define HAL_PIXEL_FORMAT_TI_NV12 0x100

namespace android {

CameraDevHAL::CameraDevHAL()
			:   mNotifyFn(NULL),
				mDataFn(NULL),
				mNotifyCb(NULL),
				mDataCb(NULL),
				mTimestampFn(NULL),
				mDataCbTimestamp(NULL),
				mRecordHeap(0),
				mUser(NULL),
				mANativeWindow(NULL),
				mPreviewFrameSize(0),
				mHeap(0),
				previewStopped(true),
				mLoop(0),
				mRecordRunning(false),
				mBufferCount(0)
{
	initDefaultParameters();
	/*mBufferHandleMap = NULL;*/
	mPreviewThread = 0;
	mMsgEnabled = 0;
}

CameraDevHAL::~CameraDevHAL()
{
    /*singleton.clear();*/
}

void CameraDevHAL::initDefaultParameters()
{
    #if 0
    CameraParameters p;

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(15);
    p.setPreviewFormat("yuv422sp");
    p.set(p.KEY_SUPPORTED_PREVIEW_SIZES, CAM_SIZE);

    p.setPictureSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPictureFormat("jpeg");
    p.set(p.KEY_SUPPORTED_PICTURE_SIZES, CAM_SIZE);

    if (setParameters(p) != NO_ERROR) {
        ALOGE("Failed to set default parameters?!");
    }
	#endif
}

void CameraDevHAL::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
	ALOGE("_______enableMsgType_%d",msgType);
}

void CameraDevHAL::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled &= ~msgType;
}

bool CameraDevHAL::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    return (mMsgEnabled & msgType);
}

static ANativeWindow *__to_anw2(void *user)
{
    CameraHardwareInterface *__this =
            reinterpret_cast<CameraHardwareInterface *>(user);
    return __this->mPreviewWindow.get();
}

data_callback __to_dcb2(void *user)
{
    CameraHardwareInterface *__this =
            reinterpret_cast<CameraHardwareInterface *>(user);
    return __this->mDataCb;
}
data_callback_timestamp __to_dcbts2(void *user)
{
    CameraHardwareInterface *__this =
            reinterpret_cast<CameraHardwareInterface *>(user);
    return __this->mDataCbTimestamp;
}
notify_callback __to_ntfy2(void *user)
{
    CameraHardwareInterface *__this =
            reinterpret_cast<CameraHardwareInterface *>(user);
    return __this->mNotifyCb;
}

void * __to_user2(void *user)
{
    CameraHardwareInterface *__this =
            reinterpret_cast<CameraHardwareInterface *>(user);
    return __this->mCbUser;
}

status_t CameraDevHAL::setPreviewWindow(struct preview_stream_ops *window)
{
	if (window == NULL)
	{
		return OK;
	}
    mANativeWindow = window;
	
	#define anw2(n) __to_anw2(((struct camera_preview_window *)n)->user)
	mPreviewWindow = anw2(window);
    return OK;
}
int CameraDevHAL::setParameters(const char* params)
{
     ALOGD("_______CameraDevHAL::setParameters[%s]",params);
     return NO_ERROR;

}
char* CameraDevHAL::getParameters()
{
    char* params_string;
	params_string = (char*) malloc(sizeof(char) * (1024));
	/*strcpy(params_string, 
		"preview-size=640x480;preview-frame-rate=15;preview-format=yuv422sp;"\
		"preview-size-values=640x480;picture-size=640x480;picture-format=jpeg;"\
		"picture-size-values=640x480;focus-mode=0;");*/
	strcpy(params_string, 
			"exposure-compensation=0;focus-mode=0;jpeg-quality=90;picture-format=jpeg;"\
			"picture-size=640x480;picture-size-values=640x480;preview-format=yuv422sp;"\
			"preview-frame-rate=15;preview-size=640x480;preview-size-values=640x480;"\
			"recording-hint=false");

	
    return params_string;
}

void CameraDevHAL::putParameters(char *parms)
{
    free(parms);
}


int CameraDevHAL::previewThread()
{
    Mutex::Autolock lock(mLock);
	if (!previewStopped) {
		
		if (mPreviewWindow != NULL && (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME))
		{
			void *buffer_addr = NULL;
			/*android_native_buffer_t *previewbuf;*/
			ANativeWindowBuffer* previewbuf;
			mPreviewWindow->dequeueBuffer_DEPERCATED(mPreviewWindow.get(), &previewbuf);	
			sp<GraphicBuffer> buf(new GraphicBuffer(previewbuf, false));
			buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&buffer_addr));
			
			if(NULL != buffer_addr)
			        camera.GrabPreviewFrameToPreviewWindow(buffer_addr, previewbuf->width, previewbuf->height);
			buf->unlock();
			mPreviewWindow->queueBuffer_DEPERCATED(mPreviewWindow.get(), previewbuf);
            usleep(2000);
	    }
		else
		{
		    return NO_ERROR;
		}
        
		 if ((mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) ||
			(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)) {
                #define user2(n) __to_user2((n))
		        mCbUser = user2(mUser);
			    #if 1
	            if (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) {
				camera.GrabRecordFrame(mRecordHeap->getBase());
				nsecs_t timeStamp = systemTime(SYSTEM_TIME_MONOTONIC);
				#define dcbts2(n) __to_dcbts2((n))
				mDataCbTimestamp = dcbts2(mUser);
				
				mDataCbTimestamp(timeStamp, CAMERA_MSG_VIDEO_FRAME,mRecordBuffer, mCbUser);
	            }
				else
				{
				    usleep(30000);
				}
					
				
				#endif

				#if 0
				#define dcb2(n) __to_dcb2((n))
				mDataCb = dcb2(mUser);
				mDataCb(CAMERA_MSG_PREVIEW_FRAME,mBuffer,NULL, mCbUser);
				#endif
		}	
		
		
	}
    return NO_ERROR;
}


status_t CameraDevHAL::startPreview()
{
 
	Mutex::Autolock lock(mLock);

    if (mPreviewThread != 0) {
        //already running
        return INVALID_OPERATION;
    }

    camera.Open(VIDEO_DEVICE, MIN_WIDTH, MIN_HEIGHT, PIXEL_FORMAT);

    mPreviewFrameSize = MIN_WIDTH * MIN_HEIGHT * 2;

    mHeap = new MemoryHeapBase(mPreviewFrameSize);
    mBuffer = new MemoryBase(mHeap, 0, mPreviewFrameSize);

    camera.Init();
    camera.StartStreaming();

    previewStopped = false;
    mPreviewThread = new PreviewThread(this);

	return 0;
    
}

void CameraDevHAL::stopPreview()
{
    #if 1
    sp<PreviewThread> previewThread;

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        previewStopped = true;
    }

    if (mPreviewThread != 0) {
        camera.Uninit();
        camera.StopStreaming();
        camera.Close();
    }

    {
        Mutex::Autolock lock(mLock);
        previewThread = mPreviewThread;
    }

    if (previewThread != 0) {
        previewThread->requestExitAndWait();
    }

    Mutex::Autolock lock(mLock);
    mPreviewThread.clear();
    #endif
}

bool CameraDevHAL::previewEnabled()
{
    return mPreviewThread != 0;
}

status_t CameraDevHAL::startRecording()
{
    Mutex::Autolock lock(mLock);

    mRecordHeap = new MemoryHeapBase(mPreviewFrameSize);
    mRecordBuffer = new MemoryBase(mRecordHeap, 0, mPreviewFrameSize);
    mRecordRunning = true;

    return NO_ERROR;
}

void CameraDevHAL::stopRecording()
{
    mRecordRunning = false;
}

bool CameraDevHAL::recordingEnabled()
{
    return mRecordRunning;
}

int CameraDevHAL::beginAutoFocusThread(void *cookie)
{
    CameraDevHAL *c = (CameraDevHAL *)cookie;
    return c->autoFocusThread();
}

int CameraDevHAL::autoFocusThread()
{
    if (mMsgEnabled & CAMERA_MSG_FOCUS)
    {
        #define ntfy2(n) __to_ntfy2((n))
		mNotifyCb = ntfy2(mUser);
        mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mUser);
    }
    return NO_ERROR;
}

status_t CameraDevHAL::autoFocus()
{
    Mutex::Autolock lock(mLock);
    if (createThread(beginAutoFocusThread, this) == false)
        return UNKNOWN_ERROR;
    return NO_ERROR;
}

status_t CameraDevHAL::cancelAutoFocus()
{
    return NO_ERROR;
}

void CameraDevHAL::release()
{
    close(camera_device);
}

status_t  CameraDevHAL::dump(int fd) const
{
    return NO_ERROR;
}
int CameraDevHAL::pictureThread()
{
    unsigned char *frame;
    int bufferSize;
    int w,h;
    int ret;
    struct v4l2_buffer buffer;
    struct v4l2_format format;
    struct v4l2_buffer cfilledbuffer;
    struct v4l2_requestbuffers creqbuf;
    struct v4l2_capability cap;

   #define user2(n) __to_user2((n))
   mCbUser = user2(mUser);
   if (mMsgEnabled & CAMERA_MSG_SHUTTER)
   	{
   	   #define ntfy2(n) __to_ntfy2((n))
		mNotifyCb = ntfy2(mUser);
        mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCbUser);
   	}
    
   
    camera.Open(VIDEO_DEVICE, MIN_WIDTH, MIN_HEIGHT, PIXEL_FORMAT);
    camera.Init();
    camera.StartStreaming();

    if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
        ALOGD ("mJpegPictureCallback");
		#define dcb2(n) __to_dcb2((n))
		mDataCb = dcb2(mUser);
        
        mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, camera.GrabJpegFrame(),NULL,mCbUser);
    }

    camera.Uninit();
    camera.StopStreaming();
    camera.Close();

    return NO_ERROR;
}

status_t CameraDevHAL::takePicture()
{
    stopPreview();
    //if (createThread(beginPictureThread, this) == false)
    //    return -1;

    pictureThread();

    return NO_ERROR;
}

status_t CameraDevHAL::cancelPicture()
{

    return NO_ERROR;
}

void CameraDevHAL::setCallbacks(/*notify_callback notify_cb,
                                  data_callback data_cb,
                                  data_callback_timestamp data_cb_timestamp,*/
                                  camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void *arg) {
    mNotifyFn = notify_cb;
    mDataFn = data_cb;
    mTimestampFn = data_cb_timestamp;
	mRequestMemory = get_memory;
    mUser = arg;
}

status_t CameraDevHAL::sendCommand(int32_t command, int32_t arg1,
                                         int32_t arg2)
{
    return BAD_VALUE;
}

void CameraDevHAL::releaseRecordingFrame(const sp<IMemory>& mem)
{
}

};

