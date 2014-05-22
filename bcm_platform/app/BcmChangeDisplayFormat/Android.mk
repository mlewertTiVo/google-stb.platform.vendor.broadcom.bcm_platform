LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_JNI_SHARED_LIBRARIES := libjni_changedisplayformat
LOCAL_PACKAGE_NAME := BcmChangeDisplayFormat
include $(BUILD_PACKAGE)

include $(LOCAL_PATH)/jni/Android.mk
