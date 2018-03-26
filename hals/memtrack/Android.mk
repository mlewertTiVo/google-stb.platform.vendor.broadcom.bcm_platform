LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SRC_FILES:=  \
    brcm_memtrack.cpp

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DLOG_TAG=\"bcm-mem\"
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_C_INCLUDES += $(TOP)/hardware/libhardware/include \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem \
                    $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_HEADER_LIBRARIES := liblog_headers

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient
LOCAL_SHARED_LIBRARIES += libutils

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE := memtrack.$(TARGET_BOARD_PLATFORM)

include $(BUILD_SHARED_LIBRARY)
