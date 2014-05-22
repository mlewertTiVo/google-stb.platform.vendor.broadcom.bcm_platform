
LOCAL_PATH := $(call my-dir)

ifeq ($(ANDROID_USES_TRELLIS_WM),y)

#----------------------------------------------------
# lbtrellis_application_androidwindowmanager_impl.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := lbtrellis_application_androidwindowmanager_impl
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_androidwindowmanager_impl.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_androidwindowmanager_impl

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_delegatewindowmanager_impl.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_delegatewindowmanager_impl
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_delegatewindowmanager_impl.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_delegatewindowmanager_impl

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_windowmanager_impl.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_windowmanager_impl
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_windowmanager_impl.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_windowmanager_impl

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_applicationmanager_impl.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_applicationmanager_impl
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_applicationmanager_impl.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_applicationmanager_impl

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_authorizationattributesmanager_impl.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_authorizationattributesmanager_impl
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_authorizationattributesmanager_impl.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_authorizationattributesmanager_impl

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_authorizationattributesmanager_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_authorizationattributesmanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_authorizationattributesmanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_authorizationattributesmanager_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_packagemanager_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_packagemanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_packagemanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_packagemanager_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_windowmanager_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_windowmanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_windowmanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_windowmanager_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_servicemanager_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_servicemanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_servicemanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_servicemanager_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_applicationmanager_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_applicationmanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_applicationmanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_applicationmanager_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_service_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_service_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_service_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_service_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_inputdriver_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_inputdriver_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_inputdriver_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_inputdriver_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_window_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_window_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_window_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_window_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_application_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_application_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_application_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_application_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------------
# libtrellis_application_usernotification_proxy.a
#----------------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_application_usernotification_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_application_usernotification_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_application_usernotification_proxy

include $(BUILD_MULTI_PREBUILT)

#--------------------------
# libtrellis_trellis-core.a
#--------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_trellis-core
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_trellis-core.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_trellis-core

include $(BUILD_MULTI_PREBUILT)

endif

ifeq ($(ATP_BUILD_PASS2),y)
#--------------------------
# libtrellis_trellis-core.a
#--------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_trellis-core
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_trellis-core.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_trellis-core

include $(BUILD_MULTI_PREBUILT)

#-------------------------------
# libtrellis_media_media_proxy.a
#-------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_media_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_media_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_media_proxy

include $(BUILD_MULTI_PREBUILT)

#-------------------------------------
# libtrellis_media_mediaplayer_proxy.a
#-------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_mediaplayer_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_mediaplayer_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_mediaplayer_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------------------------
# libtrellis_media_generictvmediaplayer_proxy.a
#----------------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_generictvmediaplayer_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_generictvmediaplayer_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_generictvmediaplayer_proxy

include $(BUILD_MULTI_PREBUILT)

#-----------------------------------------
# libtrellis_media_broadcastsource_proxy.a
#-----------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_broadcastsource_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_broadcastsource_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_broadcastsource_proxy

include $(BUILD_MULTI_PREBUILT)

#---------------------------------------
# libtrellis_media_networksource_proxy.a
#---------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_networksource_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_networksource_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_networksource_proxy

include $(BUILD_MULTI_PREBUILT)

#------------------------------------
# libtrellis_media_pushsource_proxy.a
#------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_pushsource_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_pushsource_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_pushsource_proxy

include $(BUILD_MULTI_PREBUILT)

#-----------------------------------
# libtrellis_media_dvrsource_proxy.a
#-----------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_dvrsource_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_dvrsource_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_dvrsource_proxy

include $(BUILD_MULTI_PREBUILT)

#--------------------------------------
# libtrellis_media_mediamanager_proxy.a
#--------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_mediamanager_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_mediamanager_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_mediamanager_proxy

include $(BUILD_MULTI_PREBUILT)

#------------------------------------
# libtrellis_media_mediastore_proxy.a
#------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_mediastore_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_mediastore_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_mediastore_proxy

include $(BUILD_MULTI_PREBUILT)

#---------------------------------------
# libtrellis_media_mediastreamer_proxy.a
#---------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_mediastreamer_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_mediastreamer_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_mediastreamer_proxy

include $(BUILD_MULTI_PREBUILT)

#------------------------------------
# libtrellis_media_filesource_proxy.a
#------------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_filesource_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_filesource_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_filesource_proxy

include $(BUILD_MULTI_PREBUILT)

#--------------------------------
# libtrellis_media_source_proxy.a
#--------------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_source_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_source_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_source_proxy

include $(BUILD_MULTI_PREBUILT)

#----------------------------
# libtrellis_media_tv_proxy.a
#----------------------------
include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_tv_proxy
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := optional debug
LOCAL_PREBUILT_LIBS := libtrellis_media_tv_proxy.a
LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_tv_proxy

include $(BUILD_MULTI_PREBUILT)

#-----------------------------------
# libtrellis_media_generictv_proxy.a
#-----------------------------------
#include $(CLEAR_VARS)
#LOCAL_MODULE := libtrellis_media_generictv_proxy
#LOCAL_MODULE_TAGS := optional debug
#LOCAL_PREBUILT_LIBS := libtrellis_media_generictv_proxy.a
#LOCAL_MODULE_CLASS := STATIC_LIBRARIES
#LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/trellis_media_generictv_proxy

#include $(BUILD_MULTI_PREBUILT)
endif
