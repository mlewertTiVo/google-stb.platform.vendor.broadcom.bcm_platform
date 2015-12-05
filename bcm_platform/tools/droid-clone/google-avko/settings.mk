# copyright 2015 - broadcom canada ltd.
#

export ANDROID_PRODUCT_OUT := avko

PRODUCT_NAME := avko
PRODUCT_DEVICE := avko
PRODUCT_MODEL := avko
PRODUCT_CHARACTERISTICS := tv
PRODUCT_MANUFACTURER := Google
PRODUCT_BRAND := Google

# clone install the correct extensions for the hardware based identified modules.
PRODUCT_COPY_FILES += \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/prebuilt/fstab.broadcomstb:root/fstab.avko \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/prebuilt/init.broadcomstb.rc:root/init.avko.rc \
    ${BCM_VENDOR_STB_ROOT}/bcm_platform/prebuilt/ueventd.bcm_platform.rc:root/ueventd.avko.rc
