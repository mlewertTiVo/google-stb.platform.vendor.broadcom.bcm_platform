############################################################
#     Copyright (c) 2003-2014, Broadcom Corporation
#     All Rights Reserved
#     Confidential Property of Broadcom Corporation
#
#  THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
#  AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
#  EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
#
############################################################

NXCLIENT_APPS = \
	soft_tsm_client

APPS = $(NXCLIENT_APPS)

PHONY=all clean
.PHONY: $(PHONY) server clean_server

NEXUS_PLATFORM ?= $(PLATFORM)
ifndef NEXUS_PLATFORM
$(error NEXUS_PLATFORM is not defined)
endif

ifeq ($(B_REFSW_REAL_MAKE),)
NEXUS_TOP := $(shell cd ../../nexus; pwd)

B_REFSW_OBJ_DIR ?= obj.${NEXUS_PLATFORM}
B_REFSW_OBJ_ROOT ?= ${NEXUS_TOP}/../${B_REFSW_OBJ_DIR}
BINDIR := ${B_REFSW_OBJ_ROOT}/nexus/bin

${PHONY} ${APPS}: ${BINDIR}/exists
	@${MAKE} -C ${BINDIR} BINDIR=${BINDIR} NEXUS_TOP=${NEXUS_TOP} B_REFSW_REAL_MAKE=1 -f ${CURDIR}/Makefile $@

${BINDIR}/exists:
	@mkdir -p $@

server: ${BINDIR}/exists
	$(MAKE) -C $(NEXUS_TOP)/nxclient/server

clean_server:
	$(MAKE) -C $(NEXUS_TOP)/nxclient/server clean

else

ifdef B_REFSW_REAL_NEXUS_TOP
NEXUS_TOP ?= ${B_REFSW_REAL_NEXUS_TOP}
endif

BSEAV = $(NEXUS_TOP)/../BSEAV

include $(BSEAV)/lib/bfile/bfile.inc
ifeq ($(MEDIA_AVI_SUPPORT),y)
include $(BSEAV)/lib/avi/bavi.inc
endif
ifeq ($(MEDIA_ASF_SUPPORT),y)
include $(BSEAV)/lib/asf/basf.inc
endif
include $(BSEAV)/lib/utils/batom.inc
include $(BSEAV)/lib/media/bmedia.inc

RDKOMX_TOP := $(NEXUS_TOP)/../rdkomx

vpath %.c $(RDKOMX_TOP)/src
vpath %.cpp $(RDKOMX_TOP)/src
vpath %.c $(NEXUS_TOP)/../magnum/commonutils/udp/src

# Add our include directories
CXXFLAGS += -I$(RDKOMX_TOP)/include -I$(RDKOMX_TOP)/src
CXXFLAGS+= -fno-rtti -fno-exceptions

ifeq ($(NEXUS_PREBUILT_BINARY),y)
# do "make nexus" and "make nexus_headers" first, then "make NEXUS_PREBUILT_BINARY=y"
NEXUS_BIN_DIR ?= $(NEXUS_TOP)/../obj.${NEXUS_PLATFORM}/bin
include $(NEXUS_BIN_DIR)/include/platform_app.inc
Q_ ?= @
else
include $(NEXUS_TOP)/platforms/$(NEXUS_PLATFORM)/build/platform_app.inc
endif

# NEXUS flags
CFLAGS += -g $(NEXUS_CFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DB_CLIENT=1
CXXFLAGS += -g  $(NEXUS_CXXFLAGS) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES)) -DB_CLIENT=1
# BSEAV CFLAGS
CFLAGS += $(addprefix -I,$(BMEDIA_INCLUDES) $(BFILE_MEDIA_INCLUDES)) -I$(BSEAV)/lib/mpeg2_ts_parse -I$(BSEAV)/lib/tspsi -I$(NEXUS_TOP)/utils
CXXFLAGS += $(addprefix -I,$(BMEDIA_INCLUDES) $(BFILE_MEDIA_INCLUDES)) -I$(BSEAV)/lib/mpeg2_ts_parse -I$(BSEAV)/lib/tspsi -I$(NEXUS_TOP)/utils
CFLAGS += $(addprefix -D,$(BMEDIA_PROBE_DEFINES))
CXXFLAGS += $(addprefix -D,$(BMEDIA_PROBE_DEFINES))
CFLAGS += $(addprefix -I,$(BFILE_ASYNC_INCLUDES))
CXXFLAGS += $(addprefix -I,$(BFILE_ASYNC_INCLUDES))
# NXCLIENT flags
include $(NEXUS_TOP)/nxclient/include/nxclient.inc
CXXFLAGS += $(NXCLIENT_CFLAGS)
LDFLAGS += $(NXCLIENT_LDFLAGS)
# DCC flags
CXXFLAGS += -I$(NEXUS_TOP)/../magnum/commonutils/avc/include -I$(NEXUS_TOP)/../magnum/commonutils/udp/include

