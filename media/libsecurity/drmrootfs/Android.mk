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

#----------------
# libdrmrootfs.so
#----------------
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/drmrootfs
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include $(CLEAR_VARS)

LOCAL_MODULE := libdrmrootfs
LOCAL_SRC_FILES := drm_data.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -fshort-wchar
ifeq ($(SAGE_VERSION),2x)
LOCAL_CFLAGS += -DUSE_UNIFIED_COMMON_DRM
endif

LOCAL_LDFLAGS := -Wl,--no-fatal-warnings

include $(BUILD_SHARED_LIBRARY)

