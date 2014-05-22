LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

JB_OR_OLDER := $(shell test "${BRCM_ANDROID_VERSION}" \< "kk" && echo "y")

LOCAL_MODULE :=driver/bcmdhd.ko
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := firmware/rtecdc.bin.trx
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := firmware/43242-rtecdc.bin.trx
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
include $(CLEAR_VARS)
LOCAL_MODULE := firmware/43569-rtecdc.bin.trx
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
include $(CLEAR_VARS)
LOCAL_MODULE := nvrams/fake43236usb_p532.nvm
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := nvrams/bcm943242usbref_p461_comp.txt
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
include $(CLEAR_VARS)
LOCAL_MODULE := nvrams/bcm943569usb_p26x_desense_3_comp.txt
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
ifeq ($(JB_OR_OLDER),y)
include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.wifi.xml
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions
LOCAL_SRC_FILES := $(LOCAL_MODULE)


include $(BUILD_PREBUILT)
endif

########################
include $(CLEAR_VARS)
include vendor/broadcom/bcm_platform/brcm_dhd/bcmdl/Android.mk





