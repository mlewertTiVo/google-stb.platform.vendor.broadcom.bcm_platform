LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/priv-app

# Use shared logos and banners
LOCAL_RESOURCE_DIR := $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := BcmCustomizer

LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)
