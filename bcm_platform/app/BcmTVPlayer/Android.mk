LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_STATIC_JAVA_LIBRARIES := BcmTVArchive
LOCAL_JNI_SHARED_LIBRARIES := libjni_bcmtv
LOCAL_PACKAGE_NAME := BcmTVPlayer

include $(BUILD_PACKAGE)
