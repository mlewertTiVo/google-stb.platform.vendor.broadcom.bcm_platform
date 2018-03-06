#  Copyright (C) 2018 Broadcom.  The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
#
#  This program is the proprietary software of Broadcom and/or its licensors,
#  and may only be used, duplicated, modified or distributed pursuant to the terms and
#  conditions of a separate, written license agreement executed between you and Broadcom
#  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
#  no license (express or implied), right to use, or waiver of any kind with respect to the
#  Software, and Broadcom expressly reserves all rights in and to the Software and all
#  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
#  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
#  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
#
#  Except as expressly set forth in the Authorized License,
#
#  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
#  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
#  and to use this information only in connection with your use of Broadcom integrated circuit products.
#
#  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
#  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
#  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
#  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
#  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
#  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
#  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
#  USE OR PERFORMANCE OF THE SOFTWARE.
#
#  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
#  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
#  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
#  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
#  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
#  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
#  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
#  ANY LIMITED REMEDY.

ifeq ($(SAGE_SUPPORT),y)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))
REFSW_TOP := ../../../refsw
APP_TOP := $(REFSW_TOP)/BSEAV/lib/security/sage/bp3

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
include $(MAGNUM_TOP)/portinginterface/hsm/bhsm_defs.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libsrai
LOCAL_SHARED_LIBRARIES += libcurl
LOCAL_SHARED_LIBRARIES += libssl
LOCAL_SHARED_LIBRARIES += libz

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/bp3/app \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/bp3/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/platforms/include

LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += -DWITH_POSIX -Wno-pedantic -Wno-variadic-macros -Wno-sometimes-uninitialized -Wno-unused-parameter -std=c11
LOCAL_CFLAGS += -DBHSM_ZEUS_VER_MAJOR=$(BHSM_ZEUS_VER_MAJOR) -DBHSM_ZEUS_VER_MINOR=$(BHSM_ZEUS_VER_MINOR)

SRCS = $(APP_TOP)/utils/sage_app_utils.c
SRCS += $(APP_TOP)/app/bp3_platform_host.c
SRCS += $(APP_TOP)/app/bp3_module_host.c
SRCS += $(APP_TOP)/app/main.c

SRCS += $(REFSW_TOP)/BSEAV/tools/bp3/bp3.c $(REFSW_TOP)/BSEAV/tools/bp3/bp3_features.c

LOCAL_SRC_FILES += $(SRCS)

LOCAL_MODULE := bp3
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

endif