ifneq ($(NEXUS_WEBCPU),)
CXXFLAGS += -DNEXUS_WEBCPU
endif

B_LIB_TOP = $(NEXUS_TOP)/lib
include $(B_LIB_TOP)/os/b_os_lib.inc
LDFLAGS += -lb_os

APPLIB_INCLUDES := $(foreach lib,$(B_LIBS),$($(lib)_PUBLIC_INCLUDES))
APPLIB_DEFINES := $(foreach lib,$(B_LIBS),$($(lib)_DEFINES))

CXXFLAGS += $(addprefix -I,$(APPLIB_INCLUDES)) $(addprefix -D,$(APPLIB_DEFINES))

OMXIL_SRC = \
    bomx_core.cpp \
    bomx_utils.cpp \
    bomx_buffer.cpp \
    bomx_port.cpp \
    bomx_component.cpp \
    bomx_clock.cpp \
    bomx_video_decoder.cpp \
    bomx_video_scheduler.cpp \
    bomx_video_renderer.cpp \
    bomx_audio_decoder.cpp \
    bomx_audio_renderer.cpp \

OMXIL_C_SRC = \
    budp_bitread.c \
    budp_dccparse.c

OMXIL_OBJECTS=$(foreach src,$(OMXIL_SRC:.cpp=.o),$(BINDIR)/$(src))
OMXIL_OBJECTS+=$(foreach src,$(OMXIL_C_SRC:.c=.o),$(BINDIR)/$(src))

CXXFLAGS += -MD -O2 -fno-rtti -fno-exceptions
CXXFLAGS += $(B_REFSW_CFLAGS) $(B_REFSW_GENERIC_MAGNUM_CFLAGS) $(B_REFSW_MAGNUM_INCLUDE_DIRS)
CXXFLAGS += -fPIC
CXXFLAGS := $(filter-out -pedantic,$(CXXFLAGS))

OMXIL_LIB=$(BINDIR)/libomxil.so

ifneq ($(VERBOSE),y)
Q_?=@
endif

.PHONY: omxil clean

all: omxil

omxil: $(OMXIL_LIB)

ifeq ($(BWIN_SUPPORT),y)
include $(BSEAV)/lib/bwin/include/bwin.mak
CXXFLAGS += ${BWIN_CFLAGS} -DBWIN_SUPPORT
endif

ifeq ($(DSG_SUPPORT), y)
CXXFLAGS += -DDSG_SUPPORT
endif

$(BINDIR)/%.o: %.cpp
	@echo [Compile... $(notdir $<)]
	${Q_}$(CXX) -c -o $@ $< $(CXXFLAGS)
ifeq ($(SYSTEM),vxworks)
ifneq ($(vxWorksVersion),6)
	@$(MV) $(patsubst %.o,%.d,$(notdir $@)) $(BINDIR)
endif
endif

$(BINDIR)/%.o: %.c
	@echo [Compile... $(notdir $<)]
	${Q_}$(CXX) -c -o $@ $< $(CXXFLAGS)
ifeq ($(SYSTEM),vxworks)
ifneq ($(vxWorksVersion),6)
	@$(MV) $(patsubst %.o,%.d,$(notdir $@)) $(BINDIR)
endif
endif

B_LIB_LDFLAGS = $(LDFLAGS) -fPIC \
		-Wl,--gc-sections -Wl,-shared,-Bsymbolic \
		-L$(NEXUS_BIN_DIR) \
		-Wl,--no-whole-archive -lm \
		-Wl,--no-undefined \
		-Wl,--whole-archive

$(OMXIL_LIB): $(OMXIL_OBJECTS)
	@echo [Linking... $(notdir $@)]
	@$(CXX) -shared -Wl,-x -o $@ -Wl,-soname,libomxil.so $^ ${B_LIB_LDFLAGS}

clean:
	-${Q_}$(RM) *.o *.a *.so *.d
	-${Q_}$(RM) $(BINDIR)/*.o $(BINDIR)/$(OMXIL_LIB) $(BINDIR)/*.d

-include $(BINDIR)/*.d
endif
