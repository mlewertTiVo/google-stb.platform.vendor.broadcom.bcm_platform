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

# set to 'true' to avoid stripping symbols during build.
HWC_DEBUG_SYMBOLS := false

# build the binder helper.
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
   blib/IHwc.cpp \
   blib/Hwc.cpp \
   blib/IHwcListener.cpp \
   blib/HwcListener.cpp \
   blib/HwcSvc.cpp

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnativehelper
LOCAL_SHARED_LIBRARIES += libutils

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/blib

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhwcbinder

include $(BUILD_SHARED_LIBRARY)

# build the hwcbinder service
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
   bexe/main.cpp

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libhwcbinder
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/blib

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcbinder

include $(BUILD_EXECUTABLE)

# build the hwcutils helper
#
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SRC_FILES:= \
   utils/hwcutils.cpp

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/utils \
                    $(NXCLIENT_INCLUDES)

LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhwcutils

include $(BUILD_SHARED_LIBRARY)

# build the hwcomposer for the device.
#
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libhwcbinder
LOCAL_SHARED_LIBRARIES += libhwcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnexusipcclient
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libsync

LOCAL_C_INCLUDES += $(TOP)/vendor/broadcom/stb/bcm_platform/libnexusservice \
                    $(TOP)/vendor/broadcom/stb/bcm_platform/libnexusipc \
                    $(TOP)/vendor/broadcom/stb/bcm_platform/libgralloc \
                    $(NXCLIENT_INCLUDES) \
                    $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/blib \
                    $(TOP)/vendor/broadcom/stb/bcm_platform/libhwcomposer/utils \
                    $(TOP)/system/core/libsync \
                    $(TOP)/system/core/libsync/include

LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-hwc\"
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_SRC_FILES := hwcomposer.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.bcm_platform

include $(BUILD_SHARED_LIBRARY)
