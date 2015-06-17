LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
           libbinder \
           libcutils \
           libhwcbinder \
           liblog \
           libutils

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
                    $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/blib

LOCAL_SRC_FILES := jni_adjustScreenOffset.cpp

LOCAL_MODULE := libjni_adjustScreenOffset
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
