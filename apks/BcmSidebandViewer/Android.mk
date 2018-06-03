LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_RESOURCE_DIR := $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res

LOCAL_JNI_SHARED_LIBRARIES := libbcmsidebandviewer_jni
LOCAL_PACKAGE_NAME := BcmSidebandViewer
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb
LOCAL_SDK_VERSION := current

include $(BUILD_PACKAGE)
include $(call all-makefiles-under,$(LOCAL_PATH))
