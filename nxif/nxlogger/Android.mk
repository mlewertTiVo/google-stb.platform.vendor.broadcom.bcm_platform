# Copyright (C) 2015 The Android Open Source Project
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
include $(CLEAR_VARS)

include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libcutils \
                          libbinder \
                          libutils \
                          libnexus \
                          libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/platforms/common/src/linuxuser.proxy
LOCAL_C_INCLUDES += $(NEXUS_TOP)/platforms/common/src/linuxkernel/common
LOCAL_C_INCLUDES += $(NEXUS_TOP)/base/include
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils
LOCAL_C_INCLUDES += $(B_REFSW_OBJ_ROOT_1ST_ARCH)/nexus/core/syncthunk
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := nxlogger
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES

ifneq ($(BCM_DIST_KNLIMG_BINS),y)
_nexus.dobuild := $(B_REFSW_OBJ_ROOT_1ST_ARCH)/_nexus.dobuild
$(_nexus.dobuild): nexus_build
	echo "forcing nexus build now..."

LOCAL_ADDITIONAL_DEPENDENCIES := $(_nexus.dobuild)
LOCAL_SRC_FILES := nxlogger.cpp
include $(BUILD_EXECUTABLE)
else
LOCAL_PATH :=
LOCAL_SRC_FILES := ${BINDIST_BIN_DIR_1ST_ARCH}/nxlogger
include $(BUILD_PREBUILT)
endif
