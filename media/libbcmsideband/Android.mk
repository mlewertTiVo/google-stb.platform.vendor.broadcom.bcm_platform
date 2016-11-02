LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= libbcmsideband
LOCAL_SRC_FILES:= bcmsideband.cpp
LOCAL_SHARED_LIBRARIES := libutils libnexusipcclient libcutils libhwcbinder libbinder
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusservice \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib
include $(BUILD_SHARED_LIBRARY)
