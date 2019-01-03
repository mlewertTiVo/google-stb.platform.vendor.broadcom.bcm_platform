$ PACKAGE=bcm.hardware.casproxy@1.0
$ LOC=vendor/broadcom/bcm_platform/hals/casproxy/1.0/default
$ hidl-gen -Landroidbp -randroid.hidl:system/libhidl/transport -rbcm.hardware:vendor/broadcom/bcm_platform/hals $PACKAGE
$ hidl-gen -o $LOC -Lc++ -randroid.hidl:system/libhidl/transport -rbcm.hardware:vendor/broadcom/bcm_platform/hals $PACKAGE
