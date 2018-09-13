LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := bcm.hardware.dpthak@1.0-service
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := bcm.hardware.dpthak@1.0-service.rc
LOCAL_CFLAGS := -Werror
LOCAL_C_INCLUDES := $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SRC_FILES := service.cpp
LOCAL_SRC_FILES += DptHak.cpp
LOCAL_SRC_FILES += ethcoal.c
LOCAL_SRC_FILES += hfrvideo.c

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.dpthak@1.0 \
    libhidlbase \
    libhidltransport \
    libbase \
    liblog \
    libcutils \
    libutils

include $(BUILD_EXECUTABLE)
