#
# Copyright (C) 2017 The Android Open Source Project
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
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(SAGE_SUPPORT),y)
GOOGLE_CLEARKEY_ROOT := ../../../../../../../frameworks/av/drm/mediacas/plugins/clearkey

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(NEXUS_TOP)/nxclient/apps/utils/nxutils.inc
include $(NEXUS_TOP)/modules/security/security_defs.inc

LOCAL_SRC_FILES:= \
    ClearKeyCasPlugin.cpp \
    ClearKeySessionLibrary.cpp \
    protos/license_protos.proto \
    $(GOOGLE_CLEARKEY_ROOT)/ClearKeyFetcher.cpp \
    $(GOOGLE_CLEARKEY_ROOT)/ClearKeyLicenseFetcher.cpp \
    $(GOOGLE_CLEARKEY_ROOT)/ecm.cpp \
    $(GOOGLE_CLEARKEY_ROOT)/ecm_generator.cpp \
    $(GOOGLE_CLEARKEY_ROOT)/JsonAssetLoader.cpp

LOCAL_MODULE := libnxclearkeycasplugin

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := mediacas

LOCAL_SHARED_LIBRARIES := \
    libutils \
    liblog \
    libcrypto \
    libstagefright_foundation \
    libprotobuf-cpp-lite \
    libcutils \
    libnexus \
    libnxclient \
    libnxwrap \
    libbomx_secbuff

LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0

LOCAL_HEADER_LIBRARIES := \
    media_plugin_headers

LOCAL_STATIC_LIBRARIES := \
    libjsmn

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_PROTOC_OPTIMIZE_TYPE := full

define proto_includes
$(call local-generated-sources-dir)/proto/$(LOCAL_PATH)
endef

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefrighthw \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default \
                    ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice

LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_C_INCLUDES += \
    external/jsmn \
    frameworks/av/include \
    frameworks/native/include/media \
    frameworks/av/drm/mediacas/plugins/clearkey \
    $(call proto_includes)

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(call proto_includes)

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif

