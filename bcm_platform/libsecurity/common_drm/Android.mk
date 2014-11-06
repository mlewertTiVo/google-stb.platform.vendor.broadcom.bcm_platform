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

#-------------
# libcmndrm.so
#-------------
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := ${DRM_BUILD_MODE}/libcmndrm.so
ifeq ($(SAGE_SUPPORT), y)
LOCAL_PREBUILT_LIBS += ${DRM_BUILD_MODE}/libcmndrm_tl.so
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

ifeq ($(ANDROID_SUPPORTS_PLAYREADY), y)
#-------------
# libcmndrmprdy.so
#-------------
include $(CLEAR_VARS)
ifeq ($(SAGE_SUPPORT), y)
LOCAL_PREBUILT_LIBS := ${DRM_BUILD_MODE}/libcmndrmprdy.so
else
LOCAL_PREBUILT_LIBS := ${DRM_BUILD_MODE}/non_sage/libcmndrmprdy.so
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)
endif
