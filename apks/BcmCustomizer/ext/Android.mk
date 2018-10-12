LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/priv-app

# Use shared logos and banners
LOCAL_RESOURCE_DIR := $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_SRC_FILES += ../base/src/com/broadcom/customizer/BcmCustomizerReceiverBase.java
LOCAL_SRC_FILES += ../base/src/com/broadcom/customizer/BcmSplashActivity.java

ifeq ($(call math_gt_or_eq,$(PRODUCT_SHIPPING_API_LEVEL),28),)
LOCAL_PRIVATE_PLATFORM_APIS := true
endif
LOCAL_PACKAGE_NAME := BcmCustomizer
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)
