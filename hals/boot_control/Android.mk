# Copyright (C) 2016 The Android Open Source Project
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

ifeq ($(HW_AB_UPDATE_SUPPORT),y)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := bootctrl.$(TARGET_BOARD_PLATFORM)
LOCAL_PRELINK_MODULE := false
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libbase
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libhwcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnxwrap
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_SHARED_LIBRARIES += bcm.hardware.nexus@1.0
else
LOCAL_SHARED_LIBRARIES += libnxbinder
LOCAL_SHARED_LIBRARIES += libnxevtsrc
endif
LOCAL_STATIC_LIBRARIES := libfs_mgr
LOCAL_C_INCLUDES := system/core/fs_mgr/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxwrap \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnexusir \
                    $(NXCLIENT_INCLUDES)
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/nexus/1.0/default \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice
else
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxbinder \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/nxif/libnxevtsrc
endif
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
# fix warnings!
LOCAL_CFLAGS += -Werror
ifeq ($(LOCAL_DEVICE_FULL_TREBLE),y)
LOCAL_CFLAGS += -DBCM_FULL_TREBLE
endif
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-bootc\"
LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_SRC_FILES := boot_control.cpp
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

# for use during recovery, therefore not full nxclient support.
include $(CLEAR_VARS)
LOCAL_MODULE := bootctrl.$(TARGET_BOARD_PLATFORM)
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog \
                          libbase
LOCAL_STATIC_LIBRARIES := libfs_mgr
LOCAL_C_INCLUDES := system/core/fs_mgr/include
# fix warnings!
LOCAL_CFLAGS += -Werror
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-bootc\"
LOCAL_CFLAGS += -DNO_NXCLIENT_CHECK
LOCAL_SRC_FILES := boot_control.cpp
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

endif

