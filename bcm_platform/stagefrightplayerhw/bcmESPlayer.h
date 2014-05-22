/******************************************************************************
 *    (c)2010-2012 Broadcom Corporation
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
 * $brcm_Workfile: bcmESPlayer.h $
 * $brcm_Revision: 5 $
 * $brcm_Date: 12/3/12 3:26p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmESPlayer.h $
 * 
 * 5   12/3/12 3:26p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 4   6/5/12 2:39p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 3   2/24/12 4:10p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 2   2/8/12 2:53p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 7   10/4/11 7:10p hvasudev
 * SW7425-1318: Added SetVolume support (currently not being used though)
 * 
 * 6   9/30/11 1:59p hvasudev
 * SW7425-1318: Corrected revision comments within the file
 * 
 * 5   9/26/11 3:05p hvasudev
 * SW7425-1318: Added audio related changes
 * 
 * 4   9/23/11 1:56p hvasudev
 * SW7425-1318: Now using playerid in the Nexus calls (as needed)
 * 
 * 3   9/22/11 4:00p hvasudev
 * SW7425-1318: Removing some globals
 *
 * 2   9/22/11 2:35p hvasudev
 * SW7425-1318: Renamed the class to BCMESPlayer & also modified the debug prints
 * 
 * 1   9/19/11 11:47p hvasudev
 * SW7425-1318: flash player related changes
 * 
 *****************************************************************************/
#if defined(FEATURE_GPU_ACCEL_H264_BRCM)
#define BSTD_CPU_ENDIAN BSTD_ENDIAN_LITTLE

#include "bstd.h"
#include "nexus_platform.h"
#include "nexus_platform_client.h"
#include "nexus_core_utils.h"
#include "nexus_video_decoder_trick.h"
#include "nexus_audio_decoder_trick.h"
#include "nexus_video_decoder_extra.h"
#include "bioatom.h"
#include "bmedia_util.h"
#include "nexus_display.h"
#include "nexus_playback.h"
#include "nexus_simple_audio_decoder.h"

#include <stdio.h>
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include"nexus_audio_mixer.h"
#include "nexus_ipc_client_factory.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif

#define CALCULATE_PTS(t)			(uint64_t(t) * 45LL)
enum AudioType
{
    kAudioTypeNone,
    kAudioTypeMP3,              //! MP3
    kAudioTypeAAC,              //! AAC
    kAudioTypeAC3,              //! AC3 aka Dolby Digital
    kAudioTypeMP1L2,            //! MPEG 1 layer 2
    kAudioTypePCM               //! PCM
};

enum VideoType
{
    kVideoTypeNone,
    kVideoTypeH264,             //! H.264 aka AVC aka MPEG4-Part10
    kVideoTypeVC1,              //! VC1 aka WMV
    kVideoTypeMPEG2,            //! MPEG 2
    kVideoTypeMPEG4,            //! MPEG 4 Part 1
    kVideoTypeSorenson,         //! Sorenson
    kVideoTypeOn2VP6            //! VP 6
};

enum State
{
    kUndefinedState,
    kPlaying,
    kPaused,
    kStopped,
	kInitialized,
    kRecoveringFromError
};

// Stream Player Resource
typedef struct BCMES_CONFIG
{
	NEXUS_VideoCodec videoCodec;
	NEXUS_AudioCodec audioCodec;
	uint16_t audioPid;
	uint16_t videoPid;
	uint16_t numPlaypumps;
	NEXUS_TransportType transportType;
	NEXUS_CallbackDesc videoDataCallback;            
	NEXUS_CallbackDesc audioDataCallback;
	NEXUS_KeySlotHandle decryptContext;
	NEXUS_CallbackDesc videoSourceCallback; 
	NEXUS_CallbackDesc fifoEmptyCallback;	
	void *stageWindow;
} BCMES_Config;

typedef struct BCMES_RESOURCES
{
    BCMES_Config							config;
    NEXUS_StcChannelHandle                  stcChannel;
    NEXUS_SimpleVideoDecoderHandle          simpleVideoDecoder;
    NEXUS_SimpleAudioDecoderHandle          simpleAudioDecoder;
    NEXUS_VideoWindowHandle                 videoWindow;
    NEXUS_PlaypumpHandle                    playpump;//[2];
    NEXUS_PidChannelHandle                  videoPidChannel;
    NEXUS_PidChannelHandle                  audioPidChannel;
	NEXUS_PlaybackHandle					playback;
    void                                    (*audioStatusUpdate) (const struct BcmNexus_StreamPlayer_Resources *resources, bool started);
	NEXUS_Error								(*setVideoRegion) (const struct BcmNexus_StreamPlayer_Resources *resources, NEXUS_Rect *planeRect, NEXUS_Rect *rect, NEXUS_Rect *clipRect);
   // NEXUS_DmaHandle                         dmaHandle;
    uint32_t                                scaleX;
    uint32_t                                scaleY;
	void									*stageWindow;
	NEXUS_DisplayHandle                     display;
} BCMES_Resources;

