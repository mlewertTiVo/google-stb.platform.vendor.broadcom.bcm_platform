# Copyright (C) 2014 The Android Open Source Project
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
PATH_TO_NEXUS := ../../../refsw/nexus
PATH_TO_BSEAV := ../../../refsw/BSEAV
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(NEXUS_TOP)/nxclient/apps/utils
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils
LOCAL_C_INCLUDES += $(BSEAV_TOP)/lib/glob
LOCAL_C_INCLUDES += $(BSEAV_TOP)/lib/tshdrbuilder
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DB_REFSW_ANDROID
LOCAL_CFLAGS += -DNXCLIENT_SUPPORT
# fix warnings!
LOCAL_CFLAGS += -Werror
# *** TODO: fix refsw.
LOCAL_CFLAGS += -Wno-strict-aliasing
LOCAL_CFLAGS += -Wno-unused-parameter

LOCAL_SHARED_LIBRARIES := libcutils liblog libutils libnexus libnxclient

LOCAL_SRC_FILES := \
    bcmsidebandplayerfactory.cpp \
    bcmsidebandplayer.cpp \
    bcmsidebandfileplayer.cpp \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/media_player.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/media_player_bip.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/media_player_ip.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/media_probe.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/bfile_crypto.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/dvr_crypto.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/nxapps_cmdline.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/live_source.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/tspsimgr3.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/binput.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/bfont.c \
    $(PATH_TO_NEXUS)/nxclient/apps/utils/bgui.c \
    $(PATH_TO_NEXUS)/utils/namevalue.c \
    $(PATH_TO_BSEAV)/lib/glob/glob.c

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_MODULE := libbcmsidebandplayer
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
