# Copyright (C) 2008-2017 The Android Open Source Project
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

# hwc version: 1.x (default) for pre-N android; 2.0 starting with N.
# in N, both v-1.x and v-2.0 continue to be supported.
ifeq ($(HAL_HWC_VERSION),)
HAL_HWC_VERSION := v-1.x
endif

# set to 'true' to avoid stripping symbols during build.
HWC_DEBUG_SYMBOLS := false

# build the binder helper.
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
   common/blib/IHwc.cpp \
   common/blib/Hwc.cpp \
   common/blib/IHwcListener.cpp \
   common/blib/HwcListener.cpp \
   common/blib/HwcSvc.cpp

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnativehelper
LOCAL_SHARED_LIBRARIES += libutils

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhwcbinder

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)

# build the hwcbinder service
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
   common/bexe/main.cpp

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libhwcbinder
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcbinder

ifeq ($(LOCAL_NVI_LAYOUT),y)
LOCAL_INIT_RC := common/hwcbinder.nvi.rc
else
LOCAL_INIT_RC := common/hwcbinder.rc
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES

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

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem \
                    $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libhwcutils

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)

# build the hwcomposer for the device.
#
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

ifeq ($(HWC_DEBUG_SYMBOLS),true)
LOCAL_STRIP_MODULE := false
endif
LOCAL_PRELINK_MODULE := false

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libhwcbinder
LOCAL_SHARED_LIBRARIES += libhwcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxevtsrc
LOCAL_SHARED_LIBRARIES += libnxbinder
LOCAL_SHARED_LIBRARIES += libnxwrap
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libsync

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc/${HAL_GR_VERSION} \
                    $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/utils \
                    $(TOP)/system/core/libsync \
                    $(TOP)/system/core/libsync/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-hwc\"
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

ifeq ($(LOCAL_DEVICE_TYPE),blemmyes)
LOCAL_SRC_FILES := $(HAL_HWC_VERSION)/hwcomposer.blemmyes.cpp
else
# fix warnings!
LOCAL_CFLAGS += -Werror
LOCAL_SRC_FILES := $(HAL_HWC_VERSION)/hwcomposer.cpp
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)

include $(BUILD_SHARED_LIBRARY)
