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

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusservice
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/apks/BcmTVInput/jni
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsideband/include

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES := libcutils liblog libutils libnexus
LOCAL_SHARED_LIBRARIES += libnxclient libnexusipcclient libbinder
LOCAL_SHARED_LIBRARIES += libbcmsideband

LOCAL_SRC_FILES := tv_input.cpp
LOCAL_SRC_FILES += hdmi_tv_input.cpp

LOCAL_MODULE := tv_input.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
