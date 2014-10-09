ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
LOCAL_PATH:= $(call my-dir)
REFSW_PATH :=vendor/broadcom/bcm_platform/brcm_nexus

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif    

$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    src/LinkedList.cpp \
    src/PESFeeder.cpp \
    src/OMXNexusDecoder.cpp \
    src/OMXNexusAudioDecoder.cpp \
    src/BcmDebug.cpp \
    src/AndroidVideoWindow.cpp

ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)
LOCAL_SRC_FILES += src/OMXNexusVideoEncoder.cpp
endif

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(REFSW_PATH)/bin/include \
    $(REFSW_PATH)/../libgralloc \
    $(TOP)/frameworks/native/include/utils \
    $(TOP)/frameworks/av/include \
    $(TOP)/vendor/broadcom/bcm_platform/brcm_nexus/bin/include \
    $(TOP)/vendor/broadcom/bcm_platform/libnexusservice \
    $(TOP)/vendor/broadcom/bcm_platform/libnexusipc 

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libutils \
    libcutils \
    $(NEXUS_LIB) \
    libnexusipcclient \
    libui

REMOVE_NEXUS_CFLAGS := -Wstrict-prototypes -march=armv7-a

LOCAL_CFLAGS := $(filter-out $(REMOVE_NEXUS_CFLAGS), $(NEXUS_CFLAGS))
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
LOCAL_CFLAGS += -Werror
ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)
LOCAL_CFLAGS += -DBCM_OMX_SUPPORT_ENCODER
endif

LOCAL_MODULE:= libbcm_utils
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif
