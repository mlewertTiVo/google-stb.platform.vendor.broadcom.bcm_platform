#
# Configuration for HDCP2 DRM Container Filter
#
LIB_NAME   := drm_hdcp2
LIB_SRC    := hdcp2_drm.c
LIB_LIBS   := middleware/hdcp2/rx/rx middleware/hdcp2/tx/tx
LIB_VPATH  := 
LIB_DEF    := 
#LIB_CFLAGS := $(WARNINGS_ARE_ERRORS)
LIB_AFLAGS :=

VLL_MEMMAP :=
VLL_ENTRYPOINTS := ../../filter_entrypts.txt
