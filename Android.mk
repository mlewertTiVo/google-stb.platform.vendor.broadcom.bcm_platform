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

ifneq ($(filter $(BCM_RBOARDS) $(BCM_DBOARDS) $(BCM_CBOARDS),$(TARGET_DEVICE)),)

# nexus interface and core integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/device-nexus.mk
# gles stack and friends.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/device-v3d.mk
# android hals implementation.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/device-hals.mk
# multimedia integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/device-media.mk

# unclassified.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/hfrvideo/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/libsmt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/netcoal/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/togplm/Android.mk

BCM_APPS_PATH := ${BCM_VENDOR_STB_ROOT}/bcm_platform/apks
# apk's released.
include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmCustomizer/Android.mk
include ${BCM_APPS_PATH}/BcmHdmiTvInput/Android.mk
#include ${BCM_APPS_PATH}/BcmSidebandViewer/Android.mk
include ${BCM_APPS_PATH}/BcmTvSettingsLauncher/Android.mk
include ${BCM_APPS_PATH}/PAICfgStub/Android.mk

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makeblimg/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makegpt/Android.mk
ifneq ($(BCM_DIST_FORCED_BINDIST), y)
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makehwcfg/Android.mk
endif

# additional tools, not needed for default integration.
ifneq ($(TARGET_BUILD_PDK),true)
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bp3/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/otpgetchipid/Android.mk
endif

# private tools, not released.
-include ${BCM_VENDOR_STB_ROOT}/bcm_platform/priv/Android.mk

# tools requiring third party packages, may or not work in a
# given environment.
-include ${BCM_VENDOR_STB_ROOT}/bcm_platform/3pip/Android.mk

endif
