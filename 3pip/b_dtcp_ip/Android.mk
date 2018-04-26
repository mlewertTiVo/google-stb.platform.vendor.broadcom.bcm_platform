LOCAL_PATH := $(call my-dir)
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
ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
include $(NEXUS_TOP)/lib/dtcp_ip_sage/dtcp_ip_lib.inc
else ifeq ($(DTCP_IP_SUPPORT),y)
include $(NEXUS_TOP)/lib/dtcp_ip/dtcp_ip_lib.inc
endif
B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser

LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_DTCP_IP_LIB_PUBLIC_INCLUDES) $(B_DTCP_IP_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(B_DTCP_IP_LIB_DEFINES))

LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),../../../refsw/nexus,$(B_DTCP_IP_LIB_SOURCES))

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libnxclient               \
        libbcrypt                 \
        libcmndrm

ifeq ($(DTCP_IP_SAGE_SUPPORT),y)
LOCAL_SHARED_LIBRARIES += libsrai libcmndrm_tl libdrmrootfs libbcrypt
else ifeq ($(DTCP_IP_SUPPORT),y)
LOCAL_SHARED_LIBRARIES += libcrypto libssl
endif

LOCAL_MODULE := libb_dtcp_ip
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

endif #ifeq ($(DTCP_ENABLED),y)
