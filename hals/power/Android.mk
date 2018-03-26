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
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_MODULE := power.$(TARGET_BOARD_PLATFORM)

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libbinder \
                          libcutils \
                          libdl \
                          liblog \
                          libnexus \
                          libnxcec \
                          libnexusir \
                          libnxclient \
                          libpmlibservice \
                          libpower \
                          libutils \
                          libhdmiext \
                          libhdmicec \
                          libnxwrap

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += libnxbinder \
                          libnxevtsrc
endif

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/droid_pm \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/linux/driver \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/modules/gpio/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hdmi_cec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/power
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/av/media/libstagefright/foundation/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SRC_FILES := nexus_power.cpp
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
LOCAL_SRC_FILES += treble/power.cpp
else
LOCAL_SRC_FILES += legacy/power.cpp
endif

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

include $(BUILD_SHARED_LIBRARY)
