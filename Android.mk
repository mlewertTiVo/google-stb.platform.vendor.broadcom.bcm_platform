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

ifneq ($(filter bcm_% fbx6lc avko banff,$(TARGET_DEVICE)),)

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
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/netcoal/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/pmlibservice/Android.mk
ifeq ($(ANDROID_SUPPORTS_DTVKIT),y)
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/misc/dtvkit/Android.mk
endif

# apk's released.
BCM_APPS_PATH := ${BCM_VENDOR_STB_ROOT}/bcm_platform/apks
include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmHdmiTvInput/Android.mk
include ${BCM_APPS_PATH}/BcmKeyInterceptor/Android.mk
include ${BCM_APPS_PATH}/BcmOtaUpdater/Android.mk
include ${BCM_APPS_PATH}/BcmSidebandViewer/Android.mk
include ${BCM_APPS_PATH}/BcmSpdifSetting/Android.mk
include ${BCM_APPS_PATH}/BcmSplash/Android.mk
ifneq ($(TARGET_BUILD_PDK),true)
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
endif
include ${BCM_APPS_PATH}/BcmTvSettingsLauncher/Android.mk
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk
include ${BCM_APPS_PATH}/LeanbackBcmCustom/Android.mk

# additional tools, not needed for default integration.
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bmem/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bsysperf/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/calcfb/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/cec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/clipping/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/fbtest/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/joinstress/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/lmkstats/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/load/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makegpt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makehwcfg/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/memcpy/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxblk/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxcfg/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxheaps/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxmem/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxplay/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxtrans/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/otpgetchipid/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/prdy_pes_playback/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/setdisplay/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/yv12torgba/Android.mk

endif
