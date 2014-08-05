cmake_minimum_required (VERSION 2.6)
project (CONTAINERS)
SET( SOURCE_DIR ${CONTAINERS_SOURCE_DIR}/.. )

SET( CMAKE_MAKE_PROGRAM make )
SET( CMAKE_BUILD_TOOL make )
include(CheckCCompilerFlag)

SET( CMAKE_VERBOSE_MAKEFILE off )
SET( CMAKE_C_FLAGS )
SET( CMAKE_CXX_FLAGS )

CHECK_C_COMPILER_FLAG(-mno-cygwin COMPILER_SUPPORTS_MNO_CYGWIN)
CHECK_C_COMPILER_FLAG(-Wno-missing-field-initializers COMPILER_SUPPORTS_NMFI)
CHECK_C_COMPILER_FLAG(-Wno-unused-value COMPILER_SUPPORTS_NUV)

# This is used to allow overriding the cygwin mode by editing CMakeCache.txt
if (CYGWIN AND NOT COMPILER_SUPPORTS_MNO_CYGWIN)
SET (CYGWIN 0)
SET (WIN32 0)
SET (LINUX 1)
ADD_DEFINITIONS( -mno-win32 )
SET( CMAKE_EXE_LINKER_FLAGS -mno-win32 )
SET( CMAKE_SHARED_LINKER_FLAGS -mno-win32 )
SET( CMAKE_MODULE_LINKER_FLAGS -mno-win32 )
endif (CYGWIN AND NOT COMPILER_SUPPORTS_MNO_CYGWIN)

SET( GCC_COMPILER_FLAGS "-Wall -g -O0 -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings -Wundef" )
SET( GCC_COMPILER_FLAGS "${GCC_COMPILER_FLAGS} -Wextra" )#-Wno-missing-field-initializers )
SET( GCC_COMPILER_FLAGS "${GCC_COMPILER_FLAGS} -D__STDC_VERSION__=199901L" )
if (COMPILER_SUPPORTS_NMFI)
SET( GCC_COMPILER_FLAGS "${GCC_COMPILER_FLAGS} -Wno-missing-field-initializers" )
endif (COMPILER_SUPPORTS_NMFI)
if (COMPILER_SUPPORTS_NUV)
SET( GCC_COMPILER_FLAGS "${GCC_COMPILER_FLAGS} -Wno-unused-value" )
endif (COMPILER_SUPPORTS_NUV)

ADD_DEFINITIONS( -DENABLE_CONTAINERS_STANDALONE -DENABLE_CONTAINERS_LOG_FORMAT -DENABLE_CONTAINER_LOG_DEBUG )

# Linux target detection
if (UNIX AND NOT CYGWIN)
SET (LINUX 1)
endif (UNIX AND NOT CYGWIN)

if (LINUX)
SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_COMPILER_FLAGS}" )

# subset of those only are OK for C++
SET(CMAKE_CXX_FLAGS "-Wmissing-declarations -Wcast-qual -Wundef -Wextra" )
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -Wno-unused-value" )
endif (LINUX)

if (CYGWIN)
ADD_DEFINITIONS( -mno-cygwin ${GCC_COMPILER_FLAGS} -D__int64=long\ long )
SET( CMAKE_EXE_LINKER_FLAGS -mno-cygwin )
SET( CMAKE_SHARED_LINKER_FLAGS -mno-cygwin )
SET( CMAKE_MODULE_LINKER_FLAGS -mno-cygwin )
endif (CYGWIN)

if (MSVC)
ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DWIN32 )
# Switch on string pooling
SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GF" )
SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GF" )
# It would be nice to enable exceptions - but Android doesn't support them, so it's better to get the warning.
else (MSVC)
SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )
endif (MSVC)

# Check if Microsoft HTML Help Workshop is installed. If it is, we will use
# it to generate .chm files in the doxygen step.
if (WIN32)
 find_package(HTMLHelp)
endif (WIN32)

# For doxygen doc generation
include(${CMAKE_ROOT}/Modules/FindDoxygen.cmake)
#SET( DOXYGEN_BASE_DIR ${SOURCE_DIR})
SET( DOXYGEN_BASE_DIR ../../.. )
SET( DOXYGEN_DOC_DIR ${DOXYGEN_BASE_DIR}/containers/doc/doxygen)
SET( DOXYGEN_SOURCE_DIR ${DOXYGEN_BASE_DIR})
configure_file(${SOURCE_DIR}/doc/doxygen/Doxyfile.in
               ${SOURCE_DIR}/doc/doxygen/Doxyfile @ONLY IMMEDIATE)
add_custom_target(doxygen ALL ${DOXYGEN} ${DOXYGEN_DOC_DIR}/Doxyfile)
set_target_properties(doxygen PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
set_target_properties(doxygen PROPERTIES EXCLUDE_FROM_ALL 1)

# Make sure the compiler can find the necessary include files
include_directories (${SOURCE_DIR}/..)

ADD_DEFINITIONS( -DLOGGING -D_DEBUG )

# Containers core library
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_io.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_io_helpers.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_codecs.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_utils.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_writer_utils.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_loader.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_filters.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_logging.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_uri.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_bits.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_list.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_index.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_clib.c)
add_library(containers_core ${core_SRCS})

