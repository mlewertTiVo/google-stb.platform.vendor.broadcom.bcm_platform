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

ifeq ($(ANDROID_SUPPORTS_MEDIACAS),y)

LOCAL_PATH := $(call my-dir)
CAS_HAL_ROOT := ../../../../../../../hardware/interfaces/cas/1.0/default

include $(CLEAR_VARS)
LOCAL_MODULE := bcm.hardware.casproxy@1.0-service
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_CPPFLAGS := -Wall -Werror -Wextra
LOCAL_INIT_RC := bcm.hardware.casproxy@1.0-service.rc
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := \
    service.cpp \
    MediaCasProxyService.cpp \
    $(CAS_HAL_ROOT)/CasImpl.cpp \
    $(CAS_HAL_ROOT)/DescramblerImpl.cpp \
    $(CAS_HAL_ROOT)/SharedLibrary.cpp \
    $(CAS_HAL_ROOT)/TypeConvert.cpp

LOCAL_CFLAGS += -DBCM_FULL_TREBLE

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/${CAS_HAL_ROOT}

LOCAL_HEADER_LIBRARIES := \
    liblog_headers \
    libstagefright_foundation_headers \
    media_plugin_headers

LOCAL_SHARED_LIBRARIES := \
    bcm.hardware.casproxy@1.0 \
    android.hardware.cas@1.0 \
    android.hardware.cas.native@1.0 \
    android.hidl.memory@1.0 \
    libbinder \
    libhidlbase \
    libhidlmemory \
    libhidltransport \
    liblog \
    libutils

include $(BUILD_EXECUTABLE)

endif # ANDROID_SUPPORTS_MEDIACAS
