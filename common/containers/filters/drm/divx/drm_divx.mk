#
# Configuration for DIVX DRM Container Filter
#
LIB_NAME   := drm_divx
LIB_SRC    := divx_drm.c
LIB_LIBS   := thirdparty/middleware/drm/divx/divx_drm_lib
LIB_IPATH  := ../../../../thirdparty/middleware/drm/divx/
LIB_VPATH  := 
LIB_DEF    := 
LIB_CFLAGS := $(WARNINGS_ARE_ERRORS)
LIB_AFLAGS :=

VLL_MEMMAP :=
VLL_ENTRYPOINTS := ../../divx_drm_filter_entrypts.txt