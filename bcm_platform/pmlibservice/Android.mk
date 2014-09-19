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


REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus

LOCAL_PATH := $(call my-dir)

ifeq ($(shell test -d vendor/broadcom/refsw/BSEAV && echo "y"),y)
BSEAV_INC_PATH := vendor/broadcom/refsw/BSEAV
BSEAV_TOP := ../../refsw/BSEAV
else
BSEAV_INC_PATH := $(LOCAL_PATH)
BSEAV_TOP  ?= ../../../../../../../../../BSEAV
endif

GLOB_PATH  := $(BSEAV_TOP)/lib/glob
POWER_PATH := $(BSEAV_TOP)/lib/power_standby

$(warning NX-BUILD: CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

# Linux 3.3 supports the /sys/devices/platform/brcmstb sysfs layout.
PMLIB_SUPPORTS_BRCMSTB_SYSFS := $(shell test "${LINUXVER}" \< "3.8.0" && echo "y")

include $(CLEAR_VARS)

LOCAL_MODULE := libpmlibservice

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libbinder libutils 

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include \
LOCAL_C_INCLUDES += $(BSEAV_INC_PATH)/$(GLOB_PATH) \
                    $(BSEAV_INC_PATH)/$(POWER_PATH)
                    
LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

ifeq ($(PMLIB_SUPPORTS_BRCMSTB_SYSFS), y)
LOCAL_CFLAGS += -DPMLIB_SUPPORTS_BRCMSTB_SYSFS=1
endif

LOCAL_SRC_FILES := IPmLibService.cpp \
                   PmLibService.cpp \
                   $(GLOB_PATH)/glob.c \
                   $(POWER_PATH)/pmlib-263x.c

LOCAL_MODULE_TAGS := optional debug

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

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include

LOCAL_CFLAGS:= $(NEXUS_CFLAGS) -DANDROID

LOCAL_MODULE_TAGS := optional debug

LOCAL_MODULE:= pmlibserver

include $(BUILD_EXECUTABLE)
