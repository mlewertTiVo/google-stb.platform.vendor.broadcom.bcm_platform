LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_MODULE:= libbcmsideband
LOCAL_SRC_FILES:= bcmsideband.cpp
LOCAL_SHARED_LIBRARIES := libutils libnexusipcclient libcutils libhwcbinder libbinder
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusipc \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libhwcomposer/blib
include $(BUILD_SHARED_LIBRARY)
