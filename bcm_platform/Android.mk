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

ifneq ($(filter bcm_% fbx6lc avko arrow,$(TARGET_DEVICE)),)

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/brcm_audio/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/brcm_memtrack/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/brcm_nexus/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hdmi_cec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libb_os/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libb_playback_ip/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libcamera2/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libGLES_nexus/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libgralloc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libhwcomposer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/liblights/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusipc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libnexusservice/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libpower/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libstagefrighthw/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libtv_input/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/libsecurity/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxdispfmt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxlogger/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxmini/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/nxserver/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/pmlibservice/Android.mk


ifeq ($(ANDROID_SUPPORTS_DTVKIT),y)
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/brcm_dtvkit/Android.mk
endif

BCM_APPS_PATH := ${BCM_VENDOR_STB_ROOT}/bcm_platform/app

include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
include ${BCM_APPS_PATH}/BcmTvSettingsLauncher/Android.mk
ifneq ($(TARGET_BUILD_PDK),true)
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk
endif
include ${BCM_APPS_PATH}/BcmOtaUpdater/Android.mk
include ${BCM_APPS_PATH}/LeanbackBcmCustom/Android.mk

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bmem/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/bsysperf/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/calcfb/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/cec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/clipping/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/cmatool/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/fbtest/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/lmkstats/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makegpt/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/makehwcfg/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxblk/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxcfg/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxheaps/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxmem/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/nxtrans/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/otpgetchipid/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/prdy_pes_playback/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/setdisplay/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/yv12torgba/Android.mk

ifneq ($(wildcard ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/README.txt),)
ifneq ($(TARGET_BUILD_PDK),true)
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/libbcmsideband/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/Bouncer/Android.mk
endif
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/ExoPlayerDemo/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/WidevineSamplePlayer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/not_for_release/omx_conformance_test/Android.mk
endif

endif
