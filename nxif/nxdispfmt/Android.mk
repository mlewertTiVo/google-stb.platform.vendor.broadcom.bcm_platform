LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnexus
LOCAL_SHARED_LIBRARIES += libnxclient

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES) \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/apps/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/utils \
                    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/prop
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_HEADER_LIBRARIES := liblog_headers
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
# fix warnings!
LOCAL_CFLAGS += -Werror

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/
LOCAL_SRC_FILES := bcm_platform/nxif/nxdispfmt/nxdispfmt.c \
                   refsw/nexus/utils/namevalue.c

ifeq ($(LOCAL_NVI_LAYOUT),y)
LOCAL_INIT_RC := bcm_platform/nxif/nxdispfmt/nxdispfmt.nvi.rc
else
LOCAL_INIT_RC := bcm_platform/nxif/nxdispfmt/nxdispfmt.rc
endif

LOCAL_MODULE := nxdispfmt

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES

include $(BUILD_EXECUTABLE)

