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

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
DRM_BUILD_MODE := linuxkernel
else
DRM_BUILD_MODE := linuxuser
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(call all-makefiles-under,$(LOCAL_PATH))

ifneq ($(WIDEVINE_CLASSIC),n)
ifeq ($(BRCM_ANDROID_VERSION), l)
$(error "Widevine Classic should not be turned on for L!!!!!!")
endif
#-------------------
# libdrmwvmplugin.so
#-------------------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../widevine/proprietary/drmwvmplugin/plugin-core.mk

LOCAL_MODULE := libdrmwvmplugin
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/drm
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := liboemcrypto
LOCAL_SHARED_LIBRARIES += $(NEXUS_LIB) libcmndrm libnexusipcclient
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#----------
# libwvm.so
#----------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../widevine/proprietary/wvm/wvm-core.mk

LOCAL_MODULE := libwvm
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := liboemcrypto
LOCAL_SHARED_LIBRARIES += $(NEXUS_LIB) libcmndrm
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#-----------------
# libdrmdecrypt.so
#-----------------
include $(CLEAR_VARS)
include $(WV_LOCAL_PATH)/../../../widevine/proprietary/cryptoPlugin/decrypt-core.mk

LOCAL_C_INCLUDES := \
	$(TOP)/frameworks/native/include/media/hardware \
	$(TOP)/vendor/widevine/proprietary/cryptoPlugin

LOCAL_SHARED_LIBRARIES := \
	libstagefright_foundation \
	liblog

LOCAL_MODULE := libdrmdecrypt
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES := liboemcrypto
LOCAL_SHARED_LIBRARIES += $(NEXUS_LIB) libcmndrm libcrypto libcutils
include $(BUILD_SHARED_LIBRARY)
endif

