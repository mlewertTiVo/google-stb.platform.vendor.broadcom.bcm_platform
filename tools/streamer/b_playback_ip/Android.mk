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
include $(NEXUS_TOP)/lib/playback_ip/b_playback_ip_lib.inc
LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw
LOCAL_SRC_FILES := $(subst $(NEXUS_TOP),nexus,$(B_PLAYBACK_IP_LIB_SOURCES))
LOCAL_SRC_FILES := $(subst nexus/lib/../../,,$(LOCAL_SRC_FILES))
LOCAL_SRC_FILES := $(subst nexus/../,,$(LOCAL_SRC_FILES))

LOCAL_C_INCLUDES += $(subst $(B_LIB_TOP),$(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/lib,$(B_PLAYBACK_IP_LIB_PUBLIC_INCLUDES) $(B_PLAYBACK_IP_LIB_PRIVATE_INCLUDES))
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))
LOCAL_CFLAGS += -DLINUX
LOCAL_CFLAGS += $(addprefix -D,$(B_PLAYBACK_IP_LIB_DEFINES))

LOCAL_SHARED_LIBRARIES :=         \
        libb_os                   \
        libcrypto                 \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libssl                    \
        libb_dtcp_ip              \
        libbcrypt                 \
        libcmndrm

LOCAL_MODULE := libb_playback_ip
include $(BUILD_SHARED_LIBRARY)
