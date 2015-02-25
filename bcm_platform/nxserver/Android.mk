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

include $(TOP)/vendor/broadcom/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libnexusipcclient \
                          libnexusservice \
                          libnexus \
                          libnxclient

LOCAL_STATIC_LIBRARIES := libnxserver

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusservice
LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusipc
LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusir
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/refsw/nexus/nxclient/server

LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))
ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)
LOCAL_CFLAGS += -DBCM_OMX_SUPPORT_ENCODER
endif
ifeq ($(V3D_USES_MMA),y)
LOCAL_CFLAGS += -DUSE_MMA
endif

LOCAL_SRC_FILES := nxserver.cpp

LOCAL_MODULE := nxserver
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)