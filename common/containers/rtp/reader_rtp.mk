#
# Configuration for RTP Reader
#
LIB_NAME   := reader_rtp
LIB_SRC    := rtp_reader.c rtp_base64.c rtp_mpeg4.c rtp_h264.c
LIB_LIBS   := 
LIB_IPATH  := 
LIB_VPATH  := 
LIB_DEF    := 
LIB_CFLAGS := $(WARNINGS_ARE_ERRORS)
LIB_AFLAGS :=

VLL_MEMMAP :=
VLL_ENTRYPOINTS := ../reader_entrypts.txt

