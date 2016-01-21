# Copyright (C) 2015 Broadcom Canada Ltd.
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

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
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
	brcm_audio_nexus_direct.cpp \
	brcm_audio_builtin.cpp \
	brcm_audio_dummy.cpp \
	StandbyMonitorThread.cpp \

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
    frameworks/av/services/audiopolicy \
    frameworks/av/services/audiopolicy/engine/interface \
    frameworks/av/services/audiopolicy/common/managerdefinitions/include \
    frameworks/av/services/audiopolicy/common/include/

LOCAL_MODULE := libaudiopolicymanager
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif

# ATV Remote audio module
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    alsa_utils.cpp \
    AudioHardwareInput.cpp \
    AudioStreamIn.cpp \
    AudioHotplugThread.cpp \
    audio_hal_hooks.c \
    audio_hal_thunks.cpp

LOCAL_C_INCLUDES := \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libmedia \
    libtinyalsa \
    libaudioutils

LOCAL_STATIC_LIBRARIES += libmedia_helper

LOCAL_MODULE := audio.atvr.default
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
