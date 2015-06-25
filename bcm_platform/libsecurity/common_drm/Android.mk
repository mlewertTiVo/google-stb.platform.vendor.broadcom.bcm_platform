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
# save original 
LOCAL_PATH_ORIG := $(LOCAL_PATH)
#-------------
# libcmndrm.so
#-------------
include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := ${DRM_BUILD_MODE}/libcmndrm.so
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

ifeq ($(ANDROID_SUPPORTS_PLAYREADY), y)
#-------------
# libcmndrmprdy.so
#-------------
include $(CLEAR_VARS)
LOCAL_LIB_NAME := libcmndrmprdy
LOCAL_MODULE_TAGS := optional

# Check if a prebuilt library has been created in the release_prebuilts folder
ifneq (,$(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/release_prebuilts/$(LOCAL_LIB_NAME).so))
# use prebuilt library if one exists
LOCAL_PREBUILT_LIBS := ../release_prebuilts/$(LOCAL_LIB_NAME).so
include $(BUILD_MULTI_PREBUILT)
else
# compile library from source
LOCAL_PATH := $(LOCAL_PATH_ORIG)/../../../refsw/BSEAV/lib/security/common_drm
include $(LOCAL_PATH)/drm/playready/playready.inc
LOCAL_SRC_FILES := ${PLAYREADY_SOURCES}

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/drmrootfs \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/playready/2.5/inc \
    $(NEXUS_APP_INCLUDE_PATHS) \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include

DRM_BUILD_PROFILE := 900
LOCAL_CFLAGS += -DDRM_BUILD_PROFILE=${DRM_BUILD_PROFILE} -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=1
LOCAL_CFLAGS += -DPIC -fpic -DANDROID
LOCAL_CFLAGS += $(NEXUS_CFLAGS) ${PLAYREADY_DEFINES}
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DBDBG_NO_MSG -DBDBG_NO_WRN -DBDBG_NO_ERR -DBDBG_NO_LOG

LOCAL_SHARED_LIBRARIES := libnexus libnxclient libplayreadypk_host libdrmrootfs

LOCAL_MODULE :=  $(LOCAL_LIB_NAME)
include $(BUILD_SHARED_LIBRARY)
endif
endif

ifeq ($(SAGE_SUPPORT), y)
#---------------
# libcmndrm_tl.so for Modular DRM
#---------------
LOCAL_PATH := $(LOCAL_PATH_ORIG)/../../../refsw/BSEAV/lib/security/common_drm

include $(CLEAR_VARS)

# add SAGElib related includes
include $(LOCAL_PATH)/../../../../magnum/syslib/sagelib/bsagelib_public.inc

############################################################################
# Go through all <DRM>_SAGE variables from config.inc and include those
# <drm>.inc files
#vvvv#####################vvvvvvvvvvvvvvvvvvvvvvv#####################vvvv##
ifeq ($(findstring $(BCHP_CHIP), 7445 7366 7439 7364), $(BCHP_CHIP))
include $(LOCAL_PATH)/config/config_zeus4x.inc
else ifeq ($(findstring $(BCHP_CHIP), 7435), $(BCHP_CHIP))
include $(LOCAL_PATH)/config/config_zeus30.inc
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

#################### NETFLIX ################################
ifeq (${NETFLIX_SAGE},OFF)
#Cannot build non-TL version of Common DRM library)
else
include $(LOCAL_PATH)/drm_tl/netflix/netflix.inc
COMMON_DRM_TL_SOURCES += ${NETFLIX_SOURCES}
COMMON_DRM_TL_DEFINES += ${NETFLIX_DEFINES}
endif

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

LOCAL_SRC_FILES := ${COMMON_DRM_TL_SOURCES}

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/bcrypt/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include/priv \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_drm/include/tl \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/include/private \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/platforms/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/drmrootfs \
    $(BSAGELIB_INCLUDES) \
    $(TOP)/external/openssl/include \
    $(NEXUS_APP_INCLUDE_PATHS)

LOCAL_CFLAGS += -DPIC -fpic -DANDROID
LOCAL_CFLAGS += $(NEXUS_CFLAGS) $(COMMON_DRM_TL_DEFINES)
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DBDBG_NO_MSG -DBDBG_NO_WRN -DBDBG_NO_ERR -DBDBG_NO_LOG
# Set common DRM TL to not overwrite Type 1 or 2 drm bin files on rootfs
LOCAL_CFLAGS += -DCMNDRM_SKIP_BINFILE_OVERWRITE

LOCAL_SHARED_LIBRARIES := $(NEXUS_LIB) libcmndrm libdrmrootfs libsrai

LOCAL_MODULE := libcmndrm_tl
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif

