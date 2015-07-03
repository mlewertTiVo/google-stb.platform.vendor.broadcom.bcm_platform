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

LOCAL_STEM := bcm_platform/device-partial.mk

# always try to build from sources if we have them available.
#
ifneq ($(wildcard vendor/widevine/Android.mk),)

export BUILD_WIDEVINE_CLASSIC_FROM_SOURCE := y
export BUILD_WIDEVINE_FROM_SOURCE := y

PRODUCT_COPY_FILES += \
    vendor/widevine/proprietary/drmwvmplugin/com.google.widevine.software.drm.xml:system/etc/permissions/com.google.widevine.software.drm.xml:widevine \
    vendor/widevine/proprietary/drmwvmplugin/com.google.widevine.software.drm.jar:system/framework/com.google.widevine.software.drm.jar:widevine

$(call first-makefiles-under, vendor/widevine)

#
# nope - use proprietary prebuilts..
#
else

$(call inherit-product-if-exists, vendor/widevine/$(LOCAL_STEM))

endif
