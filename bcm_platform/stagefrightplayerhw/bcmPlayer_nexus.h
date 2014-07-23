/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: bcmPlayer_nexus.h $
 * 
 *****************************************************************************/
#ifndef BCM_PLAYER_NEXUS_H
#define BCM_PLAYER_NEXUS_H

#ifdef ANDROID_SUPPORTS_NXCLIENT
#define MAX_NEXUS_PLAYER 3
#else
#define MAX_NEXUS_PLAYER 2
#endif

/* Nexus API used by player */
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_base_mmap.h"
#include "nexus_core_utils.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_input.h"
#include "nexus_audio_output.h"
#include "nexus_playback.h"
#include "nexus_parser_band.h"
#if NEXUS_NUM_HDDVI_INPUTS
#include "nexus_hddvi_input.h"
#endif 
#include "nexus_file.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "nexus_timebase.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_video_decoder_server.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_simple_audio_decoder_server.h"
#include "nexus_ipc_client_base.h"
#include "b_playback_ip_lib.h"

#include "gralloc_priv.h"

#include <pthread.h>

#if NEXUS_HAS_SYNC_CHANNEL
#include "nexus_sync_channel.h"
#endif

typedef enum{
    eBcmPlayerStateEnd = 0,
    eBcmPlayerStateIdle,
    eBcmPlayerStateInitialising,
    eBcmPlayerStateInitialised,
    eBcmPlayerStatePrepared,
    eBcmPlayerStateStarted,
    eBcmPlayerStateStopped,
    eBcmPlayerStatePaused,
    eBcmPlayerStatePlaybackComplete,
    eBcmPlayerStateError,
} bcmPlayerState;

// IP source specific items
typedef struct BcmPlayerIpHandle {
    B_PlaybackIpHandle          playbackIp;
    B_PlaybackIpProtocol        protocol;
    B_PlaybackIpPsiInfo         psi;
    NEXUS_Timebase              timebase;
    bool                        usePlaypump;
    bool                        liveMode;
    bool                        endOfStream;
    bool                        sessionOpened;
    uint32_t                    prevPts;
    uint32_t                    preChargeTime;
    bool                        firstBuff;
    bool                        monitoringBuffer;
    bool                        runMonitoringThread;
    void                        *sslCtx;
} BcmPlayerIpHandle;

typedef struct bcmPlayer_base_nexus_handle {
    int                          iPlayerIndex;
    NEXUS_PlaypumpHandle         playpump;
    NEXUS_PlaypumpHandle         audioPlaypump; 
    NEXUS_PlaybackHandle         playback;
    NEXUS_VideoWindowHandle     video_window;
    NEXUS_SimpleVideoDecoderHandle     simpleVideoDecoder;
    NEXUS_SimpleAudioDecoderHandle     simpleAudioDecoder;
    NEXUS_SimpleStcChannelHandle       simpleStcChannel;
    NEXUS_PidChannelHandle      videoPidChannel;
    NEXUS_PidChannelHandle      pcrPidChannel;
    NEXUS_PidChannelHandle      enhancementVideoPidChannel;
    NEXUS_PidChannelHandle      audioPidChannel;
    NEXUS_FilePlayHandle        file;
    NEXUS_FilePlayHandle        custom_file;
#if NEXUS_NUM_HDDVI_INPUTS    
    NEXUS_HdDviInputHandle      hddvi;
#endif 
#if NEXUS_NUM_HDMI_INPUTS
    NEXUS_HdmiInputHandle       hdmiInput;
#endif 
    NEXUS_SimpleAudioPlaybackHandle simpleAudioPlayback;
    BcmPlayerIpHandle           *ipHandle;
    NexusClientContext          *nexus_client;
    NexusIPCClientBase          *ipcclient;
    PSHARED_DATA                mSharedData;
    char                        *url;
    char                        *extraHeaders;
    int                         videoTrackIndex;
    int                         audioTrackIndex;
    bool                        bKeepLooping;
    bool                        bSupportsHEVC;
    NEXUS_VideoFormat           maxVideoFormat;
    int                         seekPositionMs;
    bcmPlayerState              state;
} bcmPlayer_base_nexus_handle_t;

#endif

