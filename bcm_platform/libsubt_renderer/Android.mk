
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
    subtRenderer.cpp
       
LOCAL_SHARED_LIBRARIES :=       \
        libutils                \
        libcutils               \
        libskia                   \
#       libsurfaceflinger_client \
        libui \
	libgui


LOCAL_C_INCLUDES := \
    external/skia/include/core

LOCAL_CFLAGS := -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
    
LOCAL_MODULE := libsubt_renderer

include $(BUILD_SHARED_LIBRARY)

