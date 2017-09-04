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
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

# Audio module
include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_STRIP_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES := \
    libmedia_helper \
    liblog \
    libutils \
    libcutils \
    libmedia \
    libnexus \
    libnxclient \
    libbomx_util \
    libtinyalsa \
    libaudioutils

LOCAL_STATIC_LIBRARIES := \
    audio.atvr.default

LOCAL_SRC_FILES += \
	brcm_audio.cpp \
	brcm_audio_nexus.cpp \
	brcm_audio_nexus_direct.cpp \
	brcm_audio_nexus_hdmi.cpp \
	brcm_audio_nexus_parser.cpp \
	brcm_audio_nexus_tunnel.cpp \
	brcm_audio_builtin.cpp \
	brcm_audio_atvr.cpp \
	brcm_audio_dummy.cpp \
	StandbyMonitorThread.cpp \

LOCAL_CFLAGS := -DLOG_TAG=\"bcm-audio\"
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES += $(REFSW_BASE_DIR)/nexus/nxclient/include
LOCAL_C_INCLUDES += $(REFSW_BASE_DIR)/BSEAV/lib/media
LOCAL_C_INCLUDES += $(REFSW_BASE_DIR)/BSEAV/lib/utils
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include/media/openmax
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefrighthw
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(call include-path-for, audio-utils)
LOCAL_C_INCLUDES += external/tinyalsa/include
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
endif
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

include $(BUILD_SHARED_LIBRARY)

# Audio Policy Manager
ifeq ($(USE_CUSTOM_AUDIO_POLICY), 1)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    BrcmAudioPolicyManager.cpp

LOCAL_SHARED_LIBRARIES := \
    libmedia_helper \
    liblog \
    libcutils \
    liblog \
    libutils \
    libmedia \
    libaudiopolicymanagerdefault

ifneq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += \
    libnxbinder
endif

LOCAL_C_INCLUDES := \
    external/tinyalsa/include \
    frameworks/av/services/audiopolicy \
    frameworks/av/services/audiopolicy/engine/interface \
    frameworks/av/services/audiopolicy/common/managerdefinitions/include \
    frameworks/av/services/audiopolicy/common/include/

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE := libaudiopolicymanager
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
endif

# ATV Remote audio module
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    alsa_utils.cpp \
    AudioHardwareInput.cpp \
    AudioStreamIn.cpp \
    AudioHotplugThread.cpp \
    audio_hal_thunks.cpp

LOCAL_C_INCLUDES := \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

LOCAL_SHARED_LIBRARIES := \
    libmedia_helper \
    liblog \
    libcutils \
    libutils \
    libmedia \
    libtinyalsa \
    libaudioutils

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE := audio.atvr.default
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)
