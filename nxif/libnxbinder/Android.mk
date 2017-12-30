# Copyright (C) 2017 Broadcom Canada.
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
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SRC_FILES:= \
   INxServer.cpp \
   NxServer.cpp \
   NxServerSvc.cpp

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

ifneq ($(ANDROID_ENABLE_HDMI_HDCP),n)
LOCAL_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=1
else
LOCAL_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=0
endif

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnativehelper
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnexusir
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libnxevtsrc

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libnxbinder
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
