LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_CERTIFICATE := platform
LOCAL_PACKAGE_NAME := BcmOtaUpdater

include $(BUILD_PACKAGE)

