LOCAL_PATH := $(call my-dir)
FILTER_OUT_NEXUS_CFLAGS := -march=armv7-a

include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SRC_FILES:=  \
    brcm_memtrack.cpp

LOCAL_CFLAGS += $(filter-out $(FILTER_OUT_NEXUS_CFLAGS), $(NEXUS_CFLAGS))
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-mem\"
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += -DLOGD=ALOGD -DLOGE=ALOGE -DLOGW=ALOGW -DLOGV=ALOGV -DLOGI=ALOGI

LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include \
                    $(TOP)/vendor/broadcom/bcm_platform/libnexusservice \
                    $(TOP)/vendor/broadcom/bcm_platform/libnexusipc \
                    $(NXCLIENT_INCLUDES)

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnexusipcclient
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := memtrack.bcm_platform

include $(BUILD_SHARED_LIBRARY)
