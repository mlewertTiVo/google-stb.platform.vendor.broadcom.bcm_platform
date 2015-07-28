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
REFSW_PATH :=${LOCAL_PATH}/../brcm_nexus
NEXUS_TOP ?= $(LOCAL_PATH)/../../refsw/nexus

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(CLEAR_VARS)

# Core component framework
LOCAL_SRC_FILES := \
    bomx_android_plugin.cpp \
    bomx_buffer.cpp \
    bomx_buffer_tracker.cpp \
    bomx_component.cpp \
    bomx_core.cpp \
    bomx_port.cpp \
    bomx_utils.cpp

# Component instances
LOCAL_SRC_FILES += bomx_video_decoder.cpp bomx_vp9_parser.cpp bomx_pes_formatter.cpp
LOCAL_SRC_FILES += bomx_video_encoder.cpp
LOCAL_SRC_FILES += bomx_audio_decoder.cpp

LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/frameworks/native/include/utils \
        $(TOP)/frameworks/native/include/ui  \
        $(TOP)/hardware/libhardware/include/hardware \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusipc \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libgralloc \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libhwcomposer/blib \
        $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem

include $(BSEAV)/lib/utils/batom.inc
include $(BSEAV)/lib/media/bmedia.inc
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

REMOVE_NEXUS_CFLAGS := -Wstrict-prototypes
MANGLED_NEXUS_CFLAGS := $(filter-out $(REMOVE_NEXUS_CFLAGS), $(NEXUS_CFLAGS))

LOCAL_CFLAGS := $(MANGLED_NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID $(MP_CFLAGS) $(addprefix -I,$(BMEDIA_INCLUDES) $(BFILE_MEDIA_INCLUDES))
# Required for nexusipcclient using LOGX in a header file
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include $(NEXUS_TOP)/lib/os/include/linuxuser
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

LOCAL_SHARED_LIBRARIES :=         \
        $(NEXUS_LIB)              \
        libbinder                 \
        libb_os                   \
        libcutils                 \
        libdl                     \
        libhwcbinder              \
        libnexusipcclient         \
        libnxclient               \
        libstagefright_foundation \
        libui                     \
        libutils

# Secure decoder has dependencies on Sage
ifeq ($(SAGE_SUPPORT),y)
LOCAL_SRC_FILES += bomx_video_decoder_secure.cpp
LOCAL_C_INCLUDES+=$(REFSW_BASE_DIR)/BSEAV/lib/security/sage/srai/include
LOCAL_C_INCLUDES+=$(REFSW_BASE_DIR)/magnum/syslib/sagelib/include
LOCAL_C_INCLUDES+=$(REFSW_BASE_DIR)/BSEAV/lib/security/common_crypto/include
LOCAL_SHARED_LIBRARIES+=libsrai
LOCAL_CFLAGS += -DSECURE_DECODER_ON
endif

LOCAL_MODULE := libstagefrighthw

include $(BUILD_SHARED_LIBRARY)
