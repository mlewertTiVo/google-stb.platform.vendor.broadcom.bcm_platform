# Copyright (C) 2014 The Android Open Source Project
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

PREFIX_RELATIVE_PATH := ../../../../
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(ANDROID)/frameworks/native/libs/arect/include \
                    $(ANDROID)/frameworks/native/libs/nativewindow/include \
                    $(ANDROID)/frameworks/native/libs/nativebase/include \
                    $(ANDROID)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(ANDROID)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils \
                    $(ANDROID)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/tshdrbuilder

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES :=  \
    libcutils \
    liblog \
    libutils \
    libnexus \
    libnxclient \
    libbinder \
    libandroid \
    bcm.hardware.dspsvcext@1.0

LOCAL_SRC_FILES := \
    bcmtuner.cpp \
    $(PREFIX_RELATIVE_PATH)refsw/nexus/utils/namevalue.c \
    $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/live_decode.c \
    $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/live_source.c \
    $(PREFIX_RELATIVE_PATH)refsw/nexus/nxclient/apps/utils/tspsimgr3.c \

LOCAL_MODULE := libbcmtuner
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
