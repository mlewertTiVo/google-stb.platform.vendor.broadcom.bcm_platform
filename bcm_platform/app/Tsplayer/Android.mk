LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_JNI_SHARED_LIBRARIES := libjni_tune
LOCAL_PACKAGE_NAME := Tsplayer
include $(BUILD_PACKAGE)

include $(LOCAL_PATH)/jni/Android.mk
