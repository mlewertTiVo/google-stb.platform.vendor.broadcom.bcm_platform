ifneq ($(filter $(BCM_RBOARDS) $(BCM_DBOARDS) $(BCM_CBOARDS),$(TARGET_DEVICE)),)

include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/audio/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/boot_control/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/consumerir/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/dspsvcext/1.0/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gatekeeper/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gpu/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hdmi_cec/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/hwcomposer/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/lights/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/memtrack/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/power/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/thermal/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/tv_input/Android.mk
include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/vulkan/Android.mk
#include ${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/wifi/1.0/Android.mk

endif

