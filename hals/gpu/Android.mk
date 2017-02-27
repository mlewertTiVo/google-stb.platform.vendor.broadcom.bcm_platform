LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PROPRIETARY_MODULE := true
LOCAL_PACKAGE_NAME := gfxdriver-bcmstb
LOCAL_SDK_VERSION := current

include $(BUILD_PACKAGE)

