ifneq ($(filter bcm_% fbx6lc avko banff,$(TARGET_DEVICE)),)

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/audio/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/camera2/Android.mk
#include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/consumerir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hdmi_cec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/lights/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/memtrack/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/power/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/tv_input/Android.mk

endif

