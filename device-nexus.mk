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

LOCAL_PATH := ${NEXUS_BIN_DIR}

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus
LOCAL_SRC_FILES := libnexus.so
LOCAL_32_BIT_ONLY := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxserver
LOCAL_SRC_FILES := libnxserver.a
LOCAL_32_BIT_ONLY := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnexus_static
LOCAL_SRC_FILES := libnexus_static.a
LOCAL_32_BIT_ONLY := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .a
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_STRIP_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libnxclient
LOCAL_SRC_FILES := libnxclient.so
LOCAL_32_BIT_ONLY := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib
include $(BUILD_PREBUILT)

# core nexus integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libb_os/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusipc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusservice/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxdispfmt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxlogger/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxmini/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/nxserver/Android.mk
