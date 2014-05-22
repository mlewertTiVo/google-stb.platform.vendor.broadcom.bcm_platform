LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

KK_OR_NEWER := $(shell test "${BRCM_ANDROID_VERSION}" \> "jb" && echo "y")

LOCAL_SRC_FILES := $(call all-subdir-java-files)

ifeq ($(KK_OR_NEWER),y)
	LOCAL_PROGUARD_ENABLED := disabled
endif

LOCAL_PACKAGE_NAME := BcmDLNAPlayer

include $(BUILD_PACKAGE)
