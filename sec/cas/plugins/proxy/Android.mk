#
# Copyright (C) 2018 The Android Open Source Project
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
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libcasproxyplugin
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := mediacas
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CFLAGS += -DBCM_FULL_TREBLE

LOCAL_HEADER_LIBRARIES := \
    liblog_headers \
    libstagefright_foundation_headers \
    media_plugin_headers

LOCAL_SHARED_LIBRARIES := \
    android.hardware.cas@1.0 \
    android.hardware.cas.native@1.0 \
    android.hidl.memory@1.0 \
    libbinder \
    libhidlallocatorutils \
    libhidlbase \
    libhidltransport \
    liblog \
    libstagefright_foundation \
    libutils \
    bcm.hardware.casproxy@1.0

LOCAL_SRC_FILES := \
    CasProxyPlugin.cpp

include $(BUILD_SHARED_LIBRARY)
