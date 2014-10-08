# make file for new hardware  from
#

LOCAL_PATH := $(call my-dir)

LOCAL_MODULE_TAGS := optional

include vendor/broadcom/bcm_platform/brcm_nexus/Android.mk
include vendor/broadcom/bcm_platform/nexusinit/Android.mk
include vendor/broadcom/bcm_platform/libgralloc/Android.mk
include vendor/broadcom/bcm_platform/libnexusipc/Android.mk
include vendor/broadcom/bcm_platform/libGLES_nexus/Android.mk
include vendor/broadcom/bcm_platform/brcm_memtrack/Android.mk

ifneq ($(GOOGLE_TREE_BUILD),n)
    BCM_APPS_PATH := vendor/broadcom/bcm_platform/app
else
    BCM_APPS_PATH:=$(LOCAL_PATH)/../../../../../../../../opensource/android/src/broadcom/app
endif

# Broadcom Test apps
include ${BCM_APPS_PATH}/BcmChangeDisplayFormat/Android.mk
include ${BCM_APPS_PATH}/BcmAdjustScreenOffset/Android.mk
include ${BCM_APPS_PATH}/BcmCoverFlow/Android.mk
include ${BCM_APPS_PATH}/BcmDemoPIP/Android.mk
#include ${BCM_APPS_PATH}/BcmNdkSample/Android.mk
include ${BCM_APPS_PATH}/BcmSysInfoWidget/Android.mk
include ${BCM_APPS_PATH}/BcmHdmiInPlayer/Android.mk
#include ${BCM_APPS_PATH}/BcmRemoteUI/Android.mk
include ${BCM_APPS_PATH}/BcmUriPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmGeneralSTBFunctions/Android.mk
ifeq ($(ANDROID_SUPPORTS_ANALOG_INPUT),y)
include ${BCM_APPS_PATH}/BcmAnalogInPlayer/Android.mk
endif
ifeq ($(ANDROID_ENABLE_DMS),y)
include ${BCM_APPS_PATH}/BcmDMSServer/Android.mk
endif
ifeq ($(ANDROID_NO_LOCK),y)
include ${BCM_APPS_PATH}/BcmNoLock/Android.mk
endif
ifeq ($(ANDROID_USES_TRELLIS_WM),y)
include ${BCM_APPS_PATH}/BcmAppMgr/Android.mk
include ${BCM_APPS_PATH}/BcmProcessViewer/Android.mk
endif
ifeq ($(ATP_BUILD_PASS2),y)
include ${BCM_APPS_PATH}/BcmATPArchive/Android.mk
include ${BCM_APPS_PATH}/BcmATPPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmDLNAPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmTVArchive/Android.mk
include ${BCM_APPS_PATH}/BcmTVPlayer/Android.mk
include ${BCM_APPS_PATH}/BcmEPGBrowser/Android.mk
endif
ifeq ($(ANDROID_SUPPORTS_FRONTEND_SERVICE),y)
include ${BCM_APPS_PATH}/Tsplayer/Android.mk
endif

include vendor/broadcom/bcm_platform/libhwcomposer/Android.mk

ifeq ($(ANDROID_SUPPORTS_PM),y)
include vendor/broadcom/bcm_platform/pmlibservice/Android.mk
include vendor/broadcom/bcm_platform/libpower/Android.mk
endif

include vendor/broadcom/bcm_platform/libnexusservice/Android.mk

ifeq ($(BOARD_USES_GENERIC_AUDIO),false)
include vendor/broadcom/bcm_platform/brcm_audio/Android.mk
endif
#include vendor/broadcom/bcm_platform/stagefrightplayerhw/Android.mk
#include vendor/broadcom/bcm_platform/libstagefrighthw/Android.mk
#include vendor/broadcom/bcm_platform/liboverlay/Android.mk
ifeq ($(ANDROID_ENABLE_DMS),y)
include vendor/broadcom/bcm_platform/x_dms/Android.mk
endif
ifeq ($(ANDROID_ENABLE_WEBSOCKET),y)
include vendor/broadcom/bcm_platform/remote_server/Android.mk
endif

ifeq ($(ANDROID_ENABLE_MOCA),y)
include vendor/broadcom/bcm_platform/brcm_moca/Android.mk
endif

ifeq ($(ANDROID_SUPPORTS_BCM_SUBTITLE_RENDERER),y)
include vendor/broadcom/bcm_platform/libsubt_renderer/Android.mk
endif


include vendor/broadcom/bcm_platform/ntfs3g/Android.mk

ifneq ($(USE_CAMERA_STUB),true)
include vendor/broadcom/bcm_platform/libcamera2/Android.mk
endif

ifeq ($(ANDROID_ENABLE_BTUSB),y)
include vendor/broadcom/bcm_platform/brcm_btusb/Android.mk
endif

ifeq ($(ANDROID_ENABLE_DHD),y)
include vendor/broadcom/bcm_platform/brcm_dhd/Android.mk
endif

ifneq (,$(filter y,$(ANDROID_USES_TRELLIS_WM) $(ATP_BUILD_PASS2)))
include vendor/broadcom/bcm_platform/brcm_trellis/Android.mk
endif

ifeq ($(ANDROID_SUPPORTS_FRONTEND_SERVICE),y)
include vendor/broadcom/bcm_platform/libnexusfrontendservice/Android.mk
endif

ifneq ($(filter $(ANDROID_SUPPORTS_WIDEVINE) $(ANDROID_SUPPORTS_PLAYREADY),y),)
include vendor/broadcom/bcm_platform/libsecurity/Android.mk
endif

# HDMI CEC support...
include vendor/broadcom/bcm_platform/cmds/cec/Android.mk
include vendor/broadcom/bcm_platform/hdmi_cec/Android.mk

ifeq ($(ANDROID_ENABLE_BCM_OMX_PROTOTYPE),y)
include vendor/broadcom/bcm_platform/libstagefrighthw/Android.mk
include vendor/broadcom/bcm_platform/omx_components/audio/omx_audio_dec/Android.mk
include vendor/broadcom/bcm_platform/omx_components/video/omx_video_codec/Android.mk
include vendor/broadcom/bcm_platform/omx_components/video/omx_video_encoder/Android.mk
include vendor/broadcom/bcm_platform/omx_core/Android.mk
include vendor/broadcom/bcm_platform/utils/Android.mk
endif

ifeq ($(ANDROID_SUPPORTS_TIF),y)
include ${BCM_APPS_PATH}/BcmTVInput/Android.mk
endif

#
#include $(CLEAR_VARS)
#
# include more board specific stuff here? Such as Audio parameters.
#
