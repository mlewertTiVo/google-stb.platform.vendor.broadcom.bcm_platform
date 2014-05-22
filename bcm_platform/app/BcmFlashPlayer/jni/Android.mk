ifeq ($(BRCM_ANDROID_VERSION),jb)
REFSW_PATH := vendor/broadcom/bcm${BCHP_CHIP}/brcm_nexus
else
REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus
endif
LOCAL_PATH := $(call my-dir)
APPLIB_TOP := $(LOCAL_PATH)/../../../../../../..

include $(REFSW_PATH)/bin/include/platform_app.inc
include $(CLEAR_VARS)

LOCAL_MODULE            := libjni_bcmfp
LOCAL_SRC_FILES         := BcmFP_jni.cpp
LOCAL_PRELINK_MODULE    := false
LOCAL_SHARED_LIBRARIES  := liblog libstlport libtrellis_bam_client libtrellis_common
LOCAL_MODULE_TAGS       := eng
LOCAL_C_INCLUDES        += $(APPLIB_TOP)/broadcom/trellis/include \
                           $(APPLIB_TOP)/broadcom/trellis/common \
                           $(APPLIB_TOP)/broadcom/trellis/osapi \
                           external/stlport/stlport \
                           bionic

include $(BUILD_SHARED_LIBRARY)
