# Copyright 2015 The Android Open Source Project
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


# if release_prebuilts directory exists and it contains widevine prebuilt
# binaries, always use them even if widevine source is present
#
ifeq ($(TARGET_BUILD_VARIANT),user)
RELEASE_PREBUILTS := release_prebuilts/user
else
RELEASE_PREBUILTS := release_prebuilts/userdebug
endif

ifneq ($(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libwvm.so),)

# TBD.
#
# PRODUCT_COPY_FILES +=

# no prebuilt binaries included, build from source if we have it
#
else ifneq ($(wildcard vendor/widevine/Android.mk),)

export BUILD_WIDEVINE_FROM_SOURCE := y

PRODUCT_PACKAGES += libwvdrmengine
PRODUCT_PACKAGES += libwvhidl

$(call first-makefiles-under, vendor/widevine)

endif
