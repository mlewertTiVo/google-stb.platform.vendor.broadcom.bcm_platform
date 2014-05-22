ifeq ($(strip $(BOARD_HAVE_VENDOR_CONTAINERS_LIB)),y)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#ifeq ($(filter $(PLATFORM_SDK_VERSION),1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18),$(PLATFORM_SDK_VERSION))
# Pre-KitKat
#else
# KitKat
LOCAL_CFLAGS := -DUSE_GOOGLE_MATROSKA_PARSER
LOCAL_CFLAGS += -D__KITKAT__
#LOCAL_CFLAGS += -DBCG_PORT_FLAG     #if this flag is enabled, then the code is exact same as the MPG, which involves the extend media codecs, we'd like to add in another thread.
#endif

LOCAL_CFLAGS += -DBOARD_HAVE_VENDOR_CONTAINERS_LIB

ifeq ($(OMX_EXTEND_CODECS_SUPPORT),y)
LOCAL_CFLAGS += -DOMX_EXTEND_CODECS_SUPPORT
endif

LOCAL_SRC_FILES =                         \
        ContainersExtractor.cpp           \
        MediaCircularBufferGroup.cpp      \
        CodecSpecific.cpp

LOCAL_C_INCLUDES =                        \
        $(TOP)/frameworks/base/include/media/stagefright \
        $(TOP)/frameworks/base/include/media/stagefright/foundation \
        $(TOP)/frameworks/av/include/media/stagefright \
        $(TOP)/frameworks/av/include/media/stagefright/foundation \
	$(TOP) \
        $(TOP)/vendor/broadcom/common

LOCAL_SHARED_LIBRARIES := libcontainers

LOCAL_MODULE_TAGS := eng debug
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libstagefright_containers

include $(BUILD_STATIC_LIBRARY)

endif