# Containers io library
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_file.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_null.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_net.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_pktfile.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_http.c)
add_definitions( -DENABLE_CONTAINER_IO_HTTP )
add_library(containers_io ${io_SRCS})

# Containers net library
if (MSVC)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_common.c)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_win32.c)
elseif (LINUX)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_common.c)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_bsd.c)
else (MSVC)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_null.c)
endif (MSVC)
add_library(containers_net ${net_SRCS})

# Packetizers library
set(packetizers_core_SRCS ${packetizers_core_SRCS} ${SOURCE_DIR}/core/packetizers.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/mpga/mpga_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/mpgv/mpgv_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/h264/avc1_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/pcm/pcm_packetizer.c)
add_library(packetizers_core ${packetizers_core_SRCS} ${packetizers_SRCS})

# MP4 container
set(mp4_SRCS ${mp4_SRCS} ${SOURCE_DIR}/mp4/mp4_reader.c)
set(mp4_SRCS ${mp4_SRCS} ${SOURCE_DIR}/mp4/mp4_writer.c)
add_library(container_mp4 ${mp4_SRCS})

# MPGA container
set(mpga_SRCS ${mpga_SRCS} ${SOURCE_DIR}/mpga/mpga_reader.c)
#set(mpga_SRCS ${mpga_SRCS} ${SOURCE_DIR}/mpga/mpga_writer.c)
add_library(container_mpga ${mpga_SRCS})

# ASF container
set(asf_SRCS ${asf_SRCS} ${SOURCE_DIR}/asf/asf_reader.c)
set(asf_SRCS ${asf_SRCS} ${SOURCE_DIR}/asf/asf_writer.c)
add_library(container_asf ${asf_SRCS})

# AVI container
set(avi_SRCS ${avi_SRCS} ${SOURCE_DIR}/avi/avi_reader.c)
set(avi_SRCS ${avi_SRCS} ${SOURCE_DIR}/avi/avi_writer.c)
add_library(container_avi ${avi_SRCS})

# MKV container
set(mkv_SRCS ${mkv_SRCS} ${SOURCE_DIR}/mkv/matroska_reader.c)
#set(mkv_SRCS ${mkv_SRCS} ${SOURCE_DIR}/mkv/matroska_writer.c)
add_library(container_mkv ${mkv_SRCS})

# WAV container
set(wav_SRCS ${wav_SRCS} ${SOURCE_DIR}/wav/wav_reader.c)
#set(wav_SRCS ${wav_SRCS} ${SOURCE_DIR}/wav/wav_writer.c)
add_library(container_wav ${wav_SRCS})

# FLV container
set(flv_SRCS ${flv_SRCS} ${SOURCE_DIR}/flash/flv_reader.c)
add_library(container_flv ${flv_SRCS})

# RCV container
set(rcv_SRCS ${rcv_SRCS} ${SOURCE_DIR}/rcv/rcv_reader.c)
add_library(container_rcv ${rcv_SRCS})

# RV9 container
set(rv9_SRCS ${rv9_SRCS} ${SOURCE_DIR}/rv9/rv9_reader.c)
add_library(container_rv9 ${rv9_SRCS})

# QSYNTH container
set(qsynth_SRCS ${qsynth_SRCS} ${SOURCE_DIR}/qsynth/qsynth_reader.c)
add_library(container_qsynth ${qsynth_SRCS})

# RTP container
set(rtp_SRCS ${rtp_SRCS} ${SOURCE_DIR}/rtp/rtp_reader.c)
set(rtp_SRCS ${rtp_SRCS} ${SOURCE_DIR}/rtp/rtp_h264.c)
set(rtp_SRCS ${rtp_SRCS} ${SOURCE_DIR}/rtp/rtp_mpeg4.c)
set(rtp_SRCS ${rtp_SRCS} ${SOURCE_DIR}/rtp/rtp_base64.c)
add_library(container_rtp ${rtp_SRCS})

# RTSP container
set(rtsp_SRCS ${rtsp_SRCS} ${SOURCE_DIR}/rtsp/rtsp_reader.c)
add_library(container_rtsp ${rtsp_SRCS})

# Binary container
set(binary_SRCS ${binary_SRCS} ${SOURCE_DIR}/binary/binary_reader.c)
set(binary_SRCS ${binary_SRCS} ${SOURCE_DIR}/binary/binary_writer.c)
add_library(container_binary ${binary_SRCS})

# MPEG container
set(mpeg_SRCS ${mpeg_SRCS} ${SOURCE_DIR}/mpeg/ps_reader.c)
add_library(container_mpeg ${mpeg_SRCS})

# Dummy container
set(dummy_SRCS ${dummy_SRCS} ${SOURCE_DIR}/dummy/dummy_writer.c)
add_library(container_dummy ${dummy_SRCS})

