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

ifeq ($(ANDROID_SUPPORTS_WIDEVINE),y)
ifneq ($(BUILD_WIDEVINE_CLASSIC_FROM_SOURCE),y)
# By default, prebuilt libraries are installed

LOCAL_PATH := $(call my-dir)

########################
# Feature file for clients to look up widevine drm plug-in

include $(CLEAR_VARS)
LOCAL_MODULE := com.google.widevine.software.drm.xml
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := widevine
LOCAL_MODULE_CLASS := ETC

# This will install the file in /system/etc/permissions
#
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions

include $(BUILD_PREBUILT)

########################
# Dummy library used to indicate availability of widevine drm

include $(CLEAR_VARS)
LOCAL_MODULE := com.google.widevine.software.drm
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE).jar
LOCAL_MODULE_PATH := $(TARGET_OUT)/framework
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := widevine
LOCAL_MODULE_SUFFIX := .jar
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
include $(BUILD_PREBUILT)

#-------------
# libdrmwvmplugin.so
#-------------

include $(CLEAR_VARS)
LOCAL_SRC_FILES := lib/libdrmwvmplugin.so
LOCAL_MODULE := libdrmwvmplugin
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/drm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)

#----------
# libwvm.so
#----------
include $(CLEAR_VARS)
LOCAL_SRC_FILES := lib/libwvm.so
LOCAL_MODULE := libwvm
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)

#----------
# libWVStreamControlAPI_L3.so
#----------
include $(CLEAR_VARS)
LOCAL_SRC_FILES := lib/libWVStreamControlAPI_L3.so
LOCAL_MODULE := libWVStreamControlAPI_L3
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)

#----------
# libwvdrm_L3.so
#----------
include $(CLEAR_VARS)
LOCAL_SRC_FILES := lib/libwvdrm_L3.so
LOCAL_MODULE := libwvdrm_L3
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)

#-----------------
# libdrmdecrypt.so
#-----------------
include $(CLEAR_VARS)
LOCAL_SRC_FILES := lib/libdrmdecrypt.so
LOCAL_MODULE := libdrmdecrypt
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)

endif # end of BUILD_WIDEVINE_CLASSIC_FROM_SOURCE!=y
endif # end of ANDROID_SUPPORTS_WIDEVINE=y
