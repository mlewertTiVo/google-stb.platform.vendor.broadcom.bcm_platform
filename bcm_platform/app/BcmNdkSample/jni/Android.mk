LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE    := libBcmNdkSample_jni

LOCAL_SRC_FILES := BcmNdkSample_jni.c
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
