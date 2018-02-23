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
LOCAL_PATH := $(call my-dir)

WIFI_HAL_ROOT := ../../../../../../hardware/interfaces/wifi/1.0/default
include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.wifi@1.0-bcm
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CPPFLAGS := -Wall -Werror -Wextra
ifdef WIFI_HIDL_FEATURE_AWARE
LOCAL_CPPFLAGS += -DWIFI_HIDL_FEATURE_AWARE
endif
LOCAL_SRC_FILES := \
    hidl_struct_util.cpp \
    ${WIFI_HAL_ROOT}/hidl_sync_util.cpp \
    ${WIFI_HAL_ROOT}/service.cpp \
    ${WIFI_HAL_ROOT}/wifi.cpp \
    ${WIFI_HAL_ROOT}/wifi_ap_iface.cpp \
    wifi_chip.cpp \
    ${WIFI_HAL_ROOT}/wifi_legacy_hal.cpp \
    ${WIFI_HAL_ROOT}/wifi_legacy_hal_stubs.cpp \
    ${WIFI_HAL_ROOT}/wifi_mode_controller.cpp \
    ${WIFI_HAL_ROOT}/wifi_nan_iface.cpp \
    ${WIFI_HAL_ROOT}/wifi_p2p_iface.cpp \
    ${WIFI_HAL_ROOT}/wifi_rtt_controller.cpp \
    wifi_sta_iface.cpp \
    ${WIFI_HAL_ROOT}/wifi_status_util.cpp
LOCAL_C_INCLUDES := \
    ${LOCAL_PATH}/${WIFI_HAL_ROOT}

LOCAL_SHARED_LIBRARIES := \
    android.hardware.wifi@1.0 \
    libbase \
    libcutils \
    libhidlbase \
    libhidltransport \
    liblog \
    libnl \
    libutils \
    libwifi-hal \
    libwifi-system
LOCAL_INIT_RC := android.hardware.wifi@1.0-bcm.rc
include $(BUILD_EXECUTABLE)
