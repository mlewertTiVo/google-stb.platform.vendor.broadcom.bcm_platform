LOCAL_PATH := $(call my-dir)
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus
APPLIBS_TOP ?=$(LOCAL_PATH)/../../../../../../../..
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

# Audio module
include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.bcm_platform
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
    $(NEXUS_LIB) \
    libutils \
    libcutils \
    libmedia \
    libnxclient

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_SRC_FILES += \
	brcm_audio.cpp \
	brcm_audio_nexus.cpp \
	brcm_audio_builtin.cpp \
	brcm_audio_dummy.cpp \

LOCAL_CFLAGS := -DLOG_TAG=\"BrcmAudio\" $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_C_INCLUDES += $(REFSW_BASE_DIR)/nexus/nxclient/include

include $(BUILD_SHARED_LIBRARY)

# Audio Policy Manager
ifeq ($(USE_CUSTOM_AUDIO_POLICY), 1)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    BrcmAudioPolicyManager.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libmedia \
    libbinder \
    libaudiopolicymanagerdefault

LOCAL_C_INCLUDES := \
    external/tinyalsa/include \
    frameworks/av/services/audiopolicy

LOCAL_MODULE := libaudiopolicymanager
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif

