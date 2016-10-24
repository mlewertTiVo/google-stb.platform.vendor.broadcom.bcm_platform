include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_CFLAGS += -fexceptions

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server
LOCAL_SRC_FILES := \
	source/Transcoder.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/server/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/BSEAV/lib/tshdrbuilder

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libjsonrpccpp-client      \
        libjsonrpccpp-server      \
        libuv                     \
        libTrellisCore            \
        libTrellisMedia

LOCAL_WHOLE_STATIC_LIBRARIES := libbdvb
LOCAL_WHOLE_STATIC_LIBRARIES += libjsoncpp

LOCAL_MODULE := libTrellisMediaTranscoder
include $(BUILD_SHARED_LIBRARY)
