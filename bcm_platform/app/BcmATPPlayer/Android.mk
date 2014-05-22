LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_STATIC_JAVA_LIBRARIES := BcmATPArchive
LOCAL_JNI_SHARED_LIBRARIES := libjni_bcmtp
LOCAL_PACKAGE_NAME := BcmATPPlayer

include $(BUILD_PACKAGE)
