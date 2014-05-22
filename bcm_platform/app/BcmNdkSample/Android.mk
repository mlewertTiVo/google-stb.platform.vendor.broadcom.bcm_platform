LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_JNI_SHARED_LIBRARIES = libBcmNdkSample_jni

LOCAL_PACKAGE_NAME := BcmNdkSample

include $(BUILD_PACKAGE)

MY_PATH := $(LOCAL_PATH)
include $(MY_PATH)/jni/Android.mk
