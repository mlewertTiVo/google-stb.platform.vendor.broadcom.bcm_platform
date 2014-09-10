ifeq ($(BRCM_ANDROID_VERSION),jb)
REFSW_PATH := vendor/broadcom/bcm${BCHP_CHIP}/brcm_nexus
else
REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus
endif

LOCAL_PATH:= $(call my-dir)

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
$(warning NX-BUILD: CFG: ${DEVICE_REFSW_BUILD_CONFIG}, CFLAGS ${NEXUS_CFLAGS}, CLI-CFLAGS: ${NXCLIENT_CFLAGS}, CLI-INC: ${NXCLIENT_INCLUDES})

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libnexusipcclient $(NEXUS_LIB)

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_SHARED_LIBRARIES += libnxclient
else
LOCAL_SHARED_LIBRARIES += libnexusservice
endif

LOCAL_C_INCLUDES += $(REFSW_PATH)/bin/include $(JNI_H_INCLUDE) 
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusservice
LOCAL_C_INCLUDES += $(REFSW_PATH)/../libnexusipc

LOCAL_SRC_FILES := jni_generalSTBFunctions.cpp

LOCAL_CFLAGS:= $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DANDROID

# Test for version newer than ICS
JB_OR_NEWER := $(shell test "${BRCM_ANDROID_VERSION}" \> "ics" && echo "y")

ifeq ($(JB_OR_NEWER),y)
LOCAL_CFLAGS += -DANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI
endif

ifeq ($(ANDROID_SUPPORTS_NXCLIENT),y)
LOCAL_CFLAGS += -DANDROID_SUPPORTS_NXCLIENT
endif
LOCAL_MODULE := libjni_generalSTBFunctions
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
