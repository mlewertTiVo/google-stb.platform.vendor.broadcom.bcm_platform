LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_JNI_SHARED_LIBRARIES := libjni_bcmfp
LOCAL_PACKAGE_NAME := BcmFlashPlayer
include $(BUILD_PACKAGE)

include $(LOCAL_PATH)/jni/Android.mk
