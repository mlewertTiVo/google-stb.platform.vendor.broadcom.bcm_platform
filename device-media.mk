ifneq ($(filter $(BCM_RBOARDS) $(BCM_DBOARDS) $(BCM_CBOARDS),$(TARGET_DEVICE)),)

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsideband/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libbcmsidebandplayer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefrighthw/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/Android.mk

endif

