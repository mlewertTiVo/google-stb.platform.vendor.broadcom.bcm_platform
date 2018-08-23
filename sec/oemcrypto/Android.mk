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

ifneq ($(ANDROID_SUPPORTS_WIDEVINE), n)

ifeq ($(SAGE_SUPPORT), y)
#---------------
# liboemcrypto.so for Modular DRM
#---------------
include $(CLEAR_VARS)
LOCAL_MODULE := liboemcrypto
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional

ifeq ($(TARGET_BUILD_VARIANT),user)
RELEASE_PREBUILTS := release_prebuilts/user
else
RELEASE_PREBUILTS := release_prebuilts/userdebug
endif
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
RELEASE_PREBUILTS := ${RELEASE_PREBUILTS}_treble
endif

# Check if a prebuilt library has been created in the release_prebuilts folder
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so))
# use prebuilt library if one exists
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
# fix me!
LOCAL_SRC_FILES_arm64 := ../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_arm := ../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
# fix me!
LOCAL_SRC_FILES_arm64 := ../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_arm := ../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
endif
include $(BUILD_PREBUILT)
else
# compile library from source
LOCAL_PATH := $(TOP)/vendor
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc

LOCAL_SRC_FILES := \
    broadcom/bcm_platform/sec-priv/brcm_oemcrypto_L1/src/oemcrypto_brcm_TL.cpp\
    widevine/libwvdrmengine/cdm/core/src/string_conversions.cpp\
    widevine/libwvdrmengine/cdm/core/src/properties.cpp \
    widevine/libwvdrmengine/cdm/src/log.cpp \
    broadcom/bcm_platform/sec-priv/brcm_oemcrypto_L1/tp/stringencoders/src/modp_b64.cpp \
    broadcom/bcm_platform/sec-priv/brcm_oemcrypto_L1/tp/stringencoders/src/modp_b64w.cpp \


LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libutils/include \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/bionic \
    $(TOP)/external/boringssl/include \
    $(TOP)/vendor/broadcom/bcm_platform/sec-priv/brcm_oemcrypto_L1/include \
    $(TOP)/vendor/widevine/libwvdrmengine/cdm/core/include \
    $(TOP)/vendor/widevine/brcm_oemcrypto_L1/tp/stringencoders/src \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES) \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
    $(NXCLIENT_INCLUDES)

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
LOCAL_C_INCLUDES += \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
else
LOCAL_C_INCLUDES += \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += -DNDEBUG -DBRCM_IMPL
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
ifeq ($(SAGE_VERSION),2x)
LOCAL_CFLAGS += -DUSE_UNIFIED_COMMON_DRM
endif
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both

LOCAL_SHARED_LIBRARIES := libnexus liblog
LOCAL_SHARED_LIBRARIES += libcmndrm_tl
LOCAL_SHARED_LIBRARIES += libnxwrap

ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += libnxbinder libnxevtsrc
endif

include $(BUILD_SHARED_LIBRARY)
endif
endif
endif
