# Copyright (C) 2014 The Android Open Source Project
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
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusservice
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/blib

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_SHARED_LIBRARIES := libcutils liblog libutils libnexus
LOCAL_SHARED_LIBRARIES += libnxclient libnexusipcclient libbinder libhwcbinder

LOCAL_SRC_FILES := TunerHAL.cpp

ifeq ($(ANDROID_SUPPORTS_DTVKIT),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/DTVKitPlatform/inc
LOCAL_C_INCLUDES += $(TOP)/external/DVBCore/inc
LOCAL_C_INCLUDES += $(TOP)/external/DVBCore/dvb/inc
LOCAL_C_INCLUDES += $(TOP)/external/DVBCore/dvb/src
LOCAL_C_INCLUDES += $(TOP)/external/DVBCore/midware/stb/inc
LOCAL_C_INCLUDES += $(TOP)/external/DVBCore/platform/inc
LOCAL_STATIC_LIBRARIES := libdtvkitdvbcore libdtvkitdvb_hw libdtvkitdvb_os libdtvkitdvb_version
LOCAL_SHARED_LIBRARIES += libpng libjpeg
LOCAL_SRC_FILES += BroadcastDTVKit.cpp
else
LOCAL_SRC_FILES += BroadcastDemo.cpp
endif

LOCAL_MODULE := libjni_tuner
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
