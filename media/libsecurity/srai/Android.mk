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

#-------------
# libsrai.so
#-------------
LOCAL_PATH := ${REFSW_BASE_DIR}
include $(CLEAR_VARS)
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

# add SAGElib related includes
include $(LOCAL_PATH)/magnum/syslib/sagelib/bsagelib_public.inc
include $(LOCAL_PATH)/magnum/portinginterface/hsm/bhsm_defs.inc

LOCAL_SRC_FILES := \
    BSEAV/lib/security/sage/srai/src/sage_srai.c \
    BSEAV/lib/security/sage/srai/src/sage_srai_global_lock.c \
    magnum/syslib/sagelib/src/bsagelib_tools.c

LOCAL_C_INCLUDES := \
    $(TOP)/system/core/libcutils/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/srai/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/sage/include/private \
    $(BSAGELIB_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifneq ($(TARGET_BUILD_TYPE),debug)
# Enable error logs for non debug build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif
LOCAL_CFLAGS += -DBHSM_ZEUS_VER_MAJOR=$(BHSM_ZEUS_VER_MAJOR) -DBHSM_ZEUS_VER_MINOR=$(BHSM_ZEUS_VER_MINOR)

LOCAL_SHARED_LIBRARIES := libnexus liblog

LOCAL_MODULE := libsrai
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)

endif

