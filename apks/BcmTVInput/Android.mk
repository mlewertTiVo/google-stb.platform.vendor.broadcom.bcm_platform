#
# Copyright 2014 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CERTIFICATE := $(BCM_VENDOR_STB_ROOT)/bcm_platform/signing/bcmstb

LOCAL_SRC_FILES := $(call all-java-files-under, src)

# Leanback support
LOCAL_STATIC_ANDROID_LIBRARIES += \
    android-support-v17-leanback \
    android-support-v7-recyclerview \
    android-support-compat \
    android-support-core-ui \
    android-support-media-compat \
    android-support-fragment

LOCAL_STATIC_JAVA_LIBRARIES += android-support-annotations
LOCAL_STATIC_JAVA_LIBRARIES += framework

LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res
LOCAL_USE_AAPT2 := true

LOCAL_JNI_SHARED_LIBRARIES := libbcmtuner

LOCAL_PACKAGE_NAME := BcmTVInput

LOCAL_PRIVILEGED_MODULE := true
LOCAL_MODULE_TAGS := optional
#LOCAL_PROPRIETARY_MODULE := true
LOCAL_SDK_VERSION := system_current

include $(BUILD_PACKAGE)

include $(LOCAL_PATH)/jni/Android.mk
