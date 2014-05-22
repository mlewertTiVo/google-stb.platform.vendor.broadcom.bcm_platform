LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_JNI_SHARED_LIBRARIES := libjni_bcmtp

LOCAL_MODULE := BcmATPArchive

include $(BUILD_STATIC_JAVA_LIBRARY)

include $(LOCAL_PATH)/jni/Android.mk
