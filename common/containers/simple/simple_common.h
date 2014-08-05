/*=============================================================================
Copyright (c) 2013 Broadcom Europe Limited.
All rights reserved.

FILE DESCRIPTION
Common definitions between simple reader and writer.

This simple format consists of a main metadata file which references one or
more elementary stream files.
The metadata file starts with a header describing the elementary streams
which are available. This header has the following form:

S1MPL3
TRACK video, h264, 1920, 1080
URI elementary_stream.h264
TRACK audio, mp4a, 2, 44100, 0, 0
URI elementary_stream.mp4a
3LPM1S

The first field after the track identifier is the type of stream (video, audio,
subpicture), followed by the fourcc of the codec.
For video streams, this is followed by the width and height.
For audio streams, this is followed by the number of channels, sample rate,
bits per sample and block alignment.

Following the header, each line represents a packet of data in the form:
<track_num> <size> <pts> <flags>

=============================================================================*/
#ifndef SIMPLE_COMMON_H
#define SIMPLE_COMMON_H

#define SIGNATURE_STRING "S1MPL3"
#define SIGNATURE_END_STRING "3LPM1S"

/** List of configuration options supported in the header */
#define CONFIG_VARIANT                  "VARIANT"
#define CONFIG_URI                      "URI"
#define CONFIG_CODEC_VARIANT            "CODEC_VARIANT"
#define CONFIG_BITRATE                  "BITRATE"
#define CONFIG_UNFRAMED                 "UNFRAMED"
#define CONFIG_VIDEO_CROP               "VIDEO_CROP"
#define CONFIG_VIDEO_ASPECT             "VIDEO_ASPECT"

#endif /* SIMPLE_COMMON_H */

