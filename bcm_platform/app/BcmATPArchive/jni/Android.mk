ifeq ($(BRCM_ANDROID_VERSION),jb)
REFSW_PATH := vendor/broadcom/bcm${BCHP_CHIP}/brcm_nexus
else
REFSW_PATH := vendor/broadcom/bcm_platform/brcm_nexus
endif
LOCAL_PATH := $(call my-dir)
APPLIB_TOP := $(LOCAL_PATH)/../../../../../../..

include $(APPLIB_TOP)/broadcom/services/media/$(NEXUS_PLATFORM)$(BCHP_VER_LOWER).$(B_REFSW_ARCH).$(BUILD_TYPE)/components/mediaplayer/proxy/media_mediaplayer_proxy.inc
include $(CLEAR_VARS)

LOCAL_MODULE            := libjni_bcmtp
LOCAL_SRC_FILES         := BcmTP_jni.cpp
LOCAL_PRELINK_MODULE    := false
LOCAL_SHARED_LIBRARIES  := liblog libstlport
LOCAL_STATIC_LIBRARIES  := libtrellis_trellis-core libtrellis_media_media_proxy libtrellis_media_mediaplayer_proxy
LOCAL_MODULE_TAGS       := eng
LOCAL_C_INCLUDES        += $(APPLIB_TOP)/broadcom/trellis-core/include \
						   $(APPLIB_TOP)/broadcom/services/media/components/media/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/media/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/broadcastsource/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/broadcastsource/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/mediaplayer/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/mediaplayer/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/dvrsource/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/dvrsource/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/filesource/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/filesource/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/networksource/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/networksource/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/pushsource/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/pushsource/proxy/generated \
						   $(APPLIB_TOP)/broadcom/services/media/components/source/proxy \
						   $(APPLIB_TOP)/broadcom/services/media/components/source/proxy/generated \
						   $(APPLIB_TOP)/broadcom/trellis-core/rpc \
						   $(APPLIB_TOP)/broadcom/trellis-core/rpc/orb \
						   $(APPLIB_TOP)/broadcom/trellis-core/common \
						   $(APPLIB_TOP)/broadcom/trellis-core/json \
						   $(APPLIB_TOP)/broadcom/trellis-core/osapi \
						   $(APPLIB_TOP)/opensource/boost/boost_1_52_0 \
                           external/stlport/stlport \
                           bionic

LOCAL_CFLAGS			+= $(MEDIA_MEDIAPLAYER_PROXY_CFLAGS) -DTRELLIS_ANDROID_BUILD
						   
include $(BUILD_SHARED_LIBRARY)

#LOCAL_SHARED_LIBRARIES  := liblog libstlport libtrellis_trellis-core libtrellis_media_media_proxy libtrellis_media_mediaplayer_proxy
