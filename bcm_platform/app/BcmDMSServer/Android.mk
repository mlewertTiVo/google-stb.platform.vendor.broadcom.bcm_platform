LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := BcmDMSServer

# Though below is a JNI shared library, we are calling it a general shared library
# so that it gets copied to system/lib/.
LOCAL_SHARED_LIBRARIES := libbcm_dms_server_jni

LOCAL_SDK_VERSION := current

include $(BUILD_PACKAGE)

# Use the following include to make our test apk.
include $(call all-makefiles-under,$(LOCAL_PATH))
