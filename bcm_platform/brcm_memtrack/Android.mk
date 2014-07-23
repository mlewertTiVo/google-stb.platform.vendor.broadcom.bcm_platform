LOCAL_PATH := $(call my-dir)
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus
APPLIBS_TOP ?=$(LOCAL_PATH)/../../../../../../../..
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(CLEAR_VARS)

include $(REFSW_PATH)/bin/include/platform_app.inc

LOCAL_SRC_FILES:=  \
    brcm_memtrack.cpp

LOCAL_SHARED_LIBRARIES := \
    $(NEXUS_LIB)


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := memtrack.bcm_platform

include $(BUILD_SHARED_LIBRARY)
