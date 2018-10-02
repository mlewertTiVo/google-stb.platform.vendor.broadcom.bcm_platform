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
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_2ND_ARCH}/libnexus.so
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
ifeq ($(BCM_DIST_KNLIMG_BINS),y)
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${BINDIST_BIN_DIR_1ST_ARCH}/libnexus.so
LOCAL_SRC_FILES_arm64 := ${BINDIST_BIN_DIR_1ST_ARCH}/libnexus.so
else
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus.so
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus.so
endif
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxserver
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
#LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_2ND_ARCH}/libnxserver.a
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
ifeq ($(BCM_DIST_KNLIMG_BINS),y)
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${BINDIST_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm64 := ${BINDIST_BIN_DIR_1ST_ARCH}/libnxserver.a
else
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
endif
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxserver_vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_2ND_ARCH}/libnxserver.a
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
ifeq ($(BCM_DIST_KNLIMG_BINS),y)
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${BINDIST_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm64 := ${BINDIST_BIN_DIR_1ST_ARCH}/libnxserver.a
else
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnxserver.a
endif
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus_static
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
#LOCAL_PROPRIETARY_MODULE := true
ifeq ($(TARGET_2ND_ARCH),arm)
LOCAL_MULTILIB := both
LOCAL_MODULE_TARGET_ARCH := arm arm64
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus_static.a
LOCAL_SRC_FILES_arm := ${NEXUS_BIN_DIR_2ND_ARCH}/libnexus_static.a
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
ifeq ($(BCM_DIST_KNLIMG_BINS),y)
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${BINDIST_BIN_DIR_1ST_ARCH}/libnexus_static.a
LOCAL_SRC_FILES_arm64 := ${BINDIST_BIN_DIR_1ST_ARCH}/libnexus_static.a
else
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus_static.a
LOCAL_SRC_FILES_arm64 := ${NEXUS_BIN_DIR_1ST_ARCH}/libnexus_static.a
endif
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
LOCAL_SRC_FILES_arm64 := ${NEXUS_NXC_BIN_DIR_1ST_ARCH}/libnxclient.so
LOCAL_SRC_FILES_arm   := ${NEXUS_NXC_BIN_DIR_2ND_ARCH}/libnxclient.so
else
ifeq ($(LOCAL_ARM_AARCH64_NOT_ABI_COMPATIBLE),y)
LOCAL_MODULE_TARGET_ARCH := arm
else
LOCAL_MODULE_TARGET_ARCH := ${P_REFSW_DRV_ARCH}
endif
ifeq ($(BCM_DIST_KNLIMG_BINS),y)
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${BINDIST_NXC_BIN_DIR_1ST_ARCH}/libnxclient.so
LOCAL_SRC_FILES_arm64 := ${BINDIST_NXC_BIN_DIR_1ST_ARCH}/libnxclient.so
else
# define arm|arm64, but use only one from LOCAL_MODULE_TARGET_ARCH
LOCAL_SRC_FILES_arm   := ${NEXUS_NXC_BIN_DIR_1ST_ARCH}/libnxclient.so
LOCAL_SRC_FILES_arm64 := ${NEXUS_NXC_BIN_DIR_1ST_ARCH}/libnxclient.so
endif
endif
include $(BUILD_PREBUILT)

# core nexus integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libb_os/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxdispfmt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxmini/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxserver/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxcec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxssd/Android.mk
