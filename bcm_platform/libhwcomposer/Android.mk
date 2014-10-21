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

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libbinder libutils libnexusipcclient libnexus libnxclient

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/bcm_platform/libnexusservice \
                    $(TOP)/vendor/broadcom/bcm_platform/libnexusipc \
                    $(TOP)/vendor/broadcom/bcm_platform/libgralloc \
                    $(NXCLIENT_INCLUDES)
		
FILTER_OUT_NEXUS_CFLAGS := -march=armv7-a
LOCAL_CFLAGS += $(filter-out $(FILTER_OUT_NEXUS_CFLAGS), $(NEXUS_CFLAGS))			
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-hwc\"
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_SRC_FILES := hwcomposer.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.bcm_platform

include $(BUILD_SHARED_LIBRARY)
