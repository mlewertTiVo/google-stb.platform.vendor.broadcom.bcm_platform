LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_JNI_SHARED_LIBRARIES := libjni_adjustScreenOffset
LOCAL_PACKAGE_NAME := BcmAdjustScreenOffset
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb
include $(BUILD_PACKAGE)



include $(LOCAL_PATH)/jni/Android.mk
