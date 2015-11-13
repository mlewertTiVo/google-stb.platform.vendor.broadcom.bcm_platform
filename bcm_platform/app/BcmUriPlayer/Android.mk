LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_PACKAGE_NAME := BcmUriPlayer
LOCAL_DEX_PREOPT := false

include $(BUILD_PACKAGE)

