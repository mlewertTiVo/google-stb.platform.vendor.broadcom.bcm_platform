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
ifneq ($(WIDEVINE_CLASSIC), n)
#---------------
# liboemcrypto.a
#---------------
include $(CLEAR_VARS)
LOCAL_MODULE := liboemcrypto
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := lib/arm64/liboemcrypto.a
LOCAL_SRC_FILES_arm := lib/arm/liboemcrypto.a
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm := lib/arm/liboemcrypto.a
endif
include $(BUILD_PREBUILT)
endif

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

# Check if a prebuilt library has been created in the release_prebuilts folder
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so))
# use prebuilt library if one exists
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
# TODO: fix me!
LOCAL_SRC_FILES_arm64 := ../../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_arm := ../../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm := ../../../../$(RELEASE_PREBUILTS)/$(LOCAL_MODULE).so
endif
include $(BUILD_PREBUILT)
else
# compile library from source
LOCAL_PATH := $(TOP)/vendor/widevine

# add SAGElib related includes
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc

LOCAL_SRC_FILES := \
    brcm_oemcrypto_L1/src/oemcrypto_brcm_TL.cpp\
    libwvdrmengine/cdm/core/src/string_conversions.cpp\
    libwvdrmengine/cdm/core/src/properties.cpp \
    libwvdrmengine/cdm/src/log.cpp \
    libwvdrmengine/third_party/stringencoders/src/modp_b64.cpp \
    libwvdrmengine/third_party/stringencoders/src/modp_b64w.cpp \


LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/external/boringssl/include \
    $(TOP)/vendor/widevine/libwvdrmengine/oemcrypto/include \
    $(TOP)/vendor/widevine/libwvdrmengine/cdm/core/include \
    $(TOP)/vendor/widevine/libwvdrmengine/third_party/stringencoders/src \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES)

LOCAL_CFLAGS += -DNDEBUG -DBRCM_IMPL
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DUSE_UNIFIED_COMMON_DRM

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both

LOCAL_SHARED_LIBRARIES := libnexus liblog
LOCAL_SHARED_LIBRARIES += libcmndrm_tl

include $(BUILD_SHARED_LIBRARY)
endif
endif
endif

