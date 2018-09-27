# Copyright (C) 2017 Broadcom Canada
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

LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libutils \
                          libnexus \
                          libnexusir \
                          libnxclient
LOCAL_SHARED_LIBRARIES += \
                          libhidlbase \
                          libhidltransport \
                          libhwbinder \
                          bcm.hardware.nexus@1.0

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror
LOCAL_SRC_FILES := nxwrap.cpp
LOCAL_MODULE := libnxwrap
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
