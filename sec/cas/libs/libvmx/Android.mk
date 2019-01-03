# Copyright (C) 2018 The Android Open Source Project
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

ifeq ($(ANDROID_ENABLE_CAS_VMX),y)

LOCAL_PATH := $(call my-dir)
DIR_PREFIX := ../../../../../../..
INSTALL_DIR_PREFIX := $(DIR_PREFIX)/${VENDOR_VMX_INSTALL_ROOT}
PRODUCT_DIR_PREFIX := $(DIR_PREFIX)/${VENDOR_VMX_PRODUCT_ROOT}
BRCM_DIR_PREFIX := $(INSTALL_DIR_PREFIX)/brcm
VERIMATRIX_DIR_PREFIX := $(INSTALL_DIR_PREFIX)/verimatrix

#
# libvriptvclientDEV.so
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvriptvclientDEV
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STRIP_MODULE := true
LOCAL_SRC_FILES := $(PRODUCT_DIR_PREFIX)/libs/libvriptvclientDEV.so
include $(BUILD_PREBUILT)

#
# libvmlogger.so
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvmlogger
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := arm
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STRIP_MODULE := true
LOCAL_SRC_FILES := $(PRODUCT_DIR_PREFIX)/libs/logging/libvmlogger.so
include $(BUILD_PREBUILT)

#
# libvmutils.so
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvmutils
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STRIP_MODULE := true

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/$(VERIMATRIX_DIR_PREFIX)/include \
    $(LOCAL_PATH)/$(VERIMATRIX_DIR_PREFIX)/test_application

LOCAL_C_INCLUDES := $(subst $(ANDROID)/,,$(LOCAL_C_INCLUDES))

LOCAL_SRC_FILES += \
    $(VERIMATRIX_DIR_PREFIX)/test_application/hls_util.c \
    $(VERIMATRIX_DIR_PREFIX)/test_application/io.cpp \
    $(VERIMATRIX_DIR_PREFIX)/test_application/psi.cpp

include $(BUILD_SHARED_LIBRARY)

#
# Security Utils
#
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_MODULE := libsecurity_utils
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STRIP_MODULE := true

SEC_UTIL_DIR_PREFIX := $(BRCM_DIR_PREFIX)/security_utils

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/$(SEC_UTIL_DIR_PREFIX)/include \
    $(NXCLIENT_INCLUDES)

LOCAL_C_INCLUDES := $(subst $(ANDROID)/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -Werror

LOCAL_SHARED_LIBRARIES := \
    libnexus

LOCAL_SRC_FILES := \
    $(SEC_UTIL_DIR_PREFIX)/m2m/security_m2m_util.c \
    $(SEC_UTIL_DIR_PREFIX)/misc/safemutex.c        \
    $(SEC_UTIL_DIR_PREFIX)/misc/secutil_memory.c   \
    $(SEC_UTIL_DIR_PREFIX)/misc/secutil_keyslot.c

include $(BUILD_SHARED_LIBRARY)

#
# SAGE VMX Session Manager
#
include $(CLEAR_VARS)
LOCAL_MODULE := libvmxsessionmanager
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_PROPRIETARY_MODULE := true
LOCAL_STRIP_MODULE := true

VMX_SM_DIR_PREFIX := $(VERIMATRIX_DIR_PREFIX)/ultra

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/$(VMX_SM_DIR_PREFIX)/include \
    $(LOCAL_PATH)/$(SEC_UTIL_DIR_PREFIX)/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/magnum/syslib/sagelib/include

LOCAL_C_INCLUDES := $(subst $(ANDROID)/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS += \
    -DSECURITY_SUPPORT \
    $(NEXUS_APP_CFLAGS)

LOCAL_HEADER_LIBRARIES := \
    libutils_headers

LOCAL_SHARED_LIBRARIES := \
    libnexus              \
    libsrai               \
    libsecurity_utils     \
    libutils

LOCAL_SRC_FILES := \
    $(VMX_SM_DIR_PREFIX)/host_code/sage_vmx_session_manager_hostinterface.c

include $(BUILD_SHARED_LIBRARY)

endif
