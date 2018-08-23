$ PACKAGE=bcm.hardware.sfhak@1.0
$ LOC=vendor/broadcom/bcm_platform/hals/sfhak/1.0/default
$ hidl-gen -Landroidbp -randroid.hidl:system/libhidl/transport -rbcm.hardware:vendor/broadcom/bcm_platform/hals $PACKAGE
$ hidl-gen -o $LOC -Lc++-impl -randroid.hidl:system/libhidl/transport -rbcm.hardware:vendor/broadcom/bcm_platform/hals $PACKAGE
