ifeq ($(BOARD_HAVE_VENDOR_CONTAINERS_LIB),y)

ifneq ($(USE_BRCM_PREBUILT),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Core
LOCAL_SRC_FILES += \
	core/containers_utils.c	\
	core/containers_writer_utils.c	\
	core/packetizers.c	\
	core/containers_list.c	\
	core/containers_loader.c	\
	core/containers_logging.c	\
	core/containers_uri.c	\
	core/containers_index.c	\
	core/containers_io.c	\
	core/containers_io_helpers.c	\
	core/containers_filters.c	\
	core/containers_bits.c	\
	core/containers.c	\
	core/containers_codecs.c

# I/O plugins
LOCAL_SRC_FILES += \
	net/net_sockets_bsd.c	\
	net/net_sockets_common.c	\
	io/io_http.c	\
	io/io_net.c	\
	io/io_null.c	\
	io/io_pktfile.c	\
	io/io_file.c	\

# Reader plugins
LOCAL_SRC_FILES += \
	avi/avi_reader.c	\
	asf/asf_reader.c	\
	mp4/mp4_reader.c	\
	mpga/mpga_reader.c	\
	mkv/matroska_reader.c	\
	wav/wav_reader.c	\
	flash/flv_reader.c	\
	mpeg/ps_reader.c	\
	binary/binary_reader.c	\
	simple/simple_reader.c	\
	raw/raw_video_reader.c	\
	rtp/rtp_reader.c	\
	rtp/rtp_base64.c	\
	rtp/rtp_h264.c	\
	rtp/rtp_mpeg4.c	\
	rcv/rcv_reader.c	\
	rv9/rv9_reader.c	\
	qsynth/qsynth_reader.c	\
	rtsp/rtsp_reader.c

# Writer plugins
LOCAL_SRC_FILES += \
	avi/avi_writer.c	\
	mp4/mp4_writer.c	\
	simple/simple_writer.c	\
	raw/raw_video_writer.c	\
	binary/binary_writer.c

# Packetizer plugins
LOCAL_SRC_FILES += \
	pcm/pcm_packetizer.c	\
	mpga/mpga_packetizer.c	\
	h264/avc1_packetizer.c  \
	mpgv/mpgv_packetizer.c

# Metadata reader plugins
LOCAL_SRC_FILES += \
	metadata/id3/id3_metadata_reader.c

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)),$(filter $(strip $(TARGET_BOARD_PLATFORM)),hawaii java))
#VCOS_C_INCLUDES := \
#	$(TOP)/vendor/broadcom/rhea_hawaii/vcos	\
#	$(TOP)/vendor/broadcom/rhea_hawaii/vcos/interface/vcos	\
#	$(TOP)/vendor/broadcom/rhea_hawaii/vcos/interface/vcos/pthreads
#VCOS_LIB := libVCOS
#else
#VCOS_C_INCLUDES := \
#	$(TOP)/vendor/broadcom/common/vcos/include	\
#	$(TOP)/vendor/broadcom/common/vcos/pthreads	\
#	$(TOP)/vendor/broadcom/common/vcos
#VCOS_LIB := libvcos
#endif

LOCAL_C_INCLUDES += \
	$(VCOS_C_INCLUDES)	\
	$(TOP) \
	$(TOP)/vendor/broadcom/common	\
	$(TOP)/vendor/broadcom/common/containers


LOCAL_CFLAGS += -DENABLE_CONTAINERS_STANDALONE
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-maybe-uninitialized
LOCAL_CFLAGS += -Werror

LOCAL_MODULE:= libcontainers
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libc
#LOCAL_SHARED_LIBRARIES += $(VCOS_LIB)

LOCAL_SRC_FILES += brcm_revision.c
REVISION=$(shell  cd $(LOCAL_PATH) && git log -1 --pretty=format:"%H  %cd"|sed -e 's/ /_/g' -e 's/(//g' -e 's/)//g' )
TB=$(shell cd $(LOCAL_PATH) && git branch -vv | head -n 1 | sed -e 's/ *(.*) *//' -e 's/ .*//')
TB_1=$(shell  cd $(LOCAL_PATH) && git remote -v  |sed -e 's/.*\///' |head -n 1|sed -e 's/ /_/g' -e 's/(.*)//')
LOCAL_CFLAGS += -D__REVISION__="\"$(TB_1)_$(TB)_$(REVISION)\""
include $(BUILD_SHARED_LIBRARY)

# Build containers-test app
# We do not link against libcontainers since we want to enable all the
# extra logging
LOCAL_CONTAINERS_SRC_FILES := $(LOCAL_SRC_FILES)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -DENABLE_CONTAINERS_STANDALONE
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-maybe-uninitialized
LOCAL_CFLAGS += -Werror
LOCAL_CFLAGS += -DENABLE_CONTAINERS_LOG_FORMAT -DENABLE_CONTAINER_LOG_DEBUG

LOCAL_SRC_FILES += $(LOCAL_CONTAINERS_SRC_FILES)
LOCAL_SRC_FILES += test/test.c

LOCAL_C_INCLUDES += \
	$(VCOS_C_INCLUDES)			\
	$(TOP)/vendor/broadcom/common		\
	$(TOP) \
	$(TOP)/vendor/broadcom/common/containers

LOCAL_MODULE := containers-test
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng debug

LOCAL_SHARED_LIBRARIES := liblog libc
LOCAL_SHARED_LIBRARIES += $(VCOS_LIB)

include $(BUILD_EXECUTABLE)

else # USE_BRCM_PREBUILT
######## customer build: grab the prebuilt library #################
include $(CLEAR_VARS)
LOCAL_PATH := $(TARGET_PREBUILT_LIBS)
LOCAL_SRC_FILES := libcontainers.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libcontainers
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SRC_FILES += brcm_revision.c
REVISION=$(shell  cd $(LOCAL_PATH) && git log -1 --pretty=format:"%H  %cd"|sed -e 's/ /_/g' -e 's/(//g' -e 's/)//g' )
TB=$(shell cd $(LOCAL_PATH) && git branch -vv | head -n 1 | sed -e 's/ *(.*) *//' -e 's/ .*//')
TB_1=$(shell  cd $(LOCAL_PATH) && git remote -v  |sed -e 's/.*\///' |head -n 1|sed -e 's/ /_/g' -e 's/(.*)//')
LOCAL_CFLAGS += -D__REVISION__="\"$(TB_1)_$(TB)_$(REVISION)\""
include $(BUILD_PREBUILT)
endif # USE_BRCM_PREBUILT

endif
