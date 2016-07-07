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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := efi_crc32.c efi.c make-gpt-tables.c
LOCAL_CFLAGS := -Werror

LOCAL_MODULE := makegpt
LOCAL_MODULE_TAGS := optional

include $(BUILD_HOST_EXECUTABLE)

_gpt.bin := $(PRODUCT_OUT)/gpt.bin
$(_gpt.bin): $(HOST_OUT_EXECUTABLES)/makegpt $(BCM_GPT_CONFIG_FILE)
	$< -o $@ `paste -sd " " $(BCM_GPT_CONFIG_FILE)`;

include $(CLEAR_VARS)
LOCAL_MODULE := gptbin
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := makegpt

include $(BUILD_PHONY_PACKAGE)
