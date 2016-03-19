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
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

#----------------------
# libnexusipceventlistener 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder

LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include

LOCAL_SRC_FILES := INexusHdmiCecMessageEventListener.cpp \
                   INexusHdmiHotplugEventListener.cpp \
                   INexusDisplaySettingsChangedEventListener.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexusipceventlistener

include $(BUILD_SHARED_LIBRARY)

#----------------------
# libnexusipcclient 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipceventlistener libnexus
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice \
                    $(TOP)/frameworks/native/include

#
# Switch between the NXclient APIs or the local server APIs
# 
LOCAL_SRC_FILES := nexus_ipc_client_factory.cpp nexus_ipc_client_base.cpp nexus_ipc_client.cpp nexus_nx_client.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexusipcclient
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_SHARED_LIBRARIES += libnxclient

include $(BUILD_SHARED_LIBRARY)

#----------------------
# libnexuseglclient 
#----------------------
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipcclient libnexusipceventlistener libnexus

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice \
                    $(TOP)/frameworks/native/include

LOCAL_SRC_FILES := nexus_egl_client.cpp

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnexuseglclient

include $(BUILD_SHARED_LIBRARY)
