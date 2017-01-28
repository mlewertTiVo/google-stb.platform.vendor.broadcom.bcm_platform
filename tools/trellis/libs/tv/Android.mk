LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/
LOCAL_SRC_FILES := \
	source/DVBChannel.cpp \
	source/DVBDelegate.cpp \
	source/DVBProgram.cpp \
	source/DVBSource.cpp \
	source/ReconstructRecordObjects.cpp \
	source/Record.cpp \
	source/RecordScheduler.cpp \
	source/SubtitleDisplay.cpp \
	source/TVChannel.cpp \
	source/TVManager.cpp \
	source/TVProgram.cpp \
	source/TVRecording.cpp \
	source/TVSettings.cpp \
	source/TVSource.cpp \
	source/TVSourceFactory.cpp \
	source/TVTuner.cpp

LOCAL_CFLAGS += -DTRELLIS_BUILD
LOCAL_CFLAGS += -DLINUX

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/player/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/src/si/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/media/tv/dvb/src/utils/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus                  \
        libnxclient               \
        libjsonrpccpp-client      \
        libjsonrpccpp-server      \
        libuv                     \
        libTrellisCore            \
        libTrellisMedia

LOCAL_WHOLE_STATIC_LIBRARIES := libbdvb
LOCAL_WHOLE_STATIC_LIBRARIES += libjsoncpp

LOCAL_MODULE := libTrellisTV
include $(BUILD_SHARED_LIBRARY)
