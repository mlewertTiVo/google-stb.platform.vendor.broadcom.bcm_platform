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

include $(CLEAR_VARS)

LOCAL_MODULE := libdrmrootfs
LOCAL_SRC_FILES := drm_data.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -fshort-wchar

LOCAL_LDFLAGS := -Wl,--no-fatal-warnings

include $(BUILD_SHARED_LIBRARY)

