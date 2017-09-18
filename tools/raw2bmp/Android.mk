LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := raw2bmp.c
LOCAL_MODULE := raw2bmp
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_HOST_EXECUTABLE)
