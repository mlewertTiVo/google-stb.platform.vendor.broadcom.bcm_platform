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
LOCAL_ADDITIONAL_DEPENDENCIES := $(BCM_GPT_CONFIG_FILE)

LOCAL_POST_INSTALL_CMD :=  \
  if [ ! -z $(BCM_GPT_CONFIG_FILE) ]; then \
    $(HOST_OUT_EXECUTABLES)/$(LOCAL_MODULE) `paste -sd " " $(BCM_GPT_CONFIG_FILE)` -o $(PRODUCT_OUT)/gpt.bin; \
  fi

include $(BUILD_HOST_EXECUTABLE)

