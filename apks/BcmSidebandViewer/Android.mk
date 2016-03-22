LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

# Leanback support
LOCAL_STATIC_JAVA_LIBRARIES += android-support-v4
LOCAL_STATIC_JAVA_LIBRARIES += android-support-v7-recyclerview
LOCAL_STATIC_JAVA_LIBRARIES += android-support-v17-leanback
LOCAL_RESOURCE_DIR := $(TOP)/frameworks/support/v17/leanback/res
LOCAL_RESOURCE_DIR += $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res
LOCAL_AAPT_FLAGS := --auto-add-overlay
LOCAL_AAPT_FLAGS += --extra-packages android.support.v17.leanback

# Debugging (adb install -r)
LOCAL_DEX_PREOPT := false

LOCAL_JNI_SHARED_LIBRARIES := libbcmsidebandviewer_jni
LOCAL_PROGUARD_ENABLED := disabled
LOCAL_PACKAGE_NAME := BcmSidebandViewer
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb

include $(BUILD_PACKAGE)
include $(call all-makefiles-under,$(LOCAL_PATH))
