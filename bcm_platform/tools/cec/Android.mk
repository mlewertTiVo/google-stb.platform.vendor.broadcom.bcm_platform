# Copyright (C) 2015 The Android Open Source Project
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

LOCAL_SRC_FILES := \
    send_cec.cpp

LOCAL_SHARED_LIBRARIES := \
   libcutils \
   liblog \
   libcutils \
   libdl \
   libbinder \
   libutils \
   libnexusipcclient

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/bcm_platform/libnexusservice \
                    $(TOP)/vendor/broadcom/stb/bcm_platform//libnexusipc

LOCAL_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -Wno-multichar -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGI=ALOGI -DLOGV=ALOGV

LOCAL_MODULE := send_cec
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
