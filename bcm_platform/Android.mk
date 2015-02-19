
include vendor/broadcom/bcm_platform/brcm_audio/Android.mk
include vendor/broadcom/bcm_platform/brcm_dhd/Android.mk
include vendor/broadcom/bcm_platform/brcm_memtrack/Android.mk
include vendor/broadcom/bcm_platform/brcm_nexus/Android.mk
include vendor/broadcom/bcm_platform/cmds/cec/Android.mk
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
include vendor/broadcom/bcm_platform/nxserver/Android.mk
include vendor/broadcom/bcm_platform/pmlibservice/Android.mk


ifeq ($(ANDROID_SUPPORTS_DTVKIT),y)
include vendor/broadcom/bcm_platform/brcm_dtvkit/Android.mk
endif

BCM_APPS_PATH := vendor/broadcom/bcm_platform/app

include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmChangeDisplayFormat/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmDemoPIP/Android.mk
include ${BCM_APPS_PATH}/BcmGeneralSTBFunctions/Android.mk
include ${BCM_APPS_PATH}/BcmHdmiInPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
include ${BCM_APPS_PATH}/BcmTvSettingsLauncher/Android.mk
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk

include vendor/broadcom/bcm_platform/tools/calcfb/Android.mk
include vendor/broadcom/bcm_platform/tools/clipping/Android.mk
include vendor/broadcom/bcm_platform/tools/cmatool/Android.mk
include vendor/broadcom/bcm_platform/tools/lmkstats/Android.mk
include vendor/broadcom/bcm_platform/tools/nxcfg/Android.mk
include vendor/broadcom/bcm_platform/tools/prdy_pes_playback/Android.mk

ifneq ($(wildcard vendor/broadcom/bcm_platform/not_for_release/README.txt),)
include vendor/broadcom/bcm_platform/not_for_release/libbcmsideband/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/Bouncer/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/ExoPlayerDemo/Android.mk
include vendor/broadcom/bcm_platform/not_for_release/WidevineSamplePlayer/Android.mk
endif

