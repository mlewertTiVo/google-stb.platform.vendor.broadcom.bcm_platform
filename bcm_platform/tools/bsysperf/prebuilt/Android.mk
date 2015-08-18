LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bsysperf.html
LOCAL_SRC_FILES := ../../../../refsw/BSEAV/tools/bsysperf/bsysperf.html
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem
LOCAL_MODULE_CLASS := ETC
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := bsysperf.js
LOCAL_SRC_FILES := ../../../../refsw/BSEAV/tools/bsysperf/bsysperf.js
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nexus/bmem
LOCAL_MODULE_CLASS := ETC
include $(BUILD_PREBUILT)


