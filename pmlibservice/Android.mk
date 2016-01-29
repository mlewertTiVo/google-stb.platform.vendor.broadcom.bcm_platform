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
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/pmlib/pmlib.inc

LOCAL_MODULE := libpmlibservice

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libbinder libutils 

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/glob \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/pmlib/$(PMLIB_DIR)
                    
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(PMLIB_CFLAGS)

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := bcm_platform/pmlibservice/IPmLibService.cpp \
                   bcm_platform/pmlibservice/PmLibService.cpp \
                   refsw/BSEAV/lib/glob/glob.c \
                   refsw/BSEAV/lib/pmlib/$(PMLIB_DIR)/pmlib.c

LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
