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
                          libnxwrap \
                          libnxbinder \
                          libnxcec \
                          libnxevtsrc \
                          libnexusir \
                          libnxclient \
                          libpmlibservice \
                          libpower \
                          libutils \
                          libhdmiext \
                          libhdmicec

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/droid_pm \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/linux/driver \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/modules/gpio/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hdmi_cec
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SRC_FILES := power.cpp \
                   nexus_power.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw

include $(BUILD_SHARED_LIBRARY)
