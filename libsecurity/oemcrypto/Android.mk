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
LOCAL_PREBUILT_LIBS := prebuilt/liboemcrypto.a
LOCAL_SHARED_LIBRARIES := libcmndrm libdrmrootfs
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)
endif

ifeq ($(SAGE_SUPPORT), y)
#---------------
# liboemcrypto.so for Modular DRM
#---------------
include $(CLEAR_VARS)
LOCAL_MODULE := liboemcrypto
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional

# Check if a prebuilt library has been created in the release_prebuilts folder
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/release_prebuilts/$(LOCAL_MODULE).so))
# use prebuilt library if one exists
LOCAL_SRC_FILES := ../../../release_prebuilts/$(LOCAL_MODULE).so
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)
else
# compile library from source
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/security/third_party/widevine/CENC21

# add SAGElib related includes
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc

LOCAL_SRC_FILES := \
    test_sage/src/oemcrypto_brcm_TL.cpp\
    core/src/string_conversions.cpp\
    core/src/properties.cpp \
    linux/src/log.cpp \
    third_party/stringencoders/src/modp_b64w.cpp

LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/external/boringssl/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libsecurity/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/third_party/widevine/CENC21/test_sage/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/third_party/widevine/CENC21/core/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/third_party/widevine/CENC21/third_party/stringencoders/src \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES)

LOCAL_CFLAGS += -DNDEBUG -DBRCM_IMPL
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifneq ($(TARGET_BUILD_TYPE),debug)
# Enable error logs for non debug build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := libnexus liblog
LOCAL_SHARED_LIBRARIES += libcmndrm_tl

include $(BUILD_SHARED_LIBRARY)
endif
endif
endif