class BCMESPlayer
{
public:
    BCMESPlayer(int id, VideoType videoType, AudioType audioType);
	bool Init();
    ~BCMESPlayer();

    bool SetVideoRegion(int32_t x, int32_t y, int32_t w, int32_t h);

    bool SendAudioES(uint32_t time,  uint8_t* pBuffer, int bufferSize);
    bool SendVideoES(uint32_t time,  uint8_t* pBuffer, int bufferSize);
    uint32_t GetCurrentPTS();
    bool Flush();
    bool Pause();
    bool Play();
	bool Play(uint32_t decodeToTime,bool pauseAtDecodeTime);
    bool Stop(bool videoPaused);
	State GetPlayState();

	void VideoStart();

    bool NotifyEOF();
	bool SetAudioCodec( AudioType audType);
	bool CanAcceptES(uint32_t time);
	void SetVolume(int volume);

private:
	bool sendData();
	void processAudioPayload( uint32_t pts,  uint8_t * buffer, size_t size);
	void processVideoPayload( uint32_t pts, uint8_t * buffer, size_t size);
	void startPesPacket( uint32_t pts, uint8_t id, size_t packet_size);
    unsigned GetFifoMarker(unsigned &cdbDepth) ;
    uint32_t GetCurrentPTSLocked(bool &decoderTime) ;
    void updatePlayTimeLocked();
    bool StartPlayLocked( uint32_t seekTo,bool lockFlag);
    bool PauseLocked();
    void resetPlayState();

	void dataCallback();
	static void staticDataCallback(void *context, int param);
    void videoSourceCallback();
	static void staticVideoSourceCallback (void *context, int param);

    void videoDecoderFirstPts();
    static void staticVideoDecoderFirstPts(void *context, int param);
    void videoDecoderFirstPtsPassed();
    static void staticVideoDecoderFirstPtsPassed(void *context, int param);

	void videoDecoderPictureReady();
	static void staticVideoDecoderPictureReady(void *context, int param);

private:
    bool fifo_full;
    bool stopped;
	State PlayState;
    bool paused;
    bool waitingEos;
    bool videoPaused;
    bool waitingPts;
    bool stopThread;
    bool playTimeUpdated; /* set to false when host starts playback and sets to true if playTime updated based on the decoder's PTS */
    bool decodeTimeUpdated; /* set to false when host starts playback aand sets to true if playTime updated based on the data PTS */
    bool inAccurateSeek;
    bool pacingAudio;
    uint32_t targetPts; /* timestamp we are waiting for */
	batom_factory_t factory;
	BCMES_Resources *resources;
	uint32_t pts_hi;
    size_t fifo_threshold_full; /* number of empty bytes that is used to trigger 'bufferFull' state */
    size_t fifo_threshold_low; /* number of empty bytes that is used to trigger 'bufferFull' state */    
	batom_accum_t accum;
	batom_cursor acc_cursor;
	uint8_t pes_header[64];
    NEXUS_Rect planeRect;
    uint32_t fifoMarker; /* some marker which shows activity when decoding. currently using video PTS */
    uint32_t fifoLast; /* last time when fifoMarker has changed */
    uint32_t playTime; /* last known good position */
    uint32_t prevPlayTime; /* previous last known good position, it's returned by public API while in the accurate seek state */
    uint32_t lastDecodeVideoTime; /* last PTS send to the decoder */
	unsigned long audioFifoSize;
	unsigned long videoFifoSize;
	unsigned long audioMinLevel;
	unsigned long audioMaxLevel;
	unsigned long videoMinLevel;
	unsigned long videoMaxLevel;
	AudioType m_audioType;
	VideoType m_videoType;

	pthread_mutex_t BRCM_Mutex;
	int m_id;	// player id passed on from NexusPlayer

    NexusClientContext *nexus_client;
    NexusIPCClientBase *ipcclient;

private:
    void ThreadProc();
};

extern "C"
{
void BRCMProxy_Create(int id);
bool BRCMProxy_Init();
void BRCMProxy_Destroy();
bool BRCMProxy_CanAcceptVidES(uint32_t);
bool BRCMProxy_SendVideoES(uint32_t, uint8_t*, int);
void BRCMProxy_VideoStart();
bool BRCMProxy_Flush();
bool BRCMProxy_Play();
bool BRCMProxy_Pause();
bool BRCMProxy_SetVideoRegion(int32_t, int32_t, int32_t, int32_t);
bool BRCMProxy_SendAudioES(uint32_t, uint8_t*, int);
bool BRCMProxy_CanAcceptAudES(uint32_t);
bool BRCMProxy_SetAudioCodec(int);
void BRCMProxy_SetVolume(int);
void BRCMProxy_SetSeek();
unsigned long long BRCMProxy_GetCurrentPTS();
}
#endif