LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := driver/btusb.ko
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := bt_vendor.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/bluetooth
LOCAL_MODULE_TAGS := optional debug
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

ifeq ($(BROADCOM_WIFI_CHIPSET),43570a0)
include $(CLEAR_VARS)
LOCAL_MODULE := firmware/BCM43569A0_001.001.009.0039.0000_Generic_USB_40MHz_fcbga_BU_TXPwr_8dbm_4.hcd
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

