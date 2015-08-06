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

#-------------
# libsrai.so
#-------------
LOCAL_PATH := $(call my-dir)/../../../refsw/

include $(CLEAR_VARS)

# add SAGElib related includes
include $(LOCAL_PATH)/magnum/syslib/sagelib/bsagelib_public.inc

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

LOCAL_SRC_FILES := \
    BSEAV/lib/security/sage/srai/src/sage_srai.c \
    BSEAV/lib/security/sage/srai/src/sage_srai_global_lock.c \
    magnum/syslib/sagelib/src/bsagelib_tools.c

LOCAL_C_INCLUDES := \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/libsecurity/bdbg2alog \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/include \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/security/sage/include/private \
    $(BSAGELIB_INCLUDES) \
    $(NEXUS_APP_INCLUDE_PATHS)

LOCAL_CFLAGS += -DPIC -fpic -DANDROID
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifneq ($(TARGET_BUILD_TYPE),debug)
# Enable error logs for non debug build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := $(NEXUS_LIB) liblog

LOCAL_MODULE := libsrai

include $(BUILD_SHARED_LIBRARY)

