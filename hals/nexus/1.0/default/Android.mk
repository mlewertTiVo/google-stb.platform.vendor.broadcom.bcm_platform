# Copyright (C) 2017 The Android Open Source Project
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

LOCAL_SRC_FILES := \
   bcm/hardware/nexus/1.0/types.cpp \
   bcm/hardware/nexus/1.0/NexusAll.cpp \
   bcm/hardware/nexus/1.0/NexusDspCbAll.cpp \
   bcm/hardware/nexus/1.0/NexusHpdCbAll.cpp

LOCAL_SHARED_LIBRARIES := \
   libhidlbase \
   libhidltransport \
   libhwbinder \
   liblog \
   libutils \
   libcutils

LOCAL_MODULE := bcm.hardware.nexus@1.0-impl
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

