LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_PACKAGE_NAME := BcmTvSettingsLauncher
LOCAL_PRIVATE_PLATFORM_APIS := true
include $(BUILD_PACKAGE)

