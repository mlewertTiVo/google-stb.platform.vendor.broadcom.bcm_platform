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
#ifndef ANDROID_HARDWARE_DEV_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_DEV_CAMERA_HARDWARE_H
#include <utils/threads.h>
#include <../services/camera/libcameraservice/CameraHardwareInterface.h>
/*#include "../../../../frameworks/base/services/camera/libcameraservice/CameraHardwareInterface.h"*/
#include <hardware/camera.h>

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <ui/GraphicBuffer.h>
#include <camera/CameraParameters.h>


#include <sys/ioctl.h>
#include "V4L2Camera.h"
#define VIDEO_DEVICE        "/dev/video0"
#define MIN_WIDTH           640
#define MIN_HEIGHT          480
#define CAM_SIZE            "640x480"
#define PIXEL_FORMAT        V4L2_PIX_FMT_YUYV
/** msgType in notifyCallback and dataCallback functions
     defined at mips-ics\system\core\include\system\camera.h, just put here temp for test 


enum {
    CAMERA_MSG_ERROR = 0x0001,            
    CAMERA_MSG_SHUTTER = 0x0002,          
    CAMERA_MSG_FOCUS = 0x0004,            
    CAMERA_MSG_ZOOM = 0x0008,            
    CAMERA_MSG_PREVIEW_FRAME = 0x0010,    
    CAMERA_MSG_VIDEO_FRAME = 0x0020,      
    CAMERA_MSG_POSTVIEW_FRAME = 0x0040,   
    CAMERA_MSG_RAW_IMAGE = 0x0080,        
    CAMERA_MSG_COMPRESSED_IMAGE = 0x0100, 
    CAMERA_MSG_RAW_IMAGE_NOTIFY = 0x0200, 
    
    CAMERA_MSG_PREVIEW_METADATA = 0x0400, // dataCallback
    CAMERA_MSG_ALL_MSGS = 0xFFFF
};

*/

namespace android {
class CameraDevHAL;

class CameraDevHAL

{

    /*--------------------Interface Methods---------------------------------*/

     //@{
public:
	/**
	* Only used if overlays are used for camera preview.
	*/
	int setPreviewWindow(struct preview_stream_ops *window);
	int    startPreview();

    /** Set the notification and data callbacks */
    void setCallbacks(camera_notify_callback notify_cb,
                        camera_data_callback data_cb,
                        camera_data_timestamp_callback data_cb_timestamp,
                        camera_request_memory get_memory,
                        void *user);
    void enableMsgType(int32_t msgType);
	void disableMsgType(int32_t msgType);
	bool        msgTypeEnabled(int32_t msgType);
	void        stopPreview();
	bool        previewEnabled();
	void release();
	status_t    startRecording();
	void        stopRecording();
	bool        recordingEnabled();
	status_t    autoFocus();
	static int beginAutoFocusThread(void *cookie);
	status_t    cancelAutoFocus();
	int autoFocusThread();
	status_t    takePicture();
	status_t    cancelPicture();
	status_t dump(int fd) const;
	status_t allocPreviewBufs(buffer_handle_t **previewbufint,int width, int height, const char* previewFormat,
                                        unsigned int buffercount, unsigned int &max_queueable);
	int    setParameters(const char* params);
	char*  getParameters();
    void putParameters(char *);
	status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);
	void        releaseRecordingFrame(const sp<IMemory>& mem);
	int pictureThread();
     //@}

/*--------------------Internal Member functions - Public---------------------------------*/

public:
 /** @name internalFunctionsPublic */
  //@{

    /** Constructor of CameraDevHAL */
    CameraDevHAL();

    // Destructor of CameraDevHAL
    ~CameraDevHAL();

private:

	/*status_t allocPreviewBufs(int width, int height, const char* previewFormat, unsigned int bufferCount, unsigned int &max_queueable);*/
    class PreviewThread : public Thread {
        CameraDevHAL* mHardware;
    public:
        PreviewThread(CameraDevHAL* hw)
            : Thread(false), mHardware(hw) { }
        virtual void onFirstRef() {
            run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHardware->previewThread();
            // loop until we need to quit
            return true;
        }
    };
	struct camera_preview_window {
        struct preview_stream_ops nw;
        void *user;
    };
    int previewThread();
	void initDefaultParameters();
private:
	camera_data_timestamp_callback mTimestampFn;
    void*                   mUser;
	preview_stream_ops_t          *mANativeWindow;
	camera_notify_callback         mNotifyFn;
    camera_data_callback           mDataFn;
	camera_request_memory mRequestMemory;
	int mBufferCount;
	sp<PreviewThread>       mPreviewThread;
	int                     mPreviewFrameSize;
	/*buffer_handle_t** mBufferHandleMap;
	IMG_native_handle_t** mGrallocHandleMap;*/
	V4L2Camera              camera;
	mutable Mutex           mLock;
	sp<MemoryHeapBase>      mHeap;
    sp<MemoryBase>          mBuffer;
	sp<MemoryHeapBase>      mRecordHeap;
	bool                    previewStopped;
	int32_t                 mMsgEnabled;
	sp<MemoryBase>          mRecordBuffer;
	int                     camera_device;
	sp <ANativeWindow>      mPreviewWindow;
	data_callback           mDataCb;
	notify_callback         mNotifyCb;
    data_callback_timestamp mDataCbTimestamp;
	CameraParameters        mParameters;
	void*                   mCbUser;
	int                     mLoop;
	bool                    mRecordRunning;
};


}; // namespace android

#endif
