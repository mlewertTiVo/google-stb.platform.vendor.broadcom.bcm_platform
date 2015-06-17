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

include $(TOP)/vendor/broadcom/stb/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libnexus \
                          libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) $(NEXUS_TOP)/platforms/common/src/linuxuser.proxy $(NEXUS_TOP)/platforms/common/src/linuxkernel/common $(NEXUS_TOP)/base/include
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils

LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))

LOCAL_SRC_FILES := nxlogger.cpp

LOCAL_MODULE := nxlogger
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
