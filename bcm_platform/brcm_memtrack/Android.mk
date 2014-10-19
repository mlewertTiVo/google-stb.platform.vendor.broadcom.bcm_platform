LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=  \
    brcm_memtrack.cpp

LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)

LOCAL_C_INCLUDES += hardware/libhardware/include

LOCAL_SHARED_LIBRARIES := \
    libnexus


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := memtrack.bcm_platform

include $(BUILD_SHARED_LIBRARY)
