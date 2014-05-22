ifeq ($(BRCM_ANDROID_VERSION),jb)
REFSW_PATH := vendor/broadcom/bcm${BCHP_CHIP}/brcm_nexus
else
REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus
endif

LOCAL_PATH:= $(call my-dir)

include $(REFSW_PATH)/bin/include/platform_app.inc

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
#LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusfrontendservice
LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipcclient

LOCAL_LDFLAGS := -lnexus -L$(REFSW_PATH)/bin
LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include $(JNI_H_INCLUDE) 
#LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusfrontendservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

LOCAL_SRC_FILES := jni_changedisplayformat.cpp

LOCAL_CFLAGS:= $(NEXUS_CFLAGS) -DANDROID

# Test for version newer than ICS
JB_OR_NEWER := $(shell test "${BRCM_ANDROID_VERSION}" \> "ics" && echo "y")

ifeq ($(JB_OR_NEWER),y)
LOCAL_CFLAGS += -DANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
endif

LOCAL_MODULE := libjni_changedisplayformat
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
