# Copyright (C) 2016 The Android Open Source Project
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

LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libutils \
                          libhwcbinder \
                          libbinder

# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/common/blib \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop \
                    system/core/base/include
LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_SRC_FILES := togplm.cpp
LOCAL_INIT_RC := togplm.rc
LOCAL_MODULE := togplm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
include $(BUILD_EXECUTABLE)
