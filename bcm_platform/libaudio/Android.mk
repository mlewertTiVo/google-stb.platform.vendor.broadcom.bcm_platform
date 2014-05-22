
LOCAL_PATH := $(call my-dir)
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus
APPLIBS_TOP ?=$(LOCAL_PATH)/../../../../../../../..
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)

include $(CLEAR_VARS)

include $(REFSW_PATH)/bin/include/platform_app.inc

LOCAL_SRC_FILES:=  \
   AudioPolicyManager.cpp

LOCAL_SHARED_LIBRARIES := \
                libnexus \
                libcutils \
                libutils \
                libmedia
    
LOCAL_STATIC_LIBRARIES := \
                libmedia_helper

LOCAL_WHOLE_STATIC_LIBRARIES := \
                libaudiopolicy_legacy  

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := audio_policy.bcm_platform

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := \
    libnexus \
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

LOCAL_SRC_FILES += AudioHardwareNEXUS.cpp

LOCAL_CFLAGS:= -DLOG_TAG=\"AudioHardwareNEXUS\" $(NEXUS_CFLAGS) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_WHOLE_STATIC_LIBRARIES :=\
            libaudiohw_legacy  
LOCAL_STATIC_LIBRARIES +=libmedia_helper

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

LOCAL_MODULE := audio.primary.bcm_platform
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
