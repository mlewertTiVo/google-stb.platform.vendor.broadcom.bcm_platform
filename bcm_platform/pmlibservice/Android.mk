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

BSEAV_INC_PATH := ${BCM_VENDOR_STB_ROOT}/refsw/BSEAV
LOCAL_BSEAV_TOP := ../../refsw/BSEAV
GLOB_PATH  := $(LOCAL_BSEAV_TOP)/lib/glob

include $(CLEAR_VARS)
include $(BSEAV_INC_PATH)/lib/pmlib/pmlib.inc

POWER_PATH := $(LOCAL_BSEAV_TOP)/lib/pmlib/$(PMLIB_DIR)

LOCAL_MODULE := libpmlibservice

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libbinder libutils 

LOCAL_C_INCLUDES += $(BSEAV_INC_PATH)/$(GLOB_PATH) \
                    $(BSEAV_INC_PATH)/$(POWER_PATH)
                    
LOCAL_CFLAGS := $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += $(PMLIB_CFLAGS)

LOCAL_SRC_FILES := IPmLibService.cpp \
                   PmLibService.cpp \
                   $(GLOB_PATH)/glob.c \
                   $(POWER_PATH)/pmlib.c

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= main_pmlibserver.cpp 

LOCAL_SHARED_LIBRARIES := \
    libpmlibservice \
    liblog \
    libcutils \
    libutils \
    libbinder

LOCAL_CFLAGS := $(NEXUS_CFLAGS) -DANDROID
LOCAL_CFLAGS += $(PMLIB_CFLAGS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= pmlibserver

include $(BUILD_EXECUTABLE)
