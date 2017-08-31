LOCAL_MODULE_TAGS := optional
LOCAL_SDK_VERSION := current

LOCAL_FULL_MANIFEST_FILE := $(LOCAL_PATH)/stub/AndroidManifest.xml
ifeq ($(apk_variant),)
  LOCAL_PACKAGE_NAME := BcmPlayAutoInstallConfig
  LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res
else
  LOCAL_PACKAGE_NAME := BcmPlayAutoInstallConfig-$(apk_variant)
  LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res-$(apk_variant)
endif

# We don't have other java source files.
LOCAL_SOURCE_FILES_ALL_GENERATED := true
LOCAL_PROGUARD_ENABLED := disabled
LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb

LOCAL_AAPT_FLAGS += --version-name $(APK_VERSION) --version-code $(APK_VERSION)
include $(BUILD_PACKAGE)
