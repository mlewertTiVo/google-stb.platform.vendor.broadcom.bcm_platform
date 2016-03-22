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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := \
   liblog \
   libcutils \
   libutils \
   libbinder \
   libnexusipceventlistener \
   libnexusir \
   libnexus \
   libstagefright_foundation \
   libnxclient

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils

LOCAL_SRC_FILES += \
    nexusservice.cpp \
    nexusnxservice.cpp \
    nexuscecservice.cpp \
    nexusnxcecservice.cpp

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

ifneq ($(ANDROID_ENABLE_HDMI_HDCP),n)
LOCAL_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=1
else
LOCAL__CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=0
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexusservice
include $(BUILD_SHARED_LIBRARY)
