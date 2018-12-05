LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/priv-app

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_JAVA_LIBRARIES := android.hidl.base-V1.0-java
LOCAL_STATIC_JAVA_LIBRARIES := bcm.hardware.nexus-V1.1-java

LOCAL_PACKAGE_NAME := BcmCustomizer

ifdef PRODUCT_SHIPPING_API_LEVEL
ifeq ($(call math_gt_or_eq,$(PRODUCT_SHIPPING_API_LEVEL),28),)
LOCAL_PRIVATE_PLATFORM_APIS := true
endif
else
# warning: assume legacy configuration.
LOCAL_PRIVATE_PLATFORM_APIS := true
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_CERTIFICATE := platform

LOCAL_STATIC_ANDROID_LIBRARIES := \
    android-support-annotations

include $(BUILD_PACKAGE)
