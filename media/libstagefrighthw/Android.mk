#############################################################################
#    (c)2010-2011 Broadcom Corporation
#
# This program is the proprietary software of Broadcom Corporation and/or its licensors,
# and may only be used, duplicated, modified or distributed pursuant to the terms and
# conditions of a separate, written license agreement executed between you and Broadcom
# (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
# no license (express or implied), right to use, or waiver of any kind with respect to the
# Software, and Broadcom expressly reserves all rights in and to the Software and all
# intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
# HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
# NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
#
# Except as expressly set forth in the Authorized License,
#
# 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
# secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
# and to use this information only in connection with your use of Broadcom integrated circuit products.
#
# 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
# AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
# WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
# THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
# OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
# LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
# OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
# USE OR PERFORMANCE OF THE SOFTWARE.
#
# 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
# LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
# EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
# USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
# ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
# LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
# ANY LIMITED REMEDY.
#
#############################################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        bomx_pes_formatter.cpp \
        bomx_utils.cpp

LOCAL_C_INCLUDES := $(TOP)/frameworks/native/include/media/openmax
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/utils
 
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES :=         \
        libnexus                  \
        liblog

LOCAL_MODULE := libbomx_util
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

# Core component framework
LOCAL_SRC_FILES := \
    bomx_android_plugin.cpp \
    bomx_buffer.cpp \
    bomx_buffer_tracker.cpp \
    bomx_component.cpp \
    bomx_core.cpp \
    bomx_port.cpp

# Component instances
LOCAL_SRC_FILES += bomx_video_decoder.cpp bomx_pes_formatter.cpp
LOCAL_SRC_FILES += bomx_video_encoder.cpp
LOCAL_SRC_FILES += bomx_audio_decoder.cpp bomx_aac_parser.cpp

LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/frameworks/native/include/utils \
        $(TOP)/frameworks/native/include/ui  \
        $(TOP)/hardware/libhardware/include/hardware \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusservice \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/blib \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include $(NEXUS_TOP)/lib/os/include/linuxuser
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES :=         \
        libbinder                 \
        libbomx_util              \
        libb_os                   \
        libcutils                 \
        libdl                     \
        libhwcbinder              \
        libnexus                  \
        libnexusipcclient         \
        libnxclient               \
        libstagefright_foundation \
        libui                     \
        libutils

# Secure decoder has dependencies on Sage
ifeq ($(SAGE_SUPPORT),y)
LOCAL_SRC_FILES += bomx_video_decoder_secure.cpp
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/magnum/syslib/sagelib/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/common_crypto/include
LOCAL_SHARED_LIBRARIES += libsrai
LOCAL_CFLAGS += -DSECURE_DECODER_ON
endif

LOCAL_MODULE := libstagefrighthw

include $(BUILD_SHARED_LIBRARY)
