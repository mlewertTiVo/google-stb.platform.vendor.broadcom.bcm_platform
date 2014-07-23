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
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := \
    $(NEXUS_LIB) \
    libutils \
    libcutils \
    libmedia \
    libbinder \
    libnexusipcclient 

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_SRC_FILES += brcm_audio2.cpp

LOCAL_CFLAGS:= -DLOG_TAG=\"brcm_audio2\" $(NEXUS_CFLAGS) -DANDROID $(MP_CFLAGS)


LOCAL_STATIC_LIBRARIES +=libmedia_helper

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

LOCAL_MODULE := audio.primary.bcm_platform
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
