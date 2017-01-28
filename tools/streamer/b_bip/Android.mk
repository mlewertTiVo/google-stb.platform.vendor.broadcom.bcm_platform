LOCAL_PATH := $(call my-dir)

#
# libb_bip_base
#
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),nexus,$(BIP_BASE_LIB_SOURCES))
LOCAL_SRC_FILES := $(subst nexus/lib/../../,,$(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(subst nexus/../,,$(LOCAL_SRC_FILES))

LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(BIP_BASE_LIB_PUBLIC_INCLUDES) $(BIP_BASE_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(BIP_BASE_LIB_DEFINES))
LOCAL_CFLAGS += -Drindex=strrchr

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libb_playback_ip          \
        libb_psip                 \
        libb_dtcp_ip              \
        libbcrypt                 \
        libcmndrm

LOCAL_MODULE := libb_bip_base
include $(BUILD_SHARED_LIBRARY)

#
# libb_bip_client
#
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),nexus,$(BIP_CLIENT_LIB_SOURCES))
LOCAL_SRC_FILES := $(subst nexus/lib/../../,,$(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(subst nexus/../,,$(LOCAL_SRC_FILES))

LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(BIP_CLIENT_LIB_PUBLIC_INCLUDES) $(BIP_CLIENT_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(BIP_CLIENT_LIB_DEFINES))
LOCAL_CFLAGS += -Drindex=strrchr

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libb_psip                 \
        libb_dtcp_ip              \
        libbcrypt                 \
        libcmndrm                 \
        libb_bip_base             \
        libb_playback_ip

LOCAL_MODULE := libb_bip_client
include $(BUILD_SHARED_LIBRARY)

#
# libb_bip_server
#
include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_C_INCLUDES += $(NEXUS_TOP)/lib/os/include \
                    $(NEXUS_TOP)/lib/os/include/linuxuser \
                    $(TOP)/external/boringssl/include
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

B_LIB_TOP := $(NEXUS_TOP)/lib
B_REFSW_OS ?= linuxuser
include $(NEXUS_TOP)/lib/bip/bip_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),nexus,$(BIP_SERVER_LIB_SOURCES))
LOCAL_SRC_FILES := $(subst nexus/lib/../../,,$(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(subst nexus/../,,$(LOCAL_SRC_FILES))

LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(BIP_SERVER_LIB_PUBLIC_INCLUDES) $(BIP_SERVER_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(BIP_SERVER_LIB_DEFINES))
LOCAL_CFLAGS += -Drindex=strrchr

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libb_psip                 \
        libb_bip_base             \
        libb_bip_client           \
        libbcrypt                 \
        libcmndrm                 \
        libb_dtcp_ip              \
        libb_playback_ip

LOCAL_MODULE := libb_bip_server
include $(BUILD_SHARED_LIBRARY)
