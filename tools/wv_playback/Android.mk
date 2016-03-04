ifeq ($(BUILD_WV_PLAYBACK),y)
LOCAL_PATH := $(call my-dir)/../../../../../..

include $(CLEAR_VARS)

WV_PLAYBACK_DIR := $(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/third_party/widevine/CENC21/wv_playback
ANDROID_WV_DRM_DIR := vendor/widevine/libwvdrmengine

LOCAL_SRC_FILES := \
    $(WV_PLAYBACK_DIR)/wv_playback.cpp \
    $(WV_PLAYBACK_DIR)/bmp4.c \
    $(WV_PLAYBACK_DIR)/mp4_parser.c \
    $(WV_PLAYBACK_DIR)/cenc_parser.c

ifneq ($(USE_CURL),y)
LOCAL_SRC_FILES += \
    $(ANDROID_WV_DRM_DIR)/cdm/core/test/http_socket.cpp \
    $(ANDROID_WV_DRM_DIR)/cdm/core/test/url_request.cpp
endif

LOCAL_C_INCLUDES := \
    $(TOP)/bionic \
    $(TOP)/external/stlport/stlport \
    $(TOP)/external/openssl/include \
    $(TOP)/$(ANDROID_WV_DRM_DIR)/cdm/include \
    $(TOP)/$(ANDROID_WV_DRM_DIR)/cdm/core/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/common_crypto/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/media \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/utils \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/nexus/nxclient/include \

ifeq ($(SAGE_SUPPORT),y)
LOCAL_C_INCLUDES += \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/BSEAV/lib/security/sage/srai/include \
    $(TOP)/$(BCM_VENDOR_STB_ROOT)/refsw/magnum/syslib/sagelib/include
endif

ifneq ($(USE_CURL),y)
LOCAL_C_INCLUDES += \
    $(TOP)/external/gtest/include \
    $(TOP)/$(ANDROID_WV_DRM_DIR)/cdm/core/test
endif

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libnexus \
    libnxclient \
    libstlport \
    libssl \
    libcrypto

ifeq ($(SAGE_SUPPORT),y)
LOCAL_SHARED_LIBRARIES += \
    libsrai
endif

# Below is Android only: we should link to libwvcdm.so for Linux
LOCAL_SHARED_LIBRARIES += \
    libwvdrmengine

LOCAL_MODULE := wv_playback

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
LOCAL_CFLAGS += -DDRM_INLINING_SUPPORTED=1 -DDRM_BUILD_PROFILE=900 -DTARGET_LITTLE_ENDIAN=1 -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0

ifeq ($(SAGE_SUPPORT),y)
LOCAL_CFLAGS += -DUSE_SECURE_VIDEO_PLAYBACK=1 -DUSE_SECURE_AUDIO_PLAYBACK=1
else
LOCAL_CFLAGS += -DUSE_SECURE_VIDEO_PLAYBACK=0 -DUSE_SECURE_AUDIO_PLAYBACK=0
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
endif # BUILD_WV_PLAYBACK

