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

include vendor/broadcom/bcm_platform/brcm_audio/Android.mk
include vendor/broadcom/bcm_platform/brcm_memtrack/Android.mk
include vendor/broadcom/bcm_platform/brcm_nexus/Android.mk
include vendor/broadcom/bcm_platform/hdmi_cec/Android.mk
include vendor/broadcom/bcm_platform/libcamera2/Android.mk
include vendor/broadcom/bcm_platform/libGLES_nexus/Android.mk
include vendor/broadcom/bcm_platform/libgralloc/Android.mk
include vendor/broadcom/bcm_platform/libhwcomposer/Android.mk
include vendor/broadcom/bcm_platform/liblights/Android.mk
include vendor/broadcom/bcm_platform/libnexusipc/Android.mk
include vendor/broadcom/bcm_platform/libnexusir/Android.mk
include vendor/broadcom/bcm_platform/libnexusservice/Android.mk
include vendor/broadcom/bcm_platform/libpower/Android.mk
include vendor/broadcom/bcm_platform/libstagefrighthw/Android.mk
include vendor/broadcom/bcm_platform/libtv_input/Android.mk
include vendor/broadcom/bcm_platform/libsecurity/Android.mk
include vendor/broadcom/bcm_platform/nxlogger/Android.mk
include vendor/broadcom/bcm_platform/nxmini/Android.mk
include vendor/broadcom/bcm_platform/nxserver/Android.mk
include vendor/broadcom/bcm_platform/pmlibservice/Android.mk


ifeq ($(ANDROID_SUPPORTS_DTVKIT),y)
include vendor/broadcom/bcm_platform/brcm_dtvkit/Android.mk
endif

BCM_APPS_PATH := vendor/broadcom/bcm_platform/app

include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
include ${BCM_APPS_PATH}/BcmTvSettingsLauncher/Android.mk
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmOtaUpdater/Android.mk

include vendor/broadcom/bcm_platform/tools/calcfb/Android.mk
include vendor/broadcom/bcm_platform/tools/cec/Android.mk
include vendor/broadcom/bcm_platform/tools/clipping/Android.mk
include vendor/broadcom/bcm_platform/tools/cmatool/Android.mk
include vendor/broadcom/bcm_platform/tools/fbtest/Android.mk
include vendor/broadcom/bcm_platform/tools/lmkstats/Android.mk
include vendor/broadcom/bcm_platform/tools/makegpt/Android.mk
include vendor/broadcom/bcm_platform/tools/nxcfg/Android.mk
include vendor/broadcom/bcm_platform/tools/nxmem/Android.mk
include vendor/broadcom/bcm_platform/tools/otpgetchipid/Android.mk
include vendor/broadcom/bcm_platform/tools/prdy_pes_playback/Android.mk

ifneq ($(wildcard vendor/broadcom/bcm_platform/not_for_release/README.txt),)
include vendor/broadcom/bcm_platform/not_for_release/libbcmsideband/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/Bouncer/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/ExoPlayerDemo/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/WidevineSamplePlayer/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/omx_conformance_test/Android.mk
endif

