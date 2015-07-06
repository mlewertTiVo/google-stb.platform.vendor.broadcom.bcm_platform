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

ifeq ($(ANDROID_SUPPORTS_WIDEVINE), y)
ifneq ($(WIDEVINE_CLASSIC), n)
#---------------
# liboemcrypto.a
#---------------
include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := ${DRM_BUILD_MODE}/liboemcrypto.a
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
LOCAL_SRC_FILES := ../../release_prebuilts/$(LOCAL_MODULE).so
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_PREBUILT)
else
# compile library from source
LOCAL_PATH := $(LOCAL_PATH)/../../../refsw/BSEAV/lib/security/third_party/widevine/CENC_sage

# add SAGElib related includes
include $(LOCAL_PATH)/../../../../../../magnum/syslib/sagelib/bsagelib_public.inc

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

LOCAL_SRC_FILES := \
    src/oemcrypto_brcm_TL.cpp\
    src/string_conversions.cpp\
    src/properties.cpp \
    src/modp_b64w.cpp

LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/external/stlport/stlport \
    $(TOP)/external/openssl/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/third_party/widevine/CENC_sage/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include/tl \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/platforms/include \
    $(BSAGELIB_INCLUDES) \
    $(NEXUS_APP_INCLUDE_PATHS)

LOCAL_CFLAGS += -DPIC -fpic -DANDROID
LOCAL_CFLAGS += -DNDEBUG -DBRCM_IMPL

LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DBDBG_NO_MSG -DBDBG_NO_WRN -DBDBG_NO_ERR -DBDBG_NO_LOG

LOCAL_SHARED_LIBRARIES := $(NEXUS_LIB) liblog libstlport
LOCAL_SHARED_LIBRARIES += libcmndrm_tl

include $(BUILD_SHARED_LIBRARY)
endif
endif
endif

