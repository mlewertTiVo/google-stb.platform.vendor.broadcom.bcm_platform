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

ifeq ($(SAGE_SUPPORT),y)

LOCAL_PATH := ${REFSW_BASE_DIR}
include $(CLEAR_VARS)
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

# add SAGElib related includes
include $(LOCAL_PATH)/magnum/syslib/sagelib/bsagelib_public.inc

LOCAL_SRC_FILES := \
    BSEAV/lib/security/sage/manufacturing/src/sage_manufacturing_api.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/manufacturing/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/manufacturing/src \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/manufacturing \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/common_drm/include/tl \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include/private \
    $(BSAGELIB_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifeq ($(TARGET_BUILD_VARIANT),user)
# Disable warning logs for user build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := libnexus liblog libsrai
LOCAL_MODULE := libsmt
include $(BUILD_SHARED_LIBRARY)

endif

