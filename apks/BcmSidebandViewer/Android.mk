LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

# Leanback support
LOCAL_STATIC_ANDROID_LIBRARIES += \
    android-support-v17-leanback \
    android-support-v7-recyclerview \
    android-support-compat \
    android-support-core-ui \
    android-support-media-compat \
    android-support-fragment

LOCAL_STATIC_JAVA_LIBRARIES += android-support-annotations

LOCAL_RESOURCE_DIR += $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res
LOCAL_USE_AAPT2 := true

# Debugging (adb install -r)
LOCAL_DEX_PREOPT := false

LOCAL_JNI_SHARED_LIBRARIES := libbcmsidebandviewer_jni
LOCAL_PROGUARD_ENABLED := disabled
LOCAL_PACKAGE_NAME := BcmSidebandViewer
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_PACKAGE)
include $(call all-makefiles-under,$(LOCAL_PATH))
