# Copyright (C) 2016 Broadcom Limited
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

LOCAL_PATH :=

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus.so
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_2ND_ARCH}/libnexus.so
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxserver
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_2ND_ARCH}/libnxserver.a
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus_static
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus_static.a
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_2ND_ARCH}/libnexus_static.a
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus_static.a
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxclient
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxclient.so
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_2ND_ARCH}/libnxclient.so
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxclient.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxclient_static
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxclient_static.a
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_2ND_ARCH}/libnxclient_static.a
else
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxclient_static.a
endif
include $(BUILD_PREBUILT)

# core nexus integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libb_os/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxdispfmt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxlogger/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxmini/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxserver/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec/Android.mk
