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
NEXUS_LIB=libnexus

include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusipc
LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusservice

LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_SHARED_LIBRARIES := libcutils liblog libutils $(NEXUS_LIB)
LOCAL_SHARED_LIBRARIES += libnxclient libnexusipcclient

LOCAL_SRC_FILES := tv_input.cpp
LOCAL_MODULE := tv_input.bcm_platform
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
