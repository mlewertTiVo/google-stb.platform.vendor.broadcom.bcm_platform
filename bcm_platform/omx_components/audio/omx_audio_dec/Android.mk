# TODO: Put Broadcom copyright header

ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)

REFSW_PATH :=vendor/broadcom/bcm_platform/brcm_nexus
LOCAL_PATH := $(call my-dir)

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

#
# libOMX.BCM.AUDIO.DECODER
#

include $(CLEAR_VARS)

COMMON_INCLUDE_FILES+=\
    $(TOP)/$(REFSW_PATH)/bin/include \
    $(TOP)/frameworks/native/include/media/openmax \
    $(TOP)/frameworks/native/include/utils \
    $(TOP)/frameworks/native/include/ui \
    $(TOP)/frameworks/native/include/media/hardware \
    $(TOP)/vendor/broadcom/bcm_platform/omx_core/inc \
    $(TOP)/vendor/broadcom/bcm_platform/utils/include \
    $(TOP)/vendor/broadcom/bcm_platform/libnexusservice \
    $(TOP)/vendor/broadcom/bcm_platform/libnexusipc\
    $(TOP)/vendor/broadcom/bcm_platform/libgralloc \

COMMON_INCLUDE_FILES += ${NXCLIENT_INCLUDES}

COMMON_SHARED_LIBRARIES := \
	libc \
	libutils\
	liblog \
	$(NEXUS_LIB) \
	libnexusipcclient\
	libbcm_utils
	
ifneq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_SHARED_LIBRARIES += libnexusservice
endif

COMMON_C_FLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) 
COMMON_C_FLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

COMMON_C_FLAGS += -DENABLE_BCM_OMX_PROTOTYPE

LOCAL_MODULE_TAGS:= optional

COMMON_SOURCE_FILE:= \
	src/omx_audio.cpp

#
# libOMX.BCM.aac.decoder
#

include $(CLEAR_VARS)
LOCAL_C_INCLUDES+=$(COMMON_INCLUDE_FILES)
LOCAL_SHARED_LIBRARIES:=$(COMMON_SHARED_LIBRARIES)
LOCAL_CFLAGS := $(COMMON_C_FLAGS)
LOCAL_CFLAGS += -DOMX_COMPRESSION_FORMAT=OMX_AUDIO_CodingAAC
LOCAL_MODULE_TAGS:= optional
LOCAL_SRC_FILES:= $(COMMON_SOURCE_FILE)
LOCAL_MODULE:=libOMX.BCM.aac.decoder
include $(BUILD_SHARED_LIBRARY)

#
# libOMX.BCM.ac3.decoder
#
ifeq ($(BCM_OMX_SUPPORT_AC3_CODEC),y)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES+=$(COMMON_INCLUDE_FILES)
LOCAL_SHARED_LIBRARIES:=$(COMMON_SHARED_LIBRARIES)
LOCAL_CFLAGS := $(COMMON_C_FLAGS)
LOCAL_CFLAGS += -DOMX_COMPRESSION_FORMAT=OMX_AUDIO_CodingAC3
LOCAL_CFLAGS += -DBCM_OMX_SUPPORT_AC3_CODEC
LOCAL_MODULE_TAGS:= optional
LOCAL_SRC_FILES:= $(COMMON_SOURCE_FILE)
LOCAL_MODULE:=libOMX.BCM.ac3.decoder
include $(BUILD_SHARED_LIBRARY)
endif

endif
