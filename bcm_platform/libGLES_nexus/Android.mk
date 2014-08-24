LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libGLES_nexus
LOCAL_SRC_FILES := bin/libGLES_nexus.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_STRIP_MODULE := true
ifeq ($(BRCM_ANDROID_VERSION),kk)
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl
else
   ifeq ($(TARGET_2ND_ARCH),arm)
      LOCAL_MODULE_RELATIVE_PATH := egl
   else
      LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/egl
   endif
endif

include $(BUILD_PREBUILT)

