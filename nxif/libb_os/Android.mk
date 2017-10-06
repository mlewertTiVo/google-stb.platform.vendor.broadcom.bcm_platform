#############################################################################
#    (c)2015 Broadcom Corporation
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
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include $(NEXUS_TOP)/lib/os/include/linuxuser
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/os/b_os_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
LOCAL_SRC_FILES += $(subst $(B_LIB_TOP),nexus/lib,$(B_OS_LIB_SOURCES))
LOCAL_SRC_FILES := $(subst nexus/lib/../../,,$(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(subst nexus/../,,$(LOCAL_SRC_FILES))
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_OS_LIB_PUBLIC_INCLUDES) $(B_OS_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += $(addprefix -D,$(B_OS_LIB_DEFINES))
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        liblog                    \
        libnexus

LOCAL_MODULE := libb_os
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
