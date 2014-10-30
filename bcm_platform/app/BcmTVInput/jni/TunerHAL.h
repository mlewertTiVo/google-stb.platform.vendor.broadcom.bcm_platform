#ifndef TUNER_HAL_H
#define TUNER_HAL_H

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "nxclient.h"
#include "nexus_frontend.h"
#include "nexus_parser_band.h"
#include "nexus_simple_video_decoder.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int Broadcast_Initialize();
static int Broadcast_Tune(int);
static int Broadcast_Stop();

typedef struct _tuner_data 
{
    int freq;
    NexusIPCClientBase *ipcclient;
    NexusClientContext *nexus_client;
    NEXUS_FrontendHandle frontend;
    NEXUS_ParserBand parserBand;
    NEXUS_SimpleStcChannelHandle stcChannel;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
    NEXUS_PidChannelHandle pidChannel;
}Tuner_Data, *PTuner_Data;

// Some tuner values
#define VIDEO_PID_1     0x100
#define VIDEO_PID_2     0x200

#endif
