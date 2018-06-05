LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_RESOURCE_DIR := $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res

LOCAL_PACKAGE_NAME := BcmBP3Config
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_JAVA_LIBRARIES := android.hidl.base-V1.0-java
LOCAL_STATIC_JAVA_LIBRARIES := bcm.hardware.bp3-V1.0-java-static
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb

include $(BUILD_PACKAGE)
