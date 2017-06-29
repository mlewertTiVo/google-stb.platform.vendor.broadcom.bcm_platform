LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                 \
        AnotherPacketSource.cpp   \
        ATSParser.cpp             \
        ESQueue.cpp               \
        MPEG2PSExtractor.cpp      \
        MPEG2TSExtractor.cpp      \

LOCAL_C_INCLUDES:= \
	$(TOP)/frameworks/av/media/libstagefright \
	$(TOP)/frameworks/native/include/media/openmax \
	$(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libstagefright_bcm

LOCAL_CFLAGS += -Werror -Wall
LOCAL_CLANG := true

LOCAL_MODULE:= libstagefright_mpeg2ts_bcm

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)