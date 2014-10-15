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
NEXUS_TOP ?= $(LOCAL_PATH)/../../../../../../../../../nexus

include $(CLEAR_VARS)

# Nexus multi-process, client-server related CFLAGS
MP_CFLAGS = -DANDROID_CLIENT_SECURITY_MODE=$(ANDROID_CLIENT_SECURITY_MODE)
MP_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT
MP_CFLAGS += -DANDROID_UNDER_LXC=0
MP_CFLAGS += -DANDROID_SUPPORTS_FRONTEND_SERVICE=0
MP_CFLAGS += -DATP_BUILD=0

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libnexusipcclient \
                          libnexusservice \
                          libnexus \
                          libnxclient

LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_SRC_FILES := \
                nexusinit.cpp

LOCAL_MODULE := nexusinit
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
ifeq ($(BCM_OMX_SUPPORT_ENCODER),y)
LOCAL_CFLAGS += -DBCM_OMX_SUPPORT_ENCODER
endif

include $(BUILD_EXECUTABLE)
