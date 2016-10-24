include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_CFLAGS += -fexceptions

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_SRC_FILES := \
	server/connectors/unixdomainsocketserver.cpp \
	server/abstractprotocolhandler.cpp \
	server/abstractserverconnector.cpp \
	server/requesthandlerfactory.cpp \
	server/rpcprotocolserver12.cpp \
	server/rpcprotocolserverv1.cpp \
	server/rpcprotocolserverv2.cpp \
	common/errors.cpp \
	common/exception.cpp \
	common/procedure.cpp \
	common/specificationparser.cpp \
	common/specificationwriter.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp/src
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus

LOCAL_WHOLE_STATIC_LIBRARIES := libjsoncpp

LOCAL_MODULE := libjsonrpccpp-server
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
LOCAL_C_INCLUDES += $(NXCLIENT_INCLUDES)
LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)

LOCAL_CFLAGS += -fexceptions

LOCAL_PATH := $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_SRC_FILES := \
	common/errors.cpp \
	common/exception.cpp \
	common/procedure.cpp \
	common/specificationparser.cpp \
	common/specificationwriter.cpp \
	client/batchcall.cpp \
	client/batchresponse.cpp \
	client/client.cpp \
	client/rpcprotocolclient.cpp \
	client/connectors/unixdomainsocketclient.cpp

LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp/include
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared/jsonrpccpp/src
LOCAL_C_INCLUDES += $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/trellis/shared

LOCAL_SHARED_LIBRARIES :=         \
        libcutils                 \
        libutils                  \
        libnexus

LOCAL_WHOLE_STATIC_LIBRARIES := libjsoncpp

LOCAL_MODULE := libjsonrpccpp-client
include $(BUILD_SHARED_LIBRARY)