# Simple container
set(simple_SRCS ${simple_SRCS} ${SOURCE_DIR}/simple/simple_reader.c)
set(simple_SRCS ${simple_SRCS} ${SOURCE_DIR}/simple/simple_writer.c)
add_library(container_simple ${simple_SRCS})

# Raw video container
set(raw_SRCS ${raw_SRCS} ${SOURCE_DIR}/raw/raw_video_reader.c)
set(raw_SRCS ${raw_SRCS} ${SOURCE_DIR}/raw/raw_video_writer.c)
add_library(container_raw ${raw_SRCS})

# ID3 metadata container
set(id3_metadata_SRCS ${id3_metadata_SRCS} ${SOURCE_DIR}/metadata/id3/id3_metadata_reader.c)
#set(id3_metadata_SRCS ${id3_metadata_SRCS} ${SOURCE_DIR}/metadata/id3/id3_metadata_writer.c)
add_library(container_metadata_id3 ${id3_metadata_SRCS})

# Generate test application
add_executable(test test.c)
set(test_LIBS containers_core)
set(test_LIBS ${test_LIBS} container_mp4)
set(test_LIBS ${test_LIBS} container_mpga)
set(test_LIBS ${test_LIBS} container_asf)
set(test_LIBS ${test_LIBS} container_avi)
set(test_LIBS ${test_LIBS} container_mkv)
set(test_LIBS ${test_LIBS} container_wav)
set(test_LIBS ${test_LIBS} container_flv)
set(test_LIBS ${test_LIBS} container_mpeg)
set(test_LIBS ${test_LIBS} container_rcv)
set(test_LIBS ${test_LIBS} container_rv9)
set(test_LIBS ${test_LIBS} container_qsynth)
set(test_LIBS ${test_LIBS} container_rtp)
set(test_LIBS ${test_LIBS} container_rtsp)
set(test_LIBS ${test_LIBS} container_binary)
set(test_LIBS ${test_LIBS} container_dummy)
set(test_LIBS ${test_LIBS} container_simple)
set(test_LIBS ${test_LIBS} container_raw)
set(test_LIBS ${test_LIBS} container_metadata_id3)
set(test_LIBS ${test_LIBS} containers_io)
set(test_LIBS ${test_LIBS} containers_net)
set(test_LIBS ${test_LIBS} packetizers_core)
set(test_LIBS ${test_LIBS} containers_core)
if (LINUX)
target_link_libraries(test -Wl,--whole-archive packetizers_core -Wl,--no-whole-archive ${test_LIBS} )
else (LINUX)
target_link_libraries(test ${test_LIBS} )
endif (LINUX)
install(TARGETS test DESTINATION bin)

# Generate autotest application
add_executable(autotest ${SOURCE_DIR}/autotest/autotest.cpp ${SOURCE_DIR}/autotest/crc_32.c)
if (LINUX)
target_link_libraries(autotest -Wl,--whole-archive packetizers_core -Wl,--no-whole-archive ${test_LIBS} )
else (LINUX)
target_link_libraries(autotest ${test_LIBS} )
endif (LINUX)
install(TARGETS autotest DESTINATION bin)

# Helper code to provide non-blocking console input
if (WIN32)
set( NB_IO_SOURCE nb_io_win32.c )
elseif (UNIX)
set( NB_IO_SOURCE nb_io_unix.c )
endif (WIN32)

# Generate net test applications
add_executable(stream_client stream_client.c ${NB_IO_SOURCE})
target_link_libraries(stream_client containers_core containers_io containers_net containers_core)
install(TARGETS stream_client DESTINATION bin)

add_executable(stream_server stream_server.c)
target_link_libraries(stream_server containers_net containers_core)
install(TARGETS stream_server DESTINATION bin)

add_executable(datagram_sender datagram_sender.c)
target_link_libraries(datagram_sender containers_net containers_core)
install(TARGETS datagram_sender DESTINATION bin)

add_executable(datagram_receiver datagram_receiver.c)
target_link_libraries(datagram_receiver containers_net containers_core)
install(TARGETS datagram_receiver DESTINATION bin)

add_executable(rtp_decoder rtp_decoder.c ${NB_IO_SOURCE})
target_link_libraries(rtp_decoder containers_core containers_io containers_net containers_core)
install(TARGETS rtp_decoder DESTINATION bin)

# Generate URI test application
add_executable(test_uri test_uri.c)
target_link_libraries(test_uri containers_core)
install(TARGETS test_uri DESTINATION bin)

# Generate URI pipe application
add_executable(uri_pipe uri_pipe.c ${NB_IO_SOURCE})
target_link_libraries(uri_pipe containers_core containers_io containers_net containers_core)
install(TARGETS uri_pipe DESTINATION bin)

# Generate bit stream test application
add_executable(test_bits test_bits.c)
target_link_libraries(test_bits containers_core)
install(TARGETS test_bits DESTINATION bin)

# Generate packet file dump application
add_executable(dump_pktfile dump_pktfile.c)
install(TARGETS dump_pktfile DESTINATION bin)
