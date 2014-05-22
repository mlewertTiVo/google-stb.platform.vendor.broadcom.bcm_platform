ifeq ($(BRCM_MIPS_TARGET),y)
	ifeq ($(BRCM_ANDROID_VERSION),jb)
		JWEBSOCKET_LIB := ../../../mips-jb/prebuilts/misc/common/jwebsocket_client
	else
		JWEBSOCKET_LIB := ../../../mips-ics/prebuilt/common/jwebsocket_client
	endif
else
	ifeq ($(BRCM_ANDROID_VERSION),jb)
		JWEBSOCKET_LIB := ../../../arm-jb/prebuilts/misc/common/jwebsocket_client
	else
		JWEBSOCKET_LIB := ../../../stb-droid/prebuilt/common/jwebsocket_client
	endif
endif

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

KK_OR_NEWER := $(shell test "${BRCM_ANDROID_VERSION}" \> "jb" && echo "y")

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_STATIC_JAVA_LIBRARIES = jWebSocket_ClientAPI_3 jWebSocket_Common_3 jWebSocket_SEClient_3 jWebSocket_javolution_3 

ifeq ($(KK_OR_NEWER),y)
        LOCAL_PROGUARD_ENABLED := disabled
endif

LOCAL_PACKAGE_NAME := BcmRemoteUI

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := jWebSocket_ClientAPI_3:$(JWEBSOCKET_LIB)/jWebSocketClientAPI-1.0.jar jWebSocket_Common_3:$(JWEBSOCKET_LIB)/jWebSocketCommon-1.0.jar jWebSocket_SEClient_3:$(JWEBSOCKET_LIB)/jWebSocketJavaSEClient-1.0.jar jWebSocket_javolution_3:$(JWEBSOCKET_LIB)/javolution-5.5.1.jar 
include $(BUILD_MULTI_PREBUILT)
