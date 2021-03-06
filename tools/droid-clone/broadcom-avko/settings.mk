#
# Copyright 2015 The Android Open-Source Project
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
#

PRODUCT_NAME := avko
PRODUCT_DEVICE := avko
PRODUCT_MODEL := avko
PRODUCT_CHARACTERISTICS := tv
PRODUCT_MANUFACTURER := google
PRODUCT_BRAND := google

# clone install the correct extensions for the hardware based identified modules.
PRODUCT_COPY_FILES += \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/cfgs/fstab.broadcomstb:root/fstab.avko \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/cfgs/init.broadcomstb.rc:root/init.avko.rc \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/cfgs/ueventd.bcm_platform.rc:root/ueventd.avko.rc
