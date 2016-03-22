ifneq ($(filter bcm_% fbx6lc avko arrow,$(TARGET_DEVICE)),)

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libb_playback_ip/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsideband/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsidebandplayer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefright_bcm/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefrighthw/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/Android.mk

endif

