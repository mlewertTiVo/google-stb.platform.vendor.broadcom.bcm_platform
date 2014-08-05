LIB_NAME := containers_core
LIB_SRC  := containers.c containers_io.c containers_io_helpers.c containers_loader.c containers_filters.c \
            containers_utils.c containers_codecs.c containers_logging.c io_file.c io_null.c \
            containers_writer_utils.c containers_uri.c containers_bits.c io_net.c io_pktfile.c \
            net_sockets_null.c containers_list.c containers_index.c
LIB_SRC  += packetizers.c mpga_packetizer.c

LIB_IPATH :=
LIB_VPATH := core io net
LIB_VPATH += mpga
LIB_CFLAGS := $(WARNINGS_ARE_ERRORS)
LIB_AFLAGS  :=
LIB_DEFINES := VIDEOCORE
ifeq (TRUE,$(USE_DRM))
   LIB_LIBS := interface/utils/hdcp2_link_manager/hdcp2_link_manager middleware/hdcp2/rx/rx middleware/hdcp2/tx/tx
endif
