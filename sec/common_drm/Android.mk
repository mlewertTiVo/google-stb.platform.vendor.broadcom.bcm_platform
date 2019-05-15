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
MY_LOCAL_PATH := $(LOCAL_PATH)

#-------------
# libcmndrm.so
#-------------
ifeq ($(SAGE_SUPPORT), y)
include $(CLEAR_VARS)
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))
include $(LOCAL_PATH)/drm/common/common.inc
include $(LOCAL_PATH)/../common_crypto/common_crypto.inc
LOCAL_SRC_FILES := ${COMMON_SOURCES}

LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/sec/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/priv \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/bcrypt/include \
    $(TOP)/external/boringssl/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/drmrootfs

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += ${COMMON_DEFINES}
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifeq ($(TARGET_BUILD_VARIANT),user)
# Disable warning logs for user build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := liblog libdrmrootfs libsrai libnexus libbcrypt

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_MODULE := libcmndrm
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)
endif

ifneq ($(ANDROID_SUPPORTS_PLAYREADY), n)
#-------------
# libprdyhttp.so
#-------------
include $(CLEAR_VARS)
LOCAL_MODULE :=  libprdyhttp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/thirdparty/playready/http
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))
include $(LOCAL_PATH)/prdyhttp.inc
LOCAL_SRC_FILES := src/prdy_http.c
LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/sec/bdbg2alog \
    ${REFSW_BASE_DIR}/nexus/nxclient/include
LOCAL_C_INCLUDES += ${PRDY_HTTP_INCLUDES}
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifeq ($(TARGET_BUILD_VARIANT),user)
# Disable warning logs for user build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif
LOCAL_SHARED_LIBRARIES := liblog libnexus libcmndrm_tl
LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)

#-------------
# libcmndrmprdy.so
#-------------
include $(CLEAR_VARS)
LOCAL_MODULE :=  libcmndrmprdy
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))
include $(LOCAL_PATH)/drm/playready/playready.inc
LOCAL_SRC_FILES := ${PLAYREADY_SOURCES}

LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/sec/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/drmrootfs \
    ${REFSW_BASE_DIR}/nexus/nxclient/include

DRM_BUILD_PROFILE := 900
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DDRM_BUILD_PROFILE=${DRM_BUILD_PROFILE} -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=1
LOCAL_CFLAGS += ${PLAYREADY_DEFINES}
LOCAL_CFLAGS += -DCMD_DRM_PLAYREADY_SAGE_IMPL
LOCAL_CFLAGS += -DPLAYREADY_HOST_IMPL
LOCAL_C_INCLUDES += ${REFSW_BASE_DIR}/BSEAV/thirdparty/playready/2.5/inc
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifeq ($(TARGET_BUILD_VARIANT),user)
# Disable warning logs for user build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := liblog libnexus libnxclient libdrmrootfs libsrai libcmndrm_tl
LOCAL_STATIC_LIBRARIES := libplayreadypk_host

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
endif

ifeq ($(SAGE_SUPPORT), y)
#---------------
# libcmndrm_tl.so for Modular DRM
#---------------
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))
include $(CLEAR_VARS)

# add SAGElib related includes
include ${REFSW_BASE_DIR}/magnum/syslib/sagelib/bsagelib_public.inc

############################################################################
# Go through all <DRM>_SAGE variables from config.inc and include those
# <drm>.inc files
#vvvv#####################vvvvvvvvvvvvvvvvvvvvvvv#####################vvvv##
ifeq ($(findstring $(BCHP_CHIP), 7260 7268 7271 7445 7366 7439 7364), $(BCHP_CHIP))
include $(LOCAL_PATH)/config/config_zeus4x.inc
else ifeq ($(findstring $(BCHP_CHIP), 7435), $(BCHP_CHIP))
include $(LOCAL_PATH)/config/config_zeus30.inc
else ifeq ($(findstring $(BCHP_CHIP), 7278), $(BCHP_CHIP))
include $(LOCAL_PATH)/config/config_zeus5x.inc
else
$(error Missing zeus config inc)
endif

include $(LOCAL_PATH)/drm_tl/common_tl/common_tl.inc
COMMON_DRM_TL_SOURCES += ${COMMON_TL_SOURCES}
COMMON_DRM_TL_DEFINES += ${COMMON_TL_DEFINES}

######################widevine modular DRM################################
ifeq (${WVOEMCRYPTO_SAGE},OFF)
#Cannot build non-TL version of Common DRM library)
else
include $(LOCAL_PATH)/drm_tl/wvoemcrypto/wvoemcrypto.inc
COMMON_DRM_TL_SOURCES += ${WVOEMCRYPTO_SOURCES}
COMMON_DRM_TL_DEFINES += ${WVOEMCRYPTO_DEFINES}
endif

###################### Playback ################################
ifeq (${PLAYBACK_SAGE},OFF)
else
include $(LOCAL_PATH)/drm_tl/playback/playback.inc
COMMON_DRM_TL_SOURCES += ${PLAYBACK_SOURCES}
COMMON_DRM_TL_DEFINES += ${PLAYBACK_DEFINES}
endif

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
#################### DTCP IP ################################
ifeq (${DTCP_IP_SAGE},OFF)
else
include $(LOCAL_PATH)/drm_tl/dtcp_ip/dtcp_ip.inc
COMMON_DRM_TL_SOURCES += ${DTCP_IP_TL_SOURCES}
COMMON_DRM_TL_DEFINES += ${DTCP_IP_TL_DEFINES}
endif
endif

LOCAL_SRC_FILES := ${COMMON_DRM_TL_SOURCES}

LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/sec/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/bcrypt/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_crypto/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/priv \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include/private \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/platforms/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/drmrootfs \
    $(BSAGELIB_INCLUDES) \
    $(TOP)/external/boringssl/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(COMMON_DRM_TL_DEFINES)

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifeq ($(TARGET_BUILD_VARIANT),user)
# Disable warning logs for user build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

# Set common DRM TL to not overwrite Type 1 or 2 drm bin files on rootfs
LOCAL_CFLAGS += -DCMNDRM_SKIP_BINFILE_OVERWRITE

# android P (9.0) onward.
LOCAL_CFLAGS += -DANDROID_DATA_VENDOR_PLATFORM_SPLIT

LOCAL_SHARED_LIBRARIES := libnexus libdrmrootfs libsrai liblog libcmndrm

LOCAL_MULTILIB := 32
# LOCAL_MULTILIB := both
LOCAL_MODULE := libcmndrm_tl
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)
endif

