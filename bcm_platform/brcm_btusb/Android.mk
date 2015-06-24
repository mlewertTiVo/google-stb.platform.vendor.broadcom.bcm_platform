LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := driver/btusb.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/${BCM_VENDOR_STB_ROOT}/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := bt_vendor.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/bluetooth
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

ifeq ($(BROADCOM_WIFI_CHIPSET),43242a1)
include $(CLEAR_VARS)
LOCAL_MODULE := firmware/BCM43242A1_001.002.007.0074.0000_Generic_USB_Class2_Generic_3DTV_TBFC_37_4MHz_Wake_on_BLE_Hisense.hcd
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/${BCM_VENDOR_STB_ROOT}/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

ifeq ($(BROADCOM_WIFI_CHIPSET),43570a0)
include $(CLEAR_VARS)
LOCAL_MODULE := firmware/BCM43569A0_001.001.009.0039.0000_Generic_USB_40MHz_fcbga_BU_TXPwr_8dbm_4.hcd
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/${BCM_VENDOR_STB_ROOT}/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

ifeq ($(BROADCOM_WIFI_CHIPSET),43570a2)
include $(CLEAR_VARS)
LOCAL_MODULE := firmware/BCM43569A2_001.003.004.0044.0000_Generic_USB_40MHz_fcbga_BU_Tx6dbm_desen_Freebox.hcd
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/${BCM_VENDOR_STB_ROOT}/btusb
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif

