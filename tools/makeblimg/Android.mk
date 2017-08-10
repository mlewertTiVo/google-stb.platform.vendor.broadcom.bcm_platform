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

_bootloader.img := $(PRODUCT_OUT)/bootloader.img
$(_bootloader.img): build_bootloaderimg
	echo "creating bootloader image";

_bootloader.dev.img := $(PRODUCT_OUT)/bootloader.dev.img
$(_bootloader.dev.img): build_bootloaderimg
	echo "creating bootloader (dev) image";

_bootloader.prod.img := $(PRODUCT_OUT)/bootloader.prod.img
$(_bootloader.prod.img): build_bootloaderimg
	echo "creating bootloader (prod) image";

include $(CLEAR_VARS)
LOCAL_MODULE := blimg
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(_bootloader.img)

include $(BUILD_PHONY_PACKAGE)
