ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)

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

include ${REFSW_BASE_DIR}/nexus/lib/playback_ip/b_playback_ip_lib.inc

COMMON_INCLUDE_FILES=   $(TOP)/frameworks/native/include/media/openmax \
                        $(TOP)/frameworks/native/include/utils \
                        $(TOP)/frameworks/native/include/media/hardware \
                        $(TOP)/hardware/libhardware/include/hardware \
                        $(TOP)/vendor/broadcom/bcm_platform/omx_core/inc \
                        $(TOP)/vendor/broadcom/bcm_platform/utils/include \
                        $(TOP)/vendor/broadcom/bcm_platform/brcm_nexus/bin/include \
                        $(TOP)/vendor/broadcom/bcm_platform/libnexusservice \
                        $(TOP)/vendor/broadcom/bcm_platform/libnexusipc \
                        $(TOP)/vendor/broadcom/bcm_platform/libgralloc
			
COMMON_SHARED_LIBRARIES=    libc \
                            libutils\
                            liblog \
                            $(NEXUS_LIB) \
                            libnexusipcclient \
                            libbcm_utils

COMMON_SOURCE_FILE= src/omx_video_encoder.cpp 

ifneq ($(ANDROID_SUPPORTS_NXCLIENT),y)
COMMON_SHARED_LIBRARIES += libnexusservice
endif

COMMON_C_FLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))
COMMON_C_FLAGS += $(addprefix -I,$(B_PLAYBACK_IP_LIB_PUBLIC_INCLUDES))
COMMON_C_FLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
COMMON_C_FLAGS += -DENABLE_BCM_OMX_PROTOTYPE
COMMON_C_FLAGS += -DBCM_OMX_SUPPORT_ENCODER

#
# libOMX.BCM.h264.ENCODER
#
include $(CLEAR_VARS)
LOCAL_C_INCLUDES+=$(COMMON_INCLUDE_FILES)
LOCAL_SHARED_LIBRARIES:=$(COMMON_SHARED_LIBRARIES)
LOCAL_CFLAGS := $(COMMON_C_FLAGS)
LOCAL_CFLAGS += -DOMX_COMPRESSION_FORMAT=OMX_VIDEO_CodingAVC
LOCAL_MODULE_TAGS:= optional
LOCAL_SRC_FILES:= $(COMMON_SOURCE_FILE)
LOCAL_MODULE:=libOMX.BCM.h264.encoder
include $(BUILD_SHARED_LIBRARY)

endif
endif

