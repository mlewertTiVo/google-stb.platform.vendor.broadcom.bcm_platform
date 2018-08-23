$ PACKAGE=bcm.hardware.dspsvcext@1.0
$ LOC=vendor/broadcom/bcm_platform/hals/dspsvcext/1.0/default
$ hidl-gen -o $LOC -Landroidbp -randroid.hidl:system/libhidl/transport -rbcm.hardware:vendor/broadcom/bcm_platform/hals $PACKAGE
