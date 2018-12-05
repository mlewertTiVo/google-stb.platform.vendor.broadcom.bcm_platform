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
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libutils \
                          libnexus \
                          libnexusir \
                          libnxclient \
                          libpmlibservice
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0 \
                          bcm.hardware.nexus@1.1 \
                          libhidlbase \
                          libhidltransport \
                          libhwbinder \
                          libpower

LOCAL_STATIC_LIBRARIES := libnxserver_vendor

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/server
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem
LOCAL_C_INCLUDES += $(TOP)/system/core/base/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SRC_FILES := nxserver.cpp
LOCAL_SRC_FILES += nxinexus.cpp
ifneq ($(ANDROID_ENABLE_HDMI_HDCP),n)
LOCAL_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=1
else
LOCAL_CFLAGS += -DANDROID_ENABLE_HDMI_HDCP=0
endif
ifneq ($(HW_ENCODER_SUPPORT),n)
LOCAL_SRC_FILES += nxserver_with_encoder.cpp
else
LOCAL_SRC_FILES += nxserver_stub_encoder.cpp
endif
ifeq ($(NEXUS_SECURITY_SUPPORT),n)
LOCAL_SRC_FILES += nxserver_stub_security.cpp
else
LOCAL_SRC_FILES += nxserver_with_security.cpp
ifeq ($(NEXUS_SECURITY_API_VERSION),2)
LOCAL_CFLAGS += -DSECV2
endif
endif

ifeq ($(LOCAL_NVI_LAYOUT),y)
LOCAL_INIT_RC := nxserver.nvi.rc
else
LOCAL_INIT_RC := nxserver.rc
endif

LOCAL_MODULE := nxserver
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES

include $(BUILD_EXECUTABLE)
