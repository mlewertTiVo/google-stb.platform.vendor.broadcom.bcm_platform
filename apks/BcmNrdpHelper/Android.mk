LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/priv-app

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := BcmNrdpHelper

ifdef PRODUCT_SHIPPING_API_LEVEL
ifeq ($(call math_gt_or_eq,$(PRODUCT_SHIPPING_API_LEVEL),28),)
LOCAL_PRIVATE_PLATFORM_APIS := true
endif
else
# warning: assume legacy configuration.
LOCAL_PRIVATE_PLATFORM_APIS := true
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_CERTIFICATE := media

LOCAL_STATIC_ANDROID_LIBRARIES := \
    android-support-annotations

LOCAL_STATIC_JAVA_LIBRARIES := \
    gson-prebuilt-jar

include $(BUILD_PACKAGE)
