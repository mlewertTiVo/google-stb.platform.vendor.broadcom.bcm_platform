LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ifeq ($(BROADCOM_WIFI_CHIPSET), 4360b)
LOCAL_MODULE :=driver/wl.ko
else
LOCAL_MODULE :=driver/bcmdhd.ko
endif
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)


ifneq (,$(filter 43242a1 43236b 43569a0 ,$(BROADCOM_WIFI_CHIPSET)))

include $(CLEAR_VARS)
LOCAL_MODULE := firmware/fw.bin.trx
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := nvrams/nvm.txt
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/broadcom/dhd
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

endif


ifneq ($(BRCM_ANDROID_VERSION),kk)
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





