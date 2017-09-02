LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
DTCP_ENABLED := y
$(info Building with DTCP_IP_SAGE_SUPPORT)
else ifeq ($(DTCP_IP_SUPPORT),y)
DTCP_ENABLED := y
$(info Building with DTCP_IP_SUPPORT)
else
DTCP_ENABLED := n
$(warning DTCP_IP_SUPPORT or DTCP_IP_SAGE_SUPPORT not enabled, skipping)
endif

ifeq ($(DTCP_ENABLED),y)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_PRELINK_MODULE := false

B_LIB_TOP := $(NEXUS_TOP)/lib

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
include $(NEXUS_TOP)/lib/dtcp_ip_sage/dtcp_ip_lib.inc
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/dtcp_ip_sage/include
else ifeq ($(DTCP_IP_SUPPORT),y)
include $(NEXUS_TOP)/lib/dtcp_ip/dtcp_ip_lib.inc
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/dtcp_ip/include
endif

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/nxclient/apps/utils
LOCAL_C_INCLUDES += $(NEXUS_TOP)/utils

LOCAL_CFLAGS += $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += $(NXCLIENT_CFLAGS)
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS))
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += $(B_DTCP_IP_LIB_CFLAGS)

LOCAL_SHARED_LIBRARIES += libb_dtcp_ip libnexus libnxclient

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
LOCAL_SHARED_LIBRARIES += libsrai
endif

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
LOCAL_SRC_FILES := refsw/nexus/lib/dtcp_ip_sage/examples/http_client.c
else
LOCAL_SRC_FILES := refsw/nexus/lib/dtcp_ip/apps/http_client.c
endif

LOCAL_MODULE := dtcpip_client
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_OPTIONAL_EXECUTABLES)

include $(BUILD_EXECUTABLE)
endif # ifeq ($(DTCP_ENABLED),y)
