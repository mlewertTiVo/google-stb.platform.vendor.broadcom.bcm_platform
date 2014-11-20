
ifeq ($(PRODUCT_MANUFACTURER),BROADCOM)

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
include vendor/broadcom/bcm_platform/libtv_input/Android.mk
include vendor/broadcom/bcm_platform/nexusinit/Android.mk


BCM_APPS_PATH := vendor/broadcom/bcm_platform/app

include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmChangeDisplayFormat/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmDemoPIP/Android.mk
include ${BCM_APPS_PATH}/BcmGeneralSTBFunctions/Android.mk
include ${BCM_APPS_PATH}/BcmHdmiInPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk


// why do we need a ifeq?  todo: remove.
ifeq ($(ANDROID_SUPPORTS_PM),y)
include vendor/broadcom/bcm_platform/pmlibservice/Android.mk
include vendor/broadcom/bcm_platform/libpower/Android.mk
endif

// why do we need a ifeq?  todo: remove.
ifneq ($(filter $(ANDROID_SUPPORTS_WIDEVINE) $(ANDROID_SUPPORTS_PLAYREADY),y),)
include vendor/broadcom/bcm_platform/libsecurity/Android.mk
endif

// this is needed because we cannot build a pure AOSP image with current OMX integration.  todo: remove.
ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
include vendor/broadcom/bcm_platform/libstagefrighthw/Android.mk
include vendor/broadcom/bcm_platform/omx_components/audio/omx_audio_dec/Android.mk
include vendor/broadcom/bcm_platform/omx_components/video/omx_video_codec/Android.mk
include vendor/broadcom/bcm_platform/omx_components/video/omx_video_encoder/Android.mk
include vendor/broadcom/bcm_platform/omx_core/Android.mk
include vendor/broadcom/bcm_platform/utils/Android.mk
endif


include vendor/broadcom/bcm_platform/tools/cmatest/Android.mk
include vendor/broadcom/bcm_platform/tools/lmkstats/Android.mk

endif

