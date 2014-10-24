# TODO: Put Broadcom copyright header

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    BcmOMXPlugin.cpp

LOCAL_CFLAGS := -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax

LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libdl                   \
        libui                   \
		libstagefright_foundation 
		
LOCAL_MODULE := libstagefrighthw

include $(BUILD_SHARED_LIBRARY)

