LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/dtcp_ip/dtcp_ip_lib.inc
LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),../../../../refsw/nexus,$(B_DTCP_IP_LIB_SOURCES))

LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_DTCP_IP_LIB_PUBLIC_INCLUDES) $(B_DTCP_IP_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(B_DTCP_IP_LIB_DEFINES))

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcrypto                 \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libssl                    \
        libbcrypt                 \
        libcmndrm


LOCAL_MODULE := libb_dtcp_ip
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
