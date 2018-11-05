LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

BCM_PAI_BUILD_FROM_SOURCE := n

ifeq ($(BCM_PAI_BUILD_FROM_SOURCE),n)

include $(LOCAL_PATH)/build_apk.mk

else

############################
# Stub APK
include $(CLEAR_VARS)
apk_variant :=
APK_VERSION := 1
include $(LOCAL_PATH)/build_apk.mk

############################
# Test
include $(CLEAR_VARS)
apk_variant := test
APK_VERSION := 1001
include $(LOCAL_PATH)/build_apk.mk

endif

