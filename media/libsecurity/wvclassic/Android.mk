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
WV_LOCAL_PATH := $(LOCAL_PATH)

ifneq ($(ANDROID_SUPPORTS_WIDEVINE),n)
ifeq ($(BUILD_WIDEVINE_CLASSIC_FROM_SOURCE),y)
# this requires widevine classic plugin source be placed under
# vendor/widevine/proprietary
#-------------------
# libdrmwvmplugin.so
#-------------------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../../../widevine/proprietary/drmwvmplugin/plugin-core.mk

LOCAL_MODULE := libdrmwvmplugin
ifeq ($(TARGET_2ND_ARCH),arm)
  LOCAL_MODULE_RELATIVE_PATH := drm
else
  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/drm
endif
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := liboemcrypto
LOCAL_SHARED_LIBRARIES += libnexus libcmndrm libnexusipcclient
LOCAL_PRELINK_MODULE := false
# LOCAL_MULTILIB := both
LOCAL_MULTILIB := 32
include $(BUILD_SHARED_LIBRARY)

#----------
# libwvm.so
#----------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../../../widevine/proprietary/wvm/wvm-core.mk

LOCAL_MODULE := libwvm
# LOCAL_MULTILIB := both
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#-----------------
# libdrmdecrypt.so
#-----------------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../../../widevine/proprietary/cryptoPlugin/decrypt-core.mk

LOCAL_C_INCLUDES := \
	$(TOP)/frameworks/native/include/media/hardware \
	$(TOP)/vendor/widevine/proprietary/cryptoPlugin

LOCAL_SHARED_LIBRARIES := \
	libstagefright_foundation \
	liblog

# LOCAL_MULTILIB := both
LOCAL_MULTILIB := 32
LOCAL_MODULE := libdrmdecrypt
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES += libcrypto libcutils
include $(BUILD_SHARED_LIBRARY)
endif # end of BUILD_WIDEVINE_CLASSIC_FROM_SOURCE=y
endif # end of ANDROID_SUPPORTS_WIDEVINE=n
