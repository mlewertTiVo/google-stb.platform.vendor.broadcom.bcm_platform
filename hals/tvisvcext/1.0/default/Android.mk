LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := bcm.hardware.tvisvcext@1.0-service
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_INIT_RC := bcm.hardware.tvisvcext@1.0-service.rc
LOCAL_CFLAGS := -Werror

LOCAL_SRC_FILES := service.cpp
LOCAL_SRC_FILES += TviSvcExt.cpp

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.tvisvcext@1.0 \
    bcm.hardware.nexus@1.0 \
    libhidlbase \
    libhidltransport \
    libbase \
    liblog \
    libutils \
    libnexus \
    libnxclient \
    libnxwrap

include $(BUILD_EXECUTABLE)
