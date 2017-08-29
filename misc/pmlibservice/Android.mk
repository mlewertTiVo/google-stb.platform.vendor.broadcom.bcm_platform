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
#include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/pmlib/pmlib.inc
PMLIB_DIR    := 314
PMLIB_CFLAGS := -DPMLIB_VER=$(PMLIB_DIR)

LOCAL_MODULE := libpmlibservice

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libbinder libutils

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/opensource/glob \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/pmlib/$(PMLIB_DIR)

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(PMLIB_CFLAGS)

# fix warnings!
LOCAL_CFLAGS += -Werror
LOCAL_CFLAGS += -DB_REFSW_ANDROID
FILTER_OUT_LOCAL_CFLAGS := -Wno-implicit-function-declaration
LOCAL_CPPFLAGS := $(LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(filter-out $(FILTER_OUT_LOCAL_CFLAGS), $(LOCAL_CPPFLAGS))

# M onward: sdk version >= 23.
ifneq (1,$(filter 1,$(shell echo "$$(( $(PLATFORM_SDK_VERSION) >= 23 ))" )))
LOCAL_CFLAGS += -DIP_CTRL_CMD_FOR_SDK=\"netcfg\"
else
LOCAL_CFLAGS += -DIP_CTRL_CMD_FOR_SDK=\"ifconfig\"
endif

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := bcm_platform/misc/pmlibservice/IPmLibService.cpp \
                   bcm_platform/misc/pmlibservice/PmLibService.cpp \
                   refsw/BSEAV/opensource/glob/glob.c \
                   refsw/BSEAV/lib/pmlib/$(PMLIB_DIR)/pmlib.c

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
