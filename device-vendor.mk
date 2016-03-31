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

# GMS
$(call inherit-product-if-exists, vendor/broadcom/prebuilts/gms/google/products/gms.mk)

# if release_prebuilts directory exists and it contains widevine prebuilt
# binaries, always use them even if widevine source is present
#
ifeq ($(TARGET_BUILD_VARIANT),user)
RELEASE_PREBUILTS := release_prebuilts/user
else
RELEASE_PREBUILTS := release_prebuilts/userdebug
endif

ifneq ($(wildcard $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libwvm.so),)

PRODUCT_COPY_FILES += \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/com.google.widevine.software.drm.xml:system/etc/permissions/com.google.widevine.software.drm.xml:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/com.google.widevine.software.drm.jar:system/framework/com.google.widevine.software.drm.jar:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libdrmdecrypt.so:system/lib/libdrmdecrypt.so:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libdrmwvmplugin.so:system/lib/drm/libdrmwvmplugin.so:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libwvdrm_L3.so:system/vendor/lib/libwvdrm_L3.so:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libwvm.so:system/vendor/lib/libwvm.so:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libWVStreamControlAPI_L3.so:system/vendor/lib/libWVStreamControlAPI_L3.so:widevine \
    $(TOP)/${BCM_VENDOR_STB_ROOT}/$(RELEASE_PREBUILTS)/libwvdrmengine.so:system/vendor/lib/mediadrm/libwvdrmengine.so:widevine

# no prebuilt binaries included, build from source if we have it
#
else ifneq ($(wildcard vendor/widevine/Android.mk),)

export BUILD_WIDEVINE_CLASSIC_FROM_SOURCE := y
export BUILD_WIDEVINE_FROM_SOURCE := y

PRODUCT_COPY_FILES += \
    vendor/widevine/proprietary/drmwvmplugin/com.google.widevine.software.drm.xml:system/etc/permissions/com.google.widevine.software.drm.xml:widevine

PRODUCT_PACKAGES += \
    libdrmwvmplugin \
    libwvm \
    libdrmdecrypt \
    com.google.widevine.software.drm

$(call first-makefiles-under, vendor/widevine)

endif
