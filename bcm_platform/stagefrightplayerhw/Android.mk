# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

REFSW_PATH :=vendor/broadcom/bcm_platform/brcm_nexus

LOCAL_PATH := $(call my-dir)
APPLIBS_TOP ?= $(LOCAL_PATH)/../../../../../../../..

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

ifeq ($(LIVEMEDIA_SUPPORT),y)
MP_CFLAGS += -DLIVEMEDIA_SUPPORT=1
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
ifeq ($(ANDROID_SUPPORTS_ANALOG_INPUT),y)
MP_CFLAGS += -DANDROID_SUPPORTS_ANALOG_INPUT
endif		
endif

ifeq ($(ANDROID_ENABLE_HDMI_LEGACY),y)
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=1
else
MP_CFLAGS += -DANDROID_SUPPORTS_HDMI_LEGACY=0
endif

ifneq ($(SPF_SUPPORT),y)
MP_CFLAGS += -DBCMPLAYER_RTSP_STCCHANNEL_SUPPORT
endif

# Test for version earlier than KK
JB_OR_EARLIER := $(shell test "${BRCM_ANDROID_VERSION}" \< "kk" && echo "y")

# KitKat introduced FUSE FS for interfacing to external sdcards.  FUSE does 
# not support the O_DIRECT option when opening files.
ifeq ($(JB_OR_EARLIER),y)
MP_CFLAGS += -DANDROID_SUPPORTS_O_DIRECT
endif

include $(DEVICE_REFSW_PLAY_BUILD_CONFIG)
$(warning NX-BUILD: CFG: ${DEVICE_REFSW_BUILD_CONFIG}, CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})


BCMPLAYER_SHARED_LIBRARIES := liblog libcutils libbinder libutils libb_os libb_playback_ip libb_psip libnexusipcclient $(NEXUS_LIB)

ifeq ($(LIVEMEDIA_SUPPORT),y)
BCMPLAYER_SHARED_LIBRARIES += libBasicUsageEnvironment libgroupsock libliveMedia
endif

BCMPLAYER_C_INCLUDES := $(REFSW_PATH)/bin/include \
                        $(REFSW_PATH)/../libnexusservice \
                        $(REFSW_PATH)/../libnexusipc \
                        $(REFSW_PATH)/../libgralloc \
                        $(NEXUS_TOP)/../rockford/lib/psip/include/ \
                        $(NEXUS_TOP)/../BSEAV/lib/mpeg2_ts_parse/

ifeq ($(LIVEMEDIA_SUPPORT),y)
BCMPLAYER_C_INCLUDES += $(REFSW_PATH)/bin/include/liveMedia \
                        $(REFSW_PATH)/bin/include/groupsock \
                        $(REFSW_PATH)/bin/include/BasicUsageEnvironment \
                        $(REFSW_PATH)/bin/include/UsageEnvironment
endif

BCMPLAYER_SRC_FILES := \
        bcmPlayer.cpp \
        bcmPlayer_record.cpp \
        bcmPlayer_base.cpp \
        bcmPlayer_local.cpp \
        bcmPlayer_ip.cpp \
        bcmPlayer_live.cpp \
        bcmPlayer_hdmiIn.cpp \
		bcmPlayer_analogIn.cpp \
        bcmPlayer_rawdata.cpp \
        get_ip_addr_from_url.c \
        stream_probe.c \
        bcmESPlayer.cpp \
        ip_psi.c

#if you want to get LOGV message, please add "-DLOG_NDEBUG=0" compiler flags behind "LOCAL_CFLAGS" define
BCMPLAYER_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID  -DFEATURE_GPU_ACCEL_H264_BRCM -DFEATURE_AUDIO_HWCODEC
BCMPLAYER_CFLAGS += $(addprefix -D,$(B_PLAYBACK_IP_LIB_DEFINES)) $(MP_CFLAGS)
BCMPLAYER_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

###
# First bcmPlayer instance
###
include $(CLEAR_VARS)
LOCAL_MODULE := bcmPlayer0

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := $(BCMPLAYER_SHARED_LIBRARIES)

LOCAL_C_INCLUDES := $(BCMPLAYER_C_INCLUDES)

LOCAL_SRC_FILES := $(BCMPLAYER_SRC_FILES)

LOCAL_CFLAGS := $(BCMPLAYER_CFLAGS)

include $(BUILD_SHARED_LIBRARY)

###
# Second bcmPlayer instance
###
include $(CLEAR_VARS)
LOCAL_MODULE := bcmPlayer1

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := $(BCMPLAYER_SHARED_LIBRARIES)

LOCAL_C_INCLUDES := $(BCMPLAYER_C_INCLUDES)

LOCAL_SRC_FILES := $(BCMPLAYER_SRC_FILES)

LOCAL_CFLAGS := $(BCMPLAYER_CFLAGS)

include $(BUILD_SHARED_LIBRARY)

###
# Third bcmPlayer instance
###
include $(CLEAR_VARS)
LOCAL_MODULE := bcmPlayer2

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := $(BCMPLAYER_SHARED_LIBRARIES)

LOCAL_C_INCLUDES := $(BCMPLAYER_C_INCLUDES)

LOCAL_SRC_FILES := $(BCMPLAYER_SRC_FILES)

LOCAL_CFLAGS := $(BCMPLAYER_CFLAGS)

include $(BUILD_SHARED_LIBRARY)
