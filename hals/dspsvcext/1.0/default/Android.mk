LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := bcm.hardware.dspsvcext@1.0-service
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := bcm.hardware.dspsvcext@1.0-service.rc

LOCAL_SRC_FILES := service.cpp
LOCAL_SRC_FILES += DspSvcExt.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.dspsvcext@1.0 \
    libhidlbase \
    libhidltransport \
    libbase \
    liblog \
    libutils \
    libhwcbinder \
    libbinder \

include $(BUILD_EXECUTABLE)
