
ifeq ($(BRCM_ANDROID_VERSION),jb)
REFSW_PATH := vendor/broadcom/bcm${BCHP_CHIP}/brcm_nexus
else
REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus
endif

LOCAL_PATH := $(call my-dir)
APPLIB_TOP := $(LOCAL_PATH)/../../../../../../..
UPNP_TOP := $(APPLIB_TOP)/broadcom/upnp
DLNA_TOP := $(APPLIB_TOP)/broadcom/dlna

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(CLEAR_VARS)

# Also need the JNI headers.
LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE)\
	$(UPNP_TOP)/include \
	$(DLNA_TOP)/include \
	$(DLNA_TOP)/include/dms \
	$(DLNA_TOP)/dms/aal/nexus/src

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE    := libbcm_dms_server_jni

LOCAL_SHARED_LIBRARIES := libbdlna libbdlna-dms libbdlna-dms-aal liblog $(NEXUS_LIB)

LOCAL_CFLAGS := -DLINUX -DANDROID -D_GNU_SOURCE -DHAVE_DLNA -DDMS_CROSS_PLATFORMS

# Test for version newer than ICS
JB_OR_NEWER := $(shell test "${BRCM_ANDROID_VERSION}" \> "ics" && echo "y")

ifeq ($(JB_OR_NEWER),y)
LOCAL_CFLAGS += -DANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
endif

LOCAL_LDFLAGS := -L$(REFSW_PATH)/bin -lb_playback_ip -lb_os 

LOCAL_SRC_FILES := bcm_dms_server_jni.c
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
