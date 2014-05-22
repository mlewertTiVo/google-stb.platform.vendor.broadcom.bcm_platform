/***************************************************************************
 *     (c)2008-2013 Broadcom Corporation
 *  
 *  This program is the proprietary software of Broadcom Corporation and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to the terms and
 *  conditions of a separate, written license agreement executed between you and Broadcom
 *  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 *  no license (express or implied), right to use, or waiver of any kind with respect to the
 *  Software, and Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 *  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 *  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *   
 *  Except as expressly set forth in the Authorized License,
 *   
 *  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 *  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 *  and to use this information only in connection with your use of Broadcom integrated circuit products.
 *   
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 *  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 *  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 *  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 *  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 *  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 *  USE OR PERFORMANCE OF THE SOFTWARE.
 *  
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 *  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 *  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 *  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 *  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 *  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 *  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 *  ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: bcmESPlayer.cpp $
 * $brcm_Revision: 15 $
 * $brcm_Date: 12/14/12 2:29p $
 * 
 * Module Description:
 * 
 * Revision History:
 *
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmESPlayer.cpp $
 * 
 * 15   12/14/12 2:29p kagrawal
 * SWANDROID-277: Wrapper IPC APIs for
 *  NEXUS_SimpleXXX_Acquire()/_Release()
 * 
 * 14   12/3/12 3:25p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 13   9/13/12 11:32a kagrawal
 * SWANDROID-104: Added support for dynamic display resolution change,
 *  1080p and screen resizing
 * 
 * 12   7/30/12 4:08p kagrawal
 * SWANDROID-104: Support for composite output
 * 
 * 11   6/20/12 6:00p kagrawal
 * SWANDROID-118: Extended get_output_format() to return width and height
 * 
 * 10   6/5/12 2:38p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 9   5/7/12 3:54p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 8   4/13/12 3:06p franktcc
 * SWANDROID-12: Fixed compiling error when enable nexus client/server
 *  support
 * 
 * 7   3/26/12 4:27p hvasudev
 * SW7425-1953: We are now required to provide heap information while
 *  calling NEXUS_Playpump_Open. With this fix FlashPlayer no longer
 *  switches to software mode playback
 * 
 * 6   2/24/12 4:10p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 5   2/8/12 2:53p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 4   12/21/11 4:29p hvasudev
 * SW7425-1953: We now close the audio & video PID channels (this resolves
 *  the NEXUS crash/assert in NEXUS_Playpump_ClosePidChannel)
 * 
 * 3   12/20/11 3:27p hvasudev
 * SW7425-1953: Making sure we don't call the destructor if the cleanup
 *  has already been done eariler
 * 
 * 2   12/14/11 12:37p hvasudev
 * SW7425-1953: Making sure we don't use invalid values before calling
 *  NEXUS_VideoWindow_SetSettings
 * 
 * 15   11/3/11 4:26p hvasudev
 * SW7425-1318: Checking out-of-bounds values & resetting them
 * 
 * 14   10/20/11 1:01p hvasudev
 * SW7425-1318: Re-ordered the sequence of NEXUS calls in the destructor
 * 
 * 13   10/4/11 7:10p hvasudev
 * SW7425-1318: Added SetVolume support (currently not being used though)
 * 
 * 12   10/4/11 3:12p hvasudev
 * SW7425-1318: Removing the hack (during init time) to enable audio . We
 *  now unmute the audio when we get the first video PTS
 * 
 * 11   9/30/11 4:11p hvasudev
 * SW7425-1318: Adding the brcm_Log tag
 *
 * 10   9/30/11 1:58p hvasudev
 * SW7425-1318: Correcting revision history within the file
 *
 * 9   9/27/11 5:46p hvasudev
 * SW7425-1318: Updated video window settings
 *
 * 8   9/26/11 3:05p hvasudev
 * SW7425-1318: Added audio related changes
 *
 * 7   9/23/11 6:21p hvasudev
 * SW7425-1318: Moved some audio related code under the define FEATURE_AUDIO_HWCODEC
 *
 * 6   9/23/11 3:03p hvasudev
 * SW7425-1318: Adding the player ID as the 1st arg
 *
 * 5   9/23/11 1:56p hvasudev
 * SW7425-1318: Now using playerid in the Nexus calls (as needed)
 *
 * 4   9/23/11 10:01a hvasudev
 * SW7425-1318: Added fallback to NEXUS_Platform_Join only as needed
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
 ***************************************************************************/

// For calling NEXUS APIs
#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_input.h"
#include "nexus_audio_output.h"
#include "nexus_playback.h"
#include "nexus_parser_band.h"
#include "nexus_file.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "nexus_timebase.h"

#include "bcmESPlayer.h"
#include <cutils/log.h>

#if defined(FEATURE_GPU_ACCEL_H264_BRCM)

#define BCMNEXUS_STREAM_CAPTURE_name "stream%u.es"

static BCMESPlayer *g_Private;

#define BCM_MUTEX_INIT()\
{\
	int rc = pthread_mutex_init(&BRCM_Mutex, NULL);\
\
	if (rc)\
		LOGE("BCMESPlayer: Failed to create BRCM_Mutex!! rc = %d", rc);\
}

#define BCM_MUTEX_DESTROY()\
{\
	int rc = pthread_mutex_destroy(&BRCM_Mutex);\
\
	if (rc)\
		LOGE("BCMESPlayer: Failed to destroy BRCM_Mutex!! rc = %d", rc);\
}

#define BCM_MUTEX_LOCK()\
{\
	pthread_mutex_lock(&BRCM_Mutex);\
}

#define BCM_MUTEX_UNLOCK()\
{\
	pthread_mutex_unlock(&BRCM_Mutex);\
}

class BpNexusClient: public android::BpInterface<INexusClient>
{
public:
        BpNexusClient(const android::sp<android::IBinder>& impl)
                : android::BpInterface<INexusClient>(impl)
        {
        }

        void NexusHandles(NEXUS_TRANSACT_ID eTransactId, int32_t *pHandle)
        {

                android::Parcel data, reply;
                data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
                                remote()->transact(eTransactId, data, &reply);
                                *pHandle = reply.readInt32();
        }

};

BCMESPlayer::BCMESPlayer(int id, VideoType videoType, AudioType audioType)
    : fifo_full(false),
    stopped(true),
	PlayState(kUndefinedState),
    paused(false),
    waitingEos(false),
    videoPaused(false),
    waitingPts(false),
    stopThread(false),
    playTimeUpdated(false),
    decodeTimeUpdated(false),
    inAccurateSeek(false),
    pacingAudio(false),
	factory(NULL),
	resources(NULL),
	pts_hi(0),
    fifo_threshold_full(0),
    fifo_threshold_low(0)
{
	NEXUS_Error				rc;
	NEXUS_PlatformStatus	pStatus;

	// First off, store the id
	m_id = id;

    planeRect.x = planeRect.y = 0;
    planeRect.width = planeRect.height = 0;    
	videoFifoSize = audioFifoSize = 0;
	m_videoType = videoType;
	m_audioType = audioType;

	// Allocate memory for the resources structure
	resources = (BCMES_Resources *)malloc(sizeof(BCMES_Resources));
	LOGE("BCMESPlayer::BCMESPlayer: resources = 0x%x", (unsigned int)resources);

	// Create the mutex
	BCM_MUTEX_INIT();
    ipcclient = NexusIPCClientFactory::getClient("BCMESPlayer");
    nexus_client = NULL;
    
    b_refsw_client_client_configuration config;
    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),"BCMESPlayer");
    config.resources.screen.required = false;
    config.resources.audioDecoder = true;
    config.resources.audioPlayback = true;            
    config.resources.videoDecoder = true;
    nexus_client = ipcclient->createClientContext(&config);
    if (nexus_client == NULL) {
        LOGE("%s: Could not create client \"%s\" context!!!", __FUNCTION__, ipcclient->getClientName());
	}
	else {
		LOGI("BCMESPlayer::BCMESPlayer: NEXUS_Platform_GetStatus succeeded!!");
    }
	return;
}

bool BCMESPlayer::Init()
{
	LOGE("BCMESPlayer::Init: Enter...");
	BCM_MUTEX_LOCK();
    NEXUS_PlaypumpOpenSettings  playpumpOpenSettings;
    NEXUS_ClientConfiguration   clientConfig;
	NEXUS_Error rc = 0;	
	
	BKNI_Memset(&resources->config, 0x0, sizeof(BCMES_Config));
	resources->config.numPlaypumps = 1;

#ifdef FEATURE_AUDIO_HWCODEC
	resources->config.audioCodec = NEXUS_AudioCodec_eAac;
#endif

	resources->config.audioPid = 0xC0;
	resources->config.videoCodec = NEXUS_VideoCodec_eH264;
	resources->config.videoPid = 0xE0;
    NEXUS_CALLBACKDESC_INIT(&resources->config.videoDataCallback);

	resources->config.videoDataCallback.context = this;
	resources->config.videoDataCallback.callback = staticDataCallback;
    NEXUS_CALLBACKDESC_INIT(&resources->config.videoSourceCallback);

    resources->config.videoSourceCallback.callback = staticVideoSourceCallback;
	resources->config.videoSourceCallback.context = this;

//	resources->setVideoRegion = BcmNexus_SetVideoRegion;
//	resources = BcmNexus_StreamPlayer_Resources_Acquire(&config);
	//////////////////////////////////

    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    playpumpOpenSettings.heap = clientConfig.heap[1]; /* playpump requires heap with eFull mapping */

	LOGE("BCMESPlayer::Init: Calling Nexus playpump open (id = %d)...", m_id);
	resources->playpump = NEXUS_Playpump_Open(m_id, &playpumpOpenSettings);
    if (resources->playpump == NULL)
    {
        LOGE("BCMESPlayer::Init: Nexus playpump open failed!!");
		BCM_MUTEX_UNLOCK();
		return false;
    }
	else
		LOGE("BCMESPlayer::Init: Nexus playpump open succeeded!!");
/*
	resources->playback = NEXUS_Playback_Create();
    if (resources->playback == NULL)
    {
        LOGE("BCMESPlayer::Init: Nexus playback create failed!!");
		BCM_MUTEX_UNLOCK();
		return false;
    }
*/

	LOGE("BCMESPlayer::Init: Calling NEXUS_SimpleVideoDecoder_Acquire...");
    resources->simpleVideoDecoder = ipcclient->acquireVideoDecoderHandle();
    if (resources->simpleVideoDecoder == NULL)
    {
        LOGE("Can not acquire simple video decoder\n");
        BCM_MUTEX_UNLOCK();
		return false;
    }
    // Note: connection to video window is done internally in nexus when creating SimpleVideoDecoder

    {
		bool bReset = false;
        uint32_t window_id = m_id; //there is a 1-to-1 mapping between video window id and iPlayerIndex

    	b_video_window_settings windowSettings;

		// Get the screen co-ordinates and dimensions
        uint32_t screen_w=0, screen_h=0;
		NEXUS_SurfaceComposition composition;
        ipcclient->getClientComposition(nexus_client, &composition);    
        screen_w = composition.position.width;
        screen_h = composition.position.height;

		// Get the window co-ordinates and dimensions
        ipcclient->getVideoWindowSettings(nexus_client, window_id, &windowSettings);    
        LOGE("BCMESPlayer::Init: Calling getVideoWindowSettings... (x=%d y=%d wxh=%dx%d)", windowSettings.position.x, windowSettings.position.y, windowSettings.position.width, windowSettings.position.height);

		// If the dimensions are going of bounds (width wise), flag it now
		if ((windowSettings.position.x + windowSettings.position.width) > screen_w)
		{
			windowSettings.position.x = 0;
			bReset = true;
			LOGE("BCMESPlayer::Init: Out-of-bounds width/x!!");
		}

		// If the dimensions are going of bounds (height wise), flag it now
		if ((windowSettings.position.y + windowSettings.position.height) > screen_h)
		{
			windowSettings.position.y = 0;
			bReset = true;
			LOGE("BCMESPlayer::Init: Out-of-bounds height/y");
		}

		// Check if we need to reset the x or y values
		if (bReset)
		{
			LOGE("BCMESPlayer::Init: Resetting window position...");
			ipcclient->setVideoWindowSettings(nexus_client, window_id, &windowSettings);
		}
        // Note: simpleVideoDecoder has been already attached to a videowindow, so no need to do that here
	}

	NEXUS_PlaypumpSettings playpumpSettings;
	NEXUS_Playpump_GetSettings(resources->playpump, &playpumpSettings);
	playpumpSettings.dataCallback = resources->config.videoDataCallback;

	//if (!config->transportType)
	//{
			playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
	//}

	rc = NEXUS_Playpump_SetSettings(resources->playpump, &playpumpSettings);
	if (rc != NEXUS_SUCCESS)
	{
		LOGE("BCMESPlayer::Init: NEXUS_Playpump_SetSettings failed!!");
		BCM_MUTEX_UNLOCK();
		return false;
	}

	resources->stcChannel = NULL;	
	NEXUS_StcChannelSettings stcSettings;
	
	// Is the arg 0 here the playerindex??
	NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto; /* playback */
    resources->stcChannel = NEXUS_StcChannel_Open(0, &stcSettings);

	if (!resources->stcChannel) 
	{ 
		LOGE("BCMESPlayer::Init: NEXUS_StcChannel_Open failed"); 
		BCM_MUTEX_UNLOCK();
		return false; 
	}
	
    resources->videoPidChannel = NULL;
//	LOGE("BCMESPlayer::Init: resources->config.videoPid = 0x%x", resources->config.videoPid);
	resources->videoPidChannel = NEXUS_Playpump_OpenPidChannel(resources->playpump, resources->config.videoPid, NULL);
	if (!resources->videoPidChannel) 
	{ 
		LOGE("BCMESPlayer::Init: NEXUS_Playpump_OpenPidChannel for video failed!!"); 
		BCM_MUTEX_UNLOCK();
		return false; 
	}

	resources->audioPidChannel = NULL;
    resources->simpleAudioDecoder = NULL;
#ifdef FEATURE_AUDIO_HWCODEC
    resources->simpleAudioDecoder = ipcclient->acquireAudioDecoderHandle();
    LOGE("BCMESPlayer::Init: simpleAudioDecoder = 0x%x", (unsigned int)resources->simpleAudioDecoder);
    if (resources->simpleAudioDecoder == NULL)        
    {            
		LOGE("BCMESPlayer::Init: NEXUS_SimpleAudioDecoder_Acquire failed!!");
		BCM_MUTEX_UNLOCK();
		return false; 
    }

	resources->audioPidChannel = NEXUS_Playpump_OpenPidChannel(resources->playpump, resources->config.audioPid, NULL);
	if (!resources->audioPidChannel) 
	{ 
		LOGE("BCMESPlayer::Init: NEXUS_Playpump_OpenPidChannel for audio failed!!"); 
		BCM_MUTEX_UNLOCK();
		return false; 
	}
#endif

//	LOGE("BCMESPlayer::Init: Calling batom_factory_create...");
	factory = batom_factory_create(bkni_alloc, 16);
	if (!factory)
	{
		factory = 0;
		LOGE("BCMESPlayer::Init: batom_factory_create failed!!");
		BCM_MUTEX_UNLOCK();
		return false;  
	}

//	LOGE("BCMESPlayer::Init: Calling batom_accum_create()");
	accum = batom_accum_create(factory);
	if (!factory) 
	{ 
		// atom_factory_destroy(factory);
		factory = 0;
		LOGE("BCMESPlayer::Init: batom_accum_create failed!!");
		BCM_MUTEX_UNLOCK();
		return false; 
	}

//#ifdef BCMNEXUS_STREAM_CAPTURE 
//  stream_count = 0;
//	stream = NULL;
//#endif

    NEXUS_PlaypumpStatus playpumpStatus;
    rc = NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);
    if (rc != NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::Init: NEXUS_Playpump_GetStatus returned an error, rc = %d", rc);
		BCM_MUTEX_UNLOCK();
		return false;
	}

    fifo_threshold_full = (3*playpumpStatus.fifoSize)/4;
    fifo_threshold_low = playpumpStatus.fifoSize - playpumpStatus.fifoSize/8;

    NEXUS_VideoDecoderPlaybackSettings playbackSettings;
    NEXUS_SimpleVideoDecoder_GetPlaybackSettings(resources->simpleVideoDecoder, &playbackSettings);
    playbackSettings.firstPts.callback = staticVideoDecoderFirstPts;
    playbackSettings.firstPts.context = this;
    playbackSettings.firstPtsPassed.callback = staticVideoDecoderFirstPtsPassed;
    playbackSettings.firstPtsPassed.context = this;
    rc = NEXUS_SimpleVideoDecoder_SetPlaybackSettings(resources->simpleVideoDecoder, &playbackSettings);
    if (rc != NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::Init: NEXUS_VideoDecoder_SetPlaybackSettings returned an error!!, rc = %d", rc);
		BCM_MUTEX_UNLOCK();
		return false;
	}

	NEXUS_VideoDecoderExtendedSettings extendedSettings;
	NEXUS_SimpleVideoDecoder_GetExtendedSettings(resources->simpleVideoDecoder, &extendedSettings);
	extendedSettings.dataReadyCallback.callback = staticVideoDecoderPictureReady;
	extendedSettings.dataReadyCallback.context = this;
	NEXUS_SimpleVideoDecoder_SetExtendedSettings(resources->simpleVideoDecoder, &extendedSettings);

	planeRect.x = 0;
    planeRect.y = 0;
    planeRect.width = 0;
    planeRect.height = 0;
    playTime = 0;
    prevPlayTime = 0;
    lastDecodeVideoTime = 0;

	LOGE("BCMESPlayer::Init: Calling StartPlayLocked...");
	StartPlayLocked(0, false);

	NEXUS_VideoDecoderStatus videoStatus;
	rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &videoStatus);
	if (rc != NEXUS_SUCCESS) 
		LOGE("BCMESPlayer::Init: NEXUS_VideoDecoder_GetStatus failed!!");

#ifdef FEATURE_AUDIO_HWCODEC
	NEXUS_AudioDecoderStatus audioStatus;
    rc = NEXUS_SimpleAudioDecoder_GetStatus (resources->simpleAudioDecoder, &audioStatus);
	if (rc != NEXUS_SUCCESS) 
		LOGE("BCMESPlayer::Init: NEXUS_AudioDecoder_GetStatus returned an error!! rc = 0x%x", rc);

	audioFifoSize = audioStatus.fifoSize;
#endif

	videoFifoSize = videoStatus.fifoSize;
	PlayState = kInitialized;
	
	LOGE("BCMESPlayer::Init: Done!!");
	BCM_MUTEX_UNLOCK();
	return true;
}

BCMESPlayer::~BCMESPlayer()
{
	LOGE("BCMESPlayer::~BCMESPlayer, Enter...");
    
#ifdef BCMNEXUS_STREAM_CAPTURE 
    if(stream) 
	{
        fclose(stream);
        stream=NULL;
    }
#endif

    if (resources == NULL)
    {
        LOGE("BCMESPlayer::~BCMESPlayer, NEXUS cleanup already done, returning...");
        return;
    }

    if (factory) 
	{
		// Adobe change - need to destroy the mutex in all cases,
		// so no early exit here please.
		LOGE("BCMESPlayer::~BCMESPlayer, NEXUS related cleanup...");

		waitingEos = false;
		if (!stopped) 
		{
			Stop(false);
		}

//		BCM_MUTEX_LOCK();
		// StopThread();
    
        NEXUS_VideoDecoderPlaybackSettings playbackSettings;

        /* clean all settings, and don't leak them outside */
        NEXUS_SimpleVideoDecoder_GetPlaybackSettings(resources->simpleVideoDecoder, &playbackSettings);
        playbackSettings.firstPts.callback = NULL;
        playbackSettings.firstPts.context = NULL;
        playbackSettings.firstPtsPassed.callback = NULL;
        playbackSettings.firstPtsPassed.context = NULL;
        NEXUS_SimpleVideoDecoder_SetPlaybackSettings(resources->simpleVideoDecoder, &playbackSettings);

		NEXUS_VideoDecoderExtendedSettings extendedSettings;
		NEXUS_SimpleVideoDecoder_GetExtendedSettings(resources->simpleVideoDecoder, &extendedSettings);
		extendedSettings.dataReadyCallback.callback = NULL;
		extendedSettings.dataReadyCallback.context = NULL;
		NEXUS_SimpleVideoDecoder_SetExtendedSettings(resources->simpleVideoDecoder, &extendedSettings);
		batom_accum_destroy(accum);
		batom_factory_destroy(factory);

//		BcmNexus_StreamPlayer_Resources_Release(resources);

		NEXUS_PlatformConfiguration platformConfig;
		NEXUS_Platform_GetConfiguration(&platformConfig);

#ifdef FEATURE_AUDIO_HWCODEC
		if (resources->simpleAudioDecoder)			
        {                
            NEXUS_SimpleAudioDecoder_Stop (resources->simpleAudioDecoder);                
            ipcclient->releaseAudioDecoderHandle(resources->simpleAudioDecoder);
        }
#endif

		if (resources->simpleVideoDecoder)
		{
			LOGE("BCMESPlayer::~BCMESPlayer: Stopping simple video decoder...");
			NEXUS_SimpleVideoDecoder_Stop(resources->simpleVideoDecoder);
    		ipcclient->releaseVideoDecoderHandle(resources->simpleVideoDecoder);
		}

		resources->videoWindow = NULL;

		if (resources->stcChannel)
    		NEXUS_StcChannel_Close(resources->stcChannel);
	
		if (resources->playpump)
		{
			LOGE("BCMESPlayer::~BCMESPlayer: Closing Nexus playpump...");
            NEXUS_Playpump_ClosePidChannel(resources->playpump, resources->audioPidChannel);
            NEXUS_Playpump_ClosePidChannel(resources->playpump, resources->videoPidChannel);
			NEXUS_Playpump_Close(resources->playpump);
			resources->playpump	= NULL;
		}

        resources->simpleVideoDecoder = NULL;
        resources->simpleAudioDecoder = NULL;
		resources->stcChannel	= NULL;
		resources->display		= NULL;

#ifdef BCMNEXUS_STREAM_CAPTURE 
		if(stream) 
		{
			fclose(stream);
			stream=NULL;
		}
#endif

//		BCM_MUTEX_UNLOCK();
    }
    
#ifdef BCMNEXUS_STREAM_CAPTURE 
    if(stream) 
	{
        fclose(stream);
        stream=NULL;
    }
#endif

	// Destroy the mutex
	BCM_MUTEX_DESTROY();
//	BRCM_Mutex = PTHREAD_MUTEX_INITIALIZER;

	if (resources)
	{
		free(resources);
		resources = NULL;
	}

    if(ipcclient)
    {
        if(nexus_client)
        {
            ipcclient->destroyClientContext(nexus_client);
            nexus_client = NULL;
        }
        delete ipcclient;        
        ipcclient = NULL;
    }

	LOGE("BCMESPlayer::~BCMESPlayer, Done!!");
	return;
}

bool BCMESPlayer::SetVideoRegion(int32_t x, int32_t y, int32_t w, int32_t h)
{    
    NEXUS_VideoWindowSettings inputSettings;
    NEXUS_DisplaySettings     displaySettings;
    unsigned                  displayWidth = 1280;
    unsigned                  displayHeight = 720;
    NEXUS_Error               rc = NEXUS_SUCCESS;
    NEXUS_VideoWindowSettings nWindowSettings;
    b_video_window_settings   windowSettings;
    uint32_t                  window_id = m_id; //note there is a 1-to-1 mapping between playerid and window_id


	LOGE("BCMESPlayer::SetVideoRegion: x = %d y = %d wxh = %dx%d", x, y, w, h);

	if (resources->display)
	{
		ipcclient->getDisplaySettings(0, &displaySettings);	
		if (displaySettings.format == NEXUS_VideoFormat_e720p) 
		{
			LOGE("BCMESPlayer::SetVideoRegion: NEXUS_VideoFormat_e720p");
			displayWidth = 1280;
			displayHeight = 720;
		}

		else if(displaySettings.format == NEXUS_VideoFormat_e1080p) 
		{
			LOGE("BCMESPlayer::SetVideoRegion: NEXUS_VideoFormat_e1080p");
			displayWidth = 1920;
			displayHeight = 1080;
		}

		else if(displaySettings.format == NEXUS_VideoFormat_eNtsc) 
		{
			LOGE("BCMESPlayer::SetVideoRegion: NEXUS_VideoFormat_eNtsc");
			displayWidth = 720;
			displayHeight = 480;
		}

		NEXUS_CalculateVideoWindowPositionSettings pos;
		NEXUS_GetDefaultCalculateVideoWindowPositionSettings(&pos);

		pos.viewport.x = (x);
		pos.viewport.y = (y);
		pos.viewport.width = (w);
		pos.viewport.height = (h);
		pos.displayWidth = displayWidth;
		pos.displayHeight = displayHeight;
    	ipcclient->getVideoWindowSettings(nexus_client, m_id, &windowSettings);
        nWindowSettings.position    = windowSettings.position;
        nWindowSettings.clipRect    = windowSettings.clipRect;
        nWindowSettings.visible     = windowSettings.visible;
        nWindowSettings.contentMode = windowSettings.contentMode;
        nWindowSettings.autoMaster  = windowSettings.autoMaster;
		memcpy(&inputSettings, &nWindowSettings, sizeof(NEXUS_VideoWindowSettings));
		nWindowSettings.visible = true;
		nWindowSettings.autoMaster = true;

		NEXUS_CalculateVideoWindowPosition(&pos, &nWindowSettings, &nWindowSettings);

		// Need to reset the clipRect, else the entire desktop will go black and stay
		// so for over 12 seconds. This will take out the clipping feature for now
//		windowSettings.clipRect		= inputSettings.clipRect;
		nWindowSettings.clipRect	= nWindowSettings.clipBase;

		// Make sure we have the valid positions before calling
		if ((nWindowSettings.position.width < 2) || (nWindowSettings.position.height < 1))
		{
			//windowSettings.position.width = 2;
			LOGE("BCMESPlayer::SetVideoRegion: Invalid dimensions, returning...");
			return true;
		}

		if ((inputSettings.position.x != nWindowSettings.position.x) || (inputSettings.position.y != nWindowSettings.position.y) || (inputSettings.position.width != nWindowSettings.position.width) || 
			(inputSettings.position.height != nWindowSettings.position.height))
		{
//			LOGE("BCMESPlayer::SetVideoRegion (i/p values): x = %d y = %d wxh = %dx%d", inputSettings.position.x, inputSettings.position.y, inputSettings.position.width, inputSettings.position.height);
			LOGE("BCMESPlayer::SetVideoRegion (new values): x = %d y = %d wxh = %dx%d", nWindowSettings.position.x, nWindowSettings.position.y, nWindowSettings.position.width, nWindowSettings.position.height);
            windowSettings.position    = nWindowSettings.position;
            windowSettings.clipRect    = nWindowSettings.clipRect;
            windowSettings.visible     = nWindowSettings.visible;
            windowSettings.contentMode = nWindowSettings.contentMode;
            windowSettings.autoMaster  = nWindowSettings.autoMaster;
   		    ipcclient->setVideoWindowSettings(nexus_client, m_id, &windowSettings);	
			if (rc != NEXUS_SUCCESS) 
				LOGE("BCMESPlayer::SetVideoRegion: NEXUS_VideoWindow_SetSettings, FAILED rc = %d",rc);
		}
	}

	return true;
}

bool BCMESPlayer::SetAudioCodec( AudioType audType)
{
	LOGE("BCMESPlayer::SetAudioCodec: audType = %d",audType);
	m_audioType = audType;
	return true;
}

void BCMESPlayer::VideoStart()
{
//	LOGE("BCMESPlayer::VideoStart");
	return;
}

bool BCMESPlayer::CanAcceptES(uint32_t time)
{
//	LOGE("BCMESPlayer::CanAcceptES: fifo_full = %d", fifo_full);	
	return !fifo_full;
}

void BCMESPlayer::processVideoPayload( uint32_t pts, uint8_t * buffer, size_t size)
{
	startPesPacket(pts, resources->config.videoPid,0);
    batom_accum_add_range(accum, buffer, size);
	batom_cursor_from_accum(&acc_cursor, accum);
	return;
}

void BCMESPlayer::processAudioPayload( uint32_t pts,  uint8_t * buffer, size_t size)
{
	startPesPacket(pts, resources->config.audioPid, size);
    batom_accum_add_range(accum, buffer, size);
	batom_cursor_from_accum(&acc_cursor, accum);
	return;
}

bool BCMESPlayer::SendAudioES(uint32_t pts,  uint8_t* pBuffer, int bufferSize)
{
//	LOGE("BCMESPlayer::SendAudioES: %#lx:%u", (unsigned long)pBuffer, (unsigned)bufferSize);
	BCM_MUTEX_LOCK();

	if(!pBuffer)
	{
		LOGE("BCMESPlayer::SendAudioES: pBuffer ptr is Null!!!!");
		BCM_MUTEX_UNLOCK();
		return false;
	}
    
    if(pts && !decodeTimeUpdated) 
	{ 
        decodeTimeUpdated = true;
        playTime = pts;
    }

    processAudioPayload(pts, pBuffer, bufferSize);
//	LOGE("BCMESPlayer::SendAudioES: PTS = %d pBuffer = 0x%x bufSize = %d", pts, pBuffer, bufferSize);

    bool full = sendData();

    if (full) 
	    LOGE("BCMESPlayer::SendAudioES: buffer full!!");
/*
	// Check the audio status
	NEXUS_AudioDecoderStatus audioStatus;
	NEXUS_Error rc;

	rc = NEXUS_AudioDecoder_GetStatus(resources->audioDecoder, &audioStatus);
	if (rc == NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::SendAudioES: FDec = %d FErr = %d PTS = %d QFr = %d", audioStatus.framesDecoded, audioStatus.frameErrors, audioStatus.pts, audioStatus.queuedFrames);
		LOGE("BCMESPlayer::SendAudioES: Started = %d FD = %d FS = %d PTSErr = %d", audioStatus.started, audioStatus.fifoDepth, audioStatus.fifoSize, audioStatus.ptsErrorCount);
	}

	// Check the audio state
    NEXUS_AudioDecoderTrickState audioState;
    NEXUS_AudioDecoder_GetTrickState(resources->audioDecoder, &audioState);
	LOGE("BCMESPlayer::SendAudioES: Rate = %d Muted = %d", audioState.rate, audioState.muted);

	NEXUS_AudioDecoderSettings settings;
	int32_t audioVolume = (0x800000/100)*100; // Linear scaling - values are in a 4.23 format, 0x800000 = unity.
	NEXUS_Error rc;

	NEXUS_AudioDecoder_GetSettings(resources->audioDecoder, &settings);
	LOGE("BCMESPlayer::SendAudioES: VolL = 0x%x VolR = 0x%x VolC = 0x%x", settings.volumeMatrix[NEXUS_AudioChannel_eLeft][NEXUS_AudioChannel_eLeft],
			settings.volumeMatrix[NEXUS_AudioChannel_eRight][NEXUS_AudioChannel_eRight], settings.volumeMatrix[NEXUS_AudioChannel_eCenter][NEXUS_AudioChannel_eCenter]);
	settings.volumeMatrix[NEXUS_AudioChannel_eLeft][NEXUS_AudioChannel_eLeft] = audioVolume;
	settings.volumeMatrix[NEXUS_AudioChannel_eRight][NEXUS_AudioChannel_eRight] = audioVolume;
	settings.volumeMatrix[NEXUS_AudioChannel_eCenter][NEXUS_AudioChannel_eCenter] = audioVolume;
	rc=NEXUS_AudioDecoder_SetSettings(resources->audioDecoder, &settings);

	if (rc == NEXUS_SUCCESS)	
		LOGE("Audio Status: NEXUS_AudioDecoder_SetSettings succeeded!!");
*/
	BCM_MUTEX_UNLOCK();
	return true;
}

void BCMESPlayer::dataCallback()
{
//	LOGE("BCMESPlayer::dataCallback: Locked %s %s", fifo_full?"fifo_full":"", stopped?"stopped":"");

	BCM_MUTEX_LOCK();
    if (!resources) 
	{
		BCM_MUTEX_UNLOCK();
        return;
    }

    if(!fifo_full) 
	{
//		LOGE("BCMESPlayer::dataCallback: !fifo_full");
		BCM_MUTEX_UNLOCK();
        return;
    }

    if(stopped) 
	{
		LOGE("BCMESPlayer::dataCallback stopped!!");
		BCM_MUTEX_UNLOCK();
        return;
    }

    NEXUS_PlaypumpStatus playpumpStatus;
	NEXUS_Error rc = NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);
    if (rc == NEXUS_SUCCESS) 
	{
        if ((playpumpStatus.fifoDepth + fifo_threshold_low) > playpumpStatus.fifoSize) 
		{
			LOGE("BCMESPlayer::datacallback: (playpumpStatus.fifoDepth + fifo_threshold_low) > playpumpStatus.fifoSize");
			BCM_MUTEX_UNLOCK();
            return;
        }
    } 

    LOGE("BCMESPlayer::dataCallback: bufferLow");
    fifo_full = false;
	BCM_MUTEX_UNLOCK();
    return;
}

void BCMESPlayer::staticDataCallback(void *context, int param)
{
//	LOGE("BCMESPlayer::staticDataCallback");
	((BCMESPlayer*)context)->dataCallback();
	return;
}


bool BCMESPlayer::sendData()
{
	NEXUS_Error rc;
	void *buffer;
	size_t buffer_size;
    bool full = false;	

	for(;;) {
		size_t data_size;
		rc = NEXUS_Playpump_GetBuffer(resources->playpump, &buffer, &buffer_size);
		if(rc!=NEXUS_SUCCESS) 
		{
			//rc=BERR_TRACE(rc);
			LOGE("BCMESPlayer::sendData: NEXUS_Playpump_GetBuffer failed!!");
			goto err_getBuffer;
		}

		if(buffer_size==0) {
            full = true;
			LOGE("BCMESPlayer::sendData: waiting...");
			BKNI_Sleep(10);
			continue;
		}
		data_size = batom_cursor_copy(&acc_cursor, buffer, buffer_size);
		if(data_size>0) {
#if 0
#ifdef BCMNEXUS_STREAM_CAPTURE 
            if(stream==NULL) {
                char fname[64];
                BKNI_Snprintf(fname, sizeof(fname), BCMNEXUS_STREAM_CAPTURE_name, stream_count);
                stream_count++;
                stream = fopen(fname,"wb");
            }
            if(stream) {
                fwrite(buffer, data_size, 1, stream);
            }
#endif
#endif
//			LOGE("BCMESPlayer::sendData: data_size = %d buffer = %p playpump = %p",data_size,buffer,resources->playpump);
			rc = NEXUS_Playpump_ReadComplete(resources->playpump, 0, data_size);			
			if (rc != NEXUS_SUCCESS)
			{
				LOGE("BCMESPlayer::sendData: NEXUS_Playpump_ReadComplete failed!!");
				goto err_readComplete;
			}
		}

		if (data_size < buffer_size) 
		{
//			LOGE("BCMESPlayer::sendData: data_size is less than buffersize!!");

            NEXUS_PlaypumpStatus playpumpStatus;
            bool statusValid=false;
            if (!full) 
			{
                NEXUS_Error rc;

                if (buffer_size < data_size + fifo_threshold_full) 
				{ 
					/* if number of continuous bytes smaller then threshold */
                    rc = NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);

                    if (rc != NEXUS_SUCCESS) 
					{
						LOGE("BCMESPlayer::sendData: NEXUS_Playpump_GetStatus failed!!");
                        full = true;
                        break;
                    } 

                    statusValid = true;
                    if((playpumpStatus.fifoDepth + fifo_threshold_full) >= playpumpStatus.fifoSize) 
					{
                        full = true;
                    }
                } 				
            } 

            if (!full && fifo_full) 
			{
                bool low = false;

                if (buffer_size < data_size + fifo_threshold_low) 
				{ 
					/* if number of continuous bytes smaller then threshold */
                    if (!statusValid) 
					{
                        rc = NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);

                        if (rc == NEXUS_SUCCESS) 
						{
                            statusValid = true;
                        }
                    }

                    if (statusValid) 
					{
                        low = (playpumpStatus.fifoDepth + fifo_threshold_low) <= playpumpStatus.fifoSize;
                    }
                } 
				
				else 
				{
                    low = true;
                }

                if (low) 
				{
                    fifo_full = false;
					LOGE("BCMESPlayer::sendData: bufferLow");
                }
            }

			break;
		}
	}

	if(full) 
		fifo_full = true;

	return full;

err_getBuffer:
err_readComplete:	
	return full;
}

void BCMESPlayer::startPesPacket(uint32_t pts, uint8_t id, size_t packet_size)
{
	bmedia_pes_info pes_info;
	size_t pes_header_len;
	bmedia_pes_info_init(&pes_info, id);

	if(pts) 
	{
		uint64_t pts_45Khz = CALCULATE_PTS(pts);
		/*(45/5) * (pts->GetNanoseconds()/((1000 * 1000)/5)); */
		/* we use ns, since it's native format for Time, and to convert from 1GHz to 45KHz, we need to multiply by 45000 and divide by 1.000.000.000 */
		/* BDBG_ERR(("pts:%u ms:%u us:%u ns:%u", (unsigned)pts_45Khz, (unsigned)pts->GetMilliseconds(), (unsigned)pts->GetMicroseconds(), (unsigned)pts->GetNanoseconds())); */
		pes_info.pts_valid = true;
		pes_info.pts = (uint32_t)pts_45Khz;
		if (id == resources->config.videoPid) 
		{
			pts_hi = (uint32_t)(pts_45Khz >> 32);
		}
	}

	pes_header_len = bmedia_pes_header_init(pes_header, packet_size, &pes_info);
	batom_accum_clear(accum);
	batom_accum_add_range(accum, pes_header, pes_header_len); 
	return;
}

static  uint8_t b_eos_h264[] = 
{
	0x00, 0x00, 0x01, 0x0A
};

static  uint8_t b_eos_filler[256] = 
{
    0x00
};

bool BCMESPlayer::SendVideoES(uint32_t pts,  uint8_t* pBuffer, int bufferSize)
{
//	LOGE("BCMESPlayer::SendVideoES: %u %#lx:%u %u", pts!=NULL?(unsigned)pts->GetMilliseconds():0, (unsigned long)pBuffer, (unsigned)bufferSize, (unsigned)type);
//	LOGE("BCMESPlayer::SendVideoES: called with PTS=%u pBuffer=%#lx buffersize =%u", pts, (unsigned long)pBuffer, (unsigned)bufferSize);
	
	BCM_MUTEX_LOCK();
#ifdef HEAP_SIZE_PRINT
	static int count = 0;
    NEXUS_PlatformConfiguration platformConfig;
	NEXUS_Error rc;
	if ((count++ % 20) == 0) {
		for (int i=0;i<NEXUS_MAX_HEAPS;i++) {
			NEXUS_Platform_GetConfiguration(&platformConfig);
			NEXUS_MemoryStatus status;
			NEXUS_HeapHandle heap = platformConfig.heap[i];
			if (!heap) break;
			rc = NEXUS_Heap_GetStatus(heap, &status);
			BDBG_ASSERT(!rc);
			LOGE("BCMESPlayer::SendVideoES: Heap[%d]: memcIndex=%d, size=%d, free=%d\n", i, status.memcIndex, status.size, status.free);
		}
	}
#endif

    if(pts && !decodeTimeUpdated) 
	{ 
        decodeTimeUpdated = true;
        playTime = pts;
    }

    if (pts) 
	{
        lastDecodeVideoTime = pts;
    }

    processVideoPayload(pts,pBuffer, bufferSize);
//	LOGE("BCMESPlayer::SendVideoES: PTS = %d pBuffer = 0x%x bufSize = %d", pts, pBuffer, bufferSize);

    bool full = sendData();
    if (full)
	    LOGE("BCMESPlayer::SendVideoES: buffer full!!");

	BCM_MUTEX_UNLOCK();
	return true;
}

uint32_t BCMESPlayer::GetCurrentPTS() 
{
    bool decoderTime;
//	LOGE("BCMESPlayer::GetCurrentPTS");
	BCM_MUTEX_LOCK();

    uint32_t  time = GetCurrentPTSLocked(decoderTime);
//    BDBG_MSG(("GetCurrentPTSLocked:%#x %u(%s %u:%u:%u)", (unsigned)this, (unsigned)time.GetMilliseconds(), inAccurateSeek?"AccurateSeek":"", (unsigned)prevPlayTime.GetMilliseconds(), (unsigned)lastDecodeVideoTime.GetMilliseconds(), (unsigned)targetPts.GetMilliseconds() ));
#if 0
    if(inAccurateSeek) {
        time = prevPlayTime;
    }
#endif

	BCM_MUTEX_UNLOCK();
    return time;
}

uint32_t BCMESPlayer::GetCurrentPTSLocked(bool &decoderTime) 
{
	uint64_t pts;

//	LOGE("BCMESPlayer::GetCurrentPTSLocked");
	decoderTime = false;
	if(stopped) 
	{
		return playTime;
	}

	NEXUS_Error rc = 0;
	uint32_t decoderpts = 0;
	NEXUS_PtsType ptsType = NEXUS_PtsType_eInterpolatedFromInvalidPTS;
	if (m_videoType != kVideoTypeNone) 
	{
		NEXUS_VideoDecoderStatus videoStatus;
		rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &videoStatus);
    	if (rc == NEXUS_SUCCESS) 
		{
	        NEXUS_PlaypumpStatus playpumpStatus;
            decoderpts = videoStatus.pts;
            ptsType = videoStatus.ptsType;

        	NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);
//            LOGE("BCMESPlayer::GetCurrentPTS: %u [play fifo:%u,%u] [video CDB:%u,%u %u %u]", (unsigned)decoderpts, playpumpStatus.fifoDepth, playpumpStatus.fifoSize, videoStatus.fifoDepth, videoStatus.fifoSize, videoStatus.queueDepth, videoStatus.cabacBinDepth );
	    }		
    } 
	
#ifdef FEATURE_AUDIO_HWCODEC
	else if(m_audioType != kAudioTypeNone) 
	{
		NEXUS_AudioDecoderStatus audioStatus;
        rc = NEXUS_SimpleAudioDecoder_GetStatus(resources->simpleAudioDecoder, &audioStatus);
    	if (rc == NEXUS_SUCCESS) 
		{
            decoderpts = audioStatus.pts;
            ptsType = audioStatus.ptsType;
        } 
	} 
#endif

	if (ptsType == NEXUS_PtsType_eInterpolatedFromInvalidPTS) 
	{
		return playTime;
	}

	pts = pts_hi;
	if (decoderpts > 0x7FFFFFFF && pts>0) { /* check if PTS was prior to 32-bit wrap */
		pts -=1;
	}
	pts = (pts<<32) + decoderpts;

	uint32_t time =  pts_hi; //look again
//	time.SetNanoseconds((pts*(1000*1000/5))/(45/5)); /* converted from 45KHz ticks to ns */
	decoderTime = true;
//    LOGE("BCMESPlayer::GetCurrentPTS: %u ", (unsigned)pts);
	return  time;
}

void BCMESPlayer::updatePlayTimeLocked()
{
    bool decoderTime;
//	LOGE("BCMESPlayer::updatePlayTimeLocked");
    playTime = GetCurrentPTSLocked(decoderTime);
    if(decoderTime) {
        playTimeUpdated = true;
    }
    return;
}

void BCMESPlayer::resetPlayState()
{
    waitingEos = false;
    waitingPts = false;
	pacingAudio = false;
    inAccurateSeek = false;
    return;
}

bool BCMESPlayer::Flush()
{
    NEXUS_Error rc;
    unsigned i;
    LOGE("BCMESPlayer::Flush: Enter...");

	BCM_MUTEX_LOCK();

	if(stopped) 
	{
		BCM_MUTEX_UNLOCK();
	    return false;
    }

    updatePlayTimeLocked();
    resetPlayState();
#ifdef BCMNEXUS_STREAM_CAPTURE 
    if (stream) 
	{
        fclose(stream);
        stream=NULL;
    }
#endif

    rc = NEXUS_Playpump_Flush(resources->playpump);
    if (rc != NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::Flush: NEXUS_Playpump_Flush failed!!"); 
		BCM_MUTEX_UNLOCK();
		return false;
	}

	if (m_videoType != kVideoTypeNone) 
    {
        NEXUS_SimpleVideoDecoder_Flush(resources->simpleVideoDecoder);
   }

#ifdef FEATURE_AUDIO_HWCODEC
	if (m_audioType != kAudioTypeNone) 
	{
		if (resources->simpleAudioDecoder) 
		{
			NEXUS_SimpleAudioDecoder_Flush(resources->simpleAudioDecoder);
		}
    }
#endif

	BCM_MUTEX_UNLOCK();
	LOGE("BCMESPlayer::Flush: Done!!");
	return true;
}

bool BCMESPlayer::PauseLocked()
{
	NEXUS_Error rc;
	LOGE("BCMESPlayer::PauseLocked: Enter...");
	
	BCM_MUTEX_LOCK();
	LOGE("BCMESPlayer::PauseLocked: Calling NEXUS_StcChannel_SetRate...");
    rc = NEXUS_StcChannel_SetRate(resources->stcChannel, 0, 0 /* HW adds one */ );
	if (rc != NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::PauseLocked: failed!!"); 
		BCM_MUTEX_UNLOCK();
		return false;
	}

    paused = true;
	PlayState = kPaused;
	LOGE("BCMESPlayer::PauseLocked: Done!!");
	BCM_MUTEX_UNLOCK();
	return true;
}

bool BCMESPlayer::Pause()
{
	LOGE("BCMESPlayer::Pause");
    return PauseLocked();
}

bool BCMESPlayer::StartPlayLocked(uint32_t seekTo, bool locked)
{
	NEXUS_Error rc;
    unsigned i;

	LOGE("BCMESPlayer::StartPlayLocked: Enter...");
    
	if (locked)
		BCM_MUTEX_LOCK();

//	LOGE("BCMESPlayer::StartPlayLocked: StartPlayLocked:%#x %s %s", (unsigned)this, stopped?"stopped":"", fifo_full?"fifo_full":"");

    rc = NEXUS_StcChannel_SetRate(resources->stcChannel, 1, 0 /* HW adds one */ );
	if (rc != NEXUS_SUCCESS) 
	{
		LOGE("BCMESPlayer::StartPlayLocked: NEXUS_StcChannel_SetRate failed!!");
		if (locked)
			BCM_MUTEX_UNLOCK();
		return false;
	}

    paused = false;
    if (stopped) 
	{
		LOGE("BCMESPlayer::StartPlayLocked: stopped case...");
        NEXUS_SimpleAudioDecoderStartSettings audioProgram;
        resetPlayState();
        stopped = false;

        NEXUS_SimpleVideoDecoderStartSettings svProgram;
        NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svProgram);
        NEXUS_VideoDecoderStartSettings videoProgram;
        NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
        videoProgram.codec = resources->config.videoCodec;
        videoProgram.pidChannel = resources->videoPidChannel;
        videoProgram.stcChannel = resources->stcChannel;

#ifdef FEATURE_AUDIO_HWCODEC
        NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
        audioProgram.primary.codec = resources->config.audioCodec;
        audioProgram.primary.pidChannel = resources->audioPidChannel;
        audioProgram.primary.stcChannel = resources->stcChannel;
//		LOGE("BCMESPlayer::StartPlayLocked: Audio stuff, codec = %d pidC = %d stcC = %d", resources->config.audioCodec, resources->audioPidChannel, resources->stcChannel);
#endif 
		LOGE("BCMESPlayer::StartPlayLocked: Calling NEXUS_Playpump_Start...");
        rc = NEXUS_Playpump_Start(resources->playpump);
        if (rc != NEXUS_SUCCESS) 
		{
			LOGE("BCMESPlayer::StartPlayLocked: NEXUS_Playpump_Start failed!!");
			goto err_playpumpStart;
		}

	    if (m_videoType != kVideoTypeNone) 
		{
			LOGE("BCMESPlayer::StartPlayLocked: Calling NEXUS_VideoDecoder_Start...");
            svProgram.settings = videoProgram;
            rc = NEXUS_SimpleVideoDecoder_Start(resources->simpleVideoDecoder, &svProgram);
            if (rc != NEXUS_SUCCESS) 
			{
				LOGE("BCMESPlayer::StartPlayLocked: NEXUS_VideoDecoder_Start failed!!, rc = 0x%x!!", rc);
				goto err_videoDecoderStart;
			}

			LOGE("BCMESPlayer::StartPlayLocked: NEXUS_VideoDecoder_Start succeeded!!");
        }

#ifdef FEATURE_AUDIO_HWCODEC
	    if (m_audioType != kAudioTypeNone) 
		{
            if (resources->simpleAudioDecoder) 
			{
                NEXUS_AudioDecoderTrickState audioState;

				LOGE("BCMESPlayer::StartPlayLocked: Calling NEXUS_SimpleAudioDecoder_Start...");

                rc = NEXUS_SimpleAudioDecoder_Start(resources->simpleAudioDecoder, &audioProgram);
                if (rc != NEXUS_SUCCESS) 
				{
					LOGE("BCMESPlayer::StartPlayLocked: NEXUS_SimpleAudioDecoder_Start failed!!, rc = 0x%x", rc);
					goto err_audioDecoderStart;
				}

				LOGE("BCMESPlayer::StartPlayLocked: NEXUS_SimpleAudioDecoder_Start succeeded!!");
                NEXUS_SimpleAudioDecoder_GetTrickState(resources->simpleAudioDecoder, &audioState);

				// HV: We will nute the audio here and un-mute when
				// we get our 1st PTS (videoDecoderFirstPtsPassed)
				resetPlayState();
                if (m_videoType != kVideoTypeNone)
				{
                    audioState.rate = 0;
                    audioState.muted = true;
                } 

				else
				{
                    audioState.rate = NEXUS_NORMAL_PLAY_SPEED;
                    audioState.muted = false;
                }

				LOGE("BCMESPlayer::StartPlayLocked: Calling NEXUS_SimpleAudioDecoder_SetTrickState...");
                rc = NEXUS_SimpleAudioDecoder_SetTrickState(resources->simpleAudioDecoder, &audioState);
                if (rc != NEXUS_SUCCESS) 
				{ 
					LOGE("BCMESPlayer::StartPlayLocked: NEXUS_AudioDecoder_SetTrickState failed!!");
				}
            }
        }
#endif
        playTimeUpdated = false;
        decodeTimeUpdated = false;

//      LOGE("BCMESPlayer::StartPlayLocked: %#x -> kPlaying", (unsigned)this);
        if (m_videoType != kVideoTypeNone)  
		{
//            uint32_t pts = (45 * seekTo->GetMicroseconds())/1000;
			uint32_t pts = CALCULATE_PTS(seekTo);
//            LOGE("BCMESPlayer::StartPlayLocked: %#x start from PTS:%u(%u)", (unsigned)this, (unsigned)pts, pts/45);
            rc = NEXUS_SimpleVideoDecoder_SetStartPts(resources->simpleVideoDecoder, pts);
            if (rc) 
			{
				LOGE("BCMESPlayer::StartPlayLocked: NEXUS_VideoDecoder_SetStartPts failed!!"); 
				goto err_videoDecoderSetStartPts;
			}
        }

		inAccurateSeek = true;
        prevPlayTime = playTime;
        pacingAudio = false;
	}
	
	else 
	{
		LOGE("BCMESPlayer::StartPlayLocked: non-stopped case...");
        resetPlayState();

#ifdef FEATURE_AUDIO_HWCODEC
		if (m_audioType != kAudioTypeNone) 
		{
			LOGE("BCMESPlayer::StartPlayLocked: non-stopped case, found valid audio type!!");
            
            if (resources->simpleAudioDecoder) 
			{
				NEXUS_AudioDecoderTrickState audioState;
				LOGE("BCMESPlayer::StartPlayLocked: non-stopped case, found valid audioDecoder!!");

				if (rc != NEXUS_SUCCESS) 
				{
					LOGE("BCMESPlayer::StartPlayLocked: failed!!");
					goto err_audioDecoderStart;
				}
                NEXUS_SimpleAudioDecoder_GetTrickState(resources->simpleAudioDecoder, &audioState);
				audioState.rate = NEXUS_NORMAL_PLAY_SPEED;
				audioState.muted = false;

				LOGE("BCMESPlayer::StartPlayLocked: Calling NEXUS_AudioDecoder_SetTrickState...");
                rc = NEXUS_SimpleAudioDecoder_SetTrickState(resources->simpleAudioDecoder, &audioState);
				if (rc != NEXUS_SUCCESS) 
				{ 
					LOGE("BCMESPlayer::StartPlayLocked: NEXUS_AudioDecoder_SetTrickState failed!!");
					goto err_audioDecoderStart;
				}
			}
		}
#endif
	}

	LOGE("BCMESPlayer::StartPlayLocked: Success!!");

	if (locked)
		BCM_MUTEX_UNLOCK();

	return true;

err_videoDecoderSetStartPts:
	LOGE("BCMESPlayer::StartPlayLocked: Video decoder set start pts failed!!");
err_audioDecoderStart:
	LOGE("BCMESPlayer::StartPlayLocked: Audio decoder start failed!!");
    if (resources->simpleAudioDecoder) 	
    {        
        NEXUS_SimpleAudioDecoder_Stop(resources->simpleAudioDecoder);      
        // resources->audioStatusUpdate(resources, false);    
    }
err_videoDecoderStart:
	LOGE("BCMESPlayer::StartPlayLocked: Video decoder start failed!!");
    NEXUS_SimpleVideoDecoder_SetStartPts(resources->simpleVideoDecoder, 0);
err_playpumpStart:
	LOGE("BCMESPlayer::StartPlayLocked: failed!!");

	if (locked)
		BCM_MUTEX_UNLOCK();

	return false;
}

bool BCMESPlayer::Play()
{
	LOGE("BCMESPlayer::Play: Enter...");
    if (StartPlayLocked(0, true))
	{
		PlayState = kPlaying;
		LOGE("BCMESPlayer::Play: Done (true)!!");
		return true;
	}

	else
	{
		LOGE("BCMESPlayer::Play: Done (false)!!");
		return false;
	}
}

bool BCMESPlayer::Play(uint32_t decodeToTime,bool pauseAtDecodeTime)
{
	LOGE("BCMESPlayer::Play: Enter... (%u,%s)", (unsigned)decodeToTime,pauseAtDecodeTime?"pause":"");
    uint32_t time = decodeToTime;

	BSTD_UNUSED(pauseAtDecodeTime);	
    if (StartPlayLocked(time, true))
	{
		PlayState = kPlaying;
		return true;
	}
	else
		return false;    
}

bool BCMESPlayer::Stop(bool videoPaused_)
{
    unsigned i;	

    LOGE("BCMESPlayer::Stop: Enter...");
	BCM_MUTEX_LOCK();

    {
        if (stopped)
		{
			BCM_MUTEX_UNLOCK();
			LOGE("BCMESPlayer::Stop: returning (case-1)");
            return true;
        }

#ifdef BCMNEXUS_STREAM_CAPTURE 
        if(stream) 
		{
            fclose(stream);
            stream=NULL;
        }
#endif

        if(videoPaused!=videoPaused_) 
		{
	        if (m_videoType != kVideoTypeNone) 
			{
                NEXUS_VideoDecoderSettings videoDecoderSettings;
                NEXUS_SimpleVideoDecoder_GetSettings(resources->simpleVideoDecoder, &videoDecoderSettings);
                videoDecoderSettings.channelChangeMode = videoPaused_?NEXUS_VideoDecoder_ChannelChangeMode_eHoldUntilTsmLock:NEXUS_VideoDecoder_ChannelChangeMode_eMute;
                NEXUS_Error rc = NEXUS_SimpleVideoDecoder_SetSettings(resources->simpleVideoDecoder, &videoDecoderSettings);
                if (rc != NEXUS_SUCCESS) 
				{	
					BCM_MUTEX_UNLOCK();
					LOGE("BCMESPlayer::Stop: returning (case-2)");
					return false; 
				}
            }
        }

        if (videoPaused)
		{
            updatePlayTimeLocked();
        }

		else 
		{            
			playTime = 0;
        }

        if (m_videoType != kVideoTypeNone) 
		{
            NEXUS_SimpleVideoDecoder_Stop(resources->simpleVideoDecoder);
        }

#ifdef FEATURE_AUDIO_HWCODEC
        if (m_audioType != kAudioTypeNone) 
		{
            if (resources->simpleAudioDecoder) 	
            {        
                NEXUS_SimpleAudioDecoder_Stop(resources->simpleAudioDecoder);      
            }
        }
#endif

        NEXUS_Playpump_Stop(resources->playpump);
		stopped = true;
		paused = false;
		waitingEos = false;
		videoPaused = videoPaused_;
    }

	PlayState = kStopped;
	BCM_MUTEX_UNLOCK();
	LOGE("BCMESPlayer::Stop: Done!!");
    return true;
}

State BCMESPlayer::GetPlayState()
{
	return PlayState;
}

bool BCMESPlayer::NotifyEOF()
{
	LOGE("BCMESPlayer::NotifyEOF");
	
	BCM_MUTEX_LOCK();
    startPesPacket(0, resources->config.videoPid, sizeof(b_eos_h264));
    batom_accum_add_range(accum, b_eos_h264, sizeof(b_eos_h264));
	batom_cursor_from_accum(&acc_cursor, accum);
    bool full = sendData();

    if (full) 
	{
		LOGE("BCMESPlayer::NotifyEOF: Unable to sendEOSData b_eos_h264");
    }

    startPesPacket(0, resources->config.videoPid, 16*sizeof(b_eos_filler));
    for (unsigned i = 0; i < 16; i++) 
	{
        batom_accum_add_range(accum, b_eos_h264, sizeof(b_eos_filler));
    }

	batom_cursor_from_accum(&acc_cursor, accum);
	full = sendData();

    if (full) 
	{
		LOGE("BCMESPlayer::NotifyEOF: Unable to sendEOSData b_eos_filler");
    }

    unsigned cdbDepth;
    fifoMarker = GetFifoMarker(cdbDepth);
    waitingEos = true;

	BCM_MUTEX_UNLOCK();
	return true;
}

/* B_MIN_CDB_DEPTH should be greater than max picture. If there's no video_picture_queue info, and if PTS's aren't changing,
we will wait for playback+CDB to go below this. */
#define B_MIN_CDB_DEPTH (1024*1024) /* With 1080p source, we need to set this at 1 MB */

unsigned BCMESPlayer::GetFifoMarker(unsigned &cdbDepth) 
{
    NEXUS_Error rc;
    NEXUS_PlaypumpStatus playpumpStatus;
    NEXUS_VideoDecoderStatus videoStatus;
    unsigned fifoMarker;

    fifoMarker = 0;
    cdbDepth = 0;
    rc = NEXUS_Playpump_GetStatus(resources->playpump, &playpumpStatus);
    if (rc == NEXUS_SUCCESS) 
	{
//        LOGE("BCMESPlayer::GetFifoMarker: %s: playpump %u",  "GetFifoMark", (unsigned)playpumpStatus.fifoDepth);
        if (playpumpStatus.fifoDepth>0) 
		{
            /* if there's even 1 byte in the playback fifo, we must still wait */
            cdbDepth += B_MIN_CDB_DEPTH;
        }
    }

    if (m_videoType != kVideoTypeNone) 
	{
        rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &videoStatus);
        if (rc == NEXUS_SUCCESS)
		{ 
            if (videoStatus.started) 
			{
//                LOGE("BCMESPlayer::GetFifoMarker: %s: video %u:%u:%u",  "GetFifoMark", (unsigned)videoStatus.fifoDepth, (unsigned)videoStatus.queueDepth, (unsigned)videoStatus.pts);
                if (videoStatus.queueDepth > 0) 
				{
                    cdbDepth += B_MIN_CDB_DEPTH;
                }

                fifoMarker += videoStatus.pts;
                fifoMarker += videoStatus.queueDepth;
            }
        }
    }

#ifdef FEATURE_AUDIO_HWCODEC
    if (m_audioType != kAudioTypeNone) 
	{
        //for(unsigned i=0;i<sizeof(resources->audioDecoder)/sizeof(resources->audioDecoder[0]);i++) {
            NEXUS_AudioDecoderStatus audioStatus;
           /* if( resources->audioDecoder == NULL) {
                continue;
            }*/
            rc = NEXUS_SimpleAudioDecoder_GetStatus(resources->simpleAudioDecoder, &audioStatus);
            if (rc == NEXUS_SUCCESS) 
			{
                if (audioStatus.started) 
				{
//                    LOGE("BCMESPlayer::GetFifoMarker: %s:  %u:%u:%u",  "GetFifoMark",  (unsigned)audioStatus.fifoDepth, (unsigned)audioStatus.queuedFrames, (unsigned)audioStatus.pts);
                    cdbDepth += audioStatus.fifoDepth;
                    if (audioStatus.queuedFrames>8) 
					{
                        cdbDepth += B_MIN_CDB_DEPTH;
                    }

                    fifoMarker += audioStatus.pts;
                    fifoMarker += audioStatus.queuedFrames;
                }
            }
        //}
    }
#endif

    fifoMarker += cdbDepth; 
//    LOGE("BCMESPlayer::GetFifoMarker: %s: total %u:%u",  "GetFifoMark", (unsigned)cdbDepth, (unsigned)fifoMarker);
    return fifoMarker;
}

void BCMESPlayer::videoSourceCallback()
{
    if (!resources) 
	{
        return;
    }

    if (m_videoType != kVideoTypeNone) 
	{
        NEXUS_VideoDecoderStatus status;
        NEXUS_Error rc;
        rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &status);
        if (rc == NEXUS_SUCCESS) 
		{
            if(status.started && status.source.width>0 && status.source.height>0) 
			{
/*                Notifier::Event eventVideo = Notifier::Event(kVideoStreamer,kVDimAvailable);
                eventVideo.m_videoWidth = status.source.width;
                eventVideo.m_videoHeight = status.source.height;
                BDBG_MSG(("videoSourceCallback: video %ux%u",(unsigned)status.source.width, (unsigned)status.source.height));
                SendNotification(eventVideo);*/
            }
        }
    }
}

void BCMESPlayer::staticVideoSourceCallback (void *context, int)
{
	((BCMESPlayer*)context)->videoSourceCallback();
    return;
}

void BCMESPlayer::videoDecoderFirstPtsPassed()
{
//    LOGE("BCMESPlayer::videoDecoderFirstPtsPassed: %#x %s", (unsigned)this, inAccurateSeek?"inAccurateSeek":"");
	BCM_MUTEX_LOCK();

    if (inAccurateSeek) 
	{
        inAccurateSeek = false;
        pacingAudio = false;

        if (m_videoType != kVideoTypeNone) 
		{
            NEXUS_VideoDecoderStatus videoStatus;
            NEXUS_Error rc;

            rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &videoStatus);
            if (rc == NEXUS_SUCCESS) 
                LOGE("BCMESPlayer::videoDecoderFirstPtsPassed: %#x video ptsType:%u pts:%u(%u)", (unsigned)this, (unsigned)videoStatus.ptsType, (unsigned)videoStatus.pts, (unsigned)videoStatus.pts/45);

			if (m_audioType != kAudioTypeNone) 
			{
                if (resources->simpleAudioDecoder)
				{
					NEXUS_AudioDecoderTrickState audioState;
#ifdef FEATURE_AUDIO_HWCODEC
					NEXUS_AudioDecoderStatus audioStatus;

                    rc = NEXUS_SimpleAudioDecoder_GetStatus(resources->simpleAudioDecoder, &audioStatus);
					if (rc == NEXUS_SUCCESS) 
						LOGE("BCMESPlayer::videoDecoderFirstPtsPassed: %#x audio[0] ptsType:%u pts:%u(%u)", (unsigned)this,  (unsigned)audioStatus.ptsType, (unsigned)audioStatus.pts, (unsigned)audioStatus.pts/45);

                    NEXUS_SimpleAudioDecoder_GetTrickState(resources->simpleAudioDecoder, &audioState);

//					LOGE("BCMESPlayer::videoDecoderFirstPtsPassed: waitingPts = %d", waitingPts);
					audioState.rate = NEXUS_NORMAL_PLAY_SPEED;
					audioState.muted = waitingPts ? true : false;

                    rc = NEXUS_SimpleAudioDecoder_SetTrickState(resources->simpleAudioDecoder, &audioState);
					if (rc != NEXUS_SUCCESS) 
					{ 
						LOGE("BCMESPlayer::videoDecoderFirstPtsPassed: NEXUS_AudioDecoder_SetTrickState failed!!");
					}
#endif
				}
			}
        }
    }

	BCM_MUTEX_UNLOCK();
    return;
}

void BCMESPlayer::staticVideoDecoderPictureReady(void *context, int param)
{
	((BCMESPlayer*)context)->videoDecoderPictureReady();
	return;
}

void BCMESPlayer::videoDecoderPictureReady()
{
    NEXUS_VideoDecoderStatus videoStatus;
	NEXUS_Error rc=NEXUS_SUCCESS;

    rc = NEXUS_SimpleVideoDecoder_GetStatus(resources->simpleVideoDecoder, &videoStatus);
	if (rc != NEXUS_SUCCESS)
		LOGE("BCMESPlayer::videoDecoderPictureReady: NEXUS_VideoDecoder_GetStatus failed!!");

//	if (videoStatus.numDecoded || videoStatus.numDisplayed || videoStatus.numDecodeErrors || videoStatus.numPicturesReceived)
//		LOGE("BCMESPlayer::videoDecoderPictureReady: PTS = %u Dec = %d Disp = %d Err = %d Rcvd = %d",videoStatus.pts,videoStatus.numDecoded,videoStatus.numDisplayed,videoStatus.numDecodeErrors, videoStatus.numPicturesReceived);

	//NEXUS_VideoDecoder_GetStatus and NEXUS_VideoDecoder_GetExtendedStatus
	/* This callback is invoked when a picture is made available to the display module */

	/* Send Buffer update notifications for indicating buffer level */
//	Notifier::Event updateEvent = Notifier::Event(kVideoStreamer,kFrameUpdate);
//   	GetBufferLevels(updateEvent);
//    SendNotification(updateEvent);
}

void BCMESPlayer::staticVideoDecoderFirstPtsPassed(void *context, int )
{	
	((BCMESPlayer*)context)->videoDecoderFirstPtsPassed();
	return;
}

void BCMESPlayer::videoDecoderFirstPts()
{
    return;
}

void BCMESPlayer::staticVideoDecoderFirstPts(void *context, int )
{
	((BCMESPlayer*)context)->videoDecoderFirstPts();
	return;
}

void BCMESPlayer::SetVolume(int volume)
{
	NEXUS_SimpleAudioDecoderSettings settings;
	int32_t audioVolume = (0x800000/100)*1; // Linear scaling - values are in a 4.23 format, 0x800000 = unity.
	NEXUS_Error rc;

	if (volume)
		audioVolume = (0x800000/100)*volume;

	NEXUS_SimpleAudioDecoder_GetSettings(resources->simpleAudioDecoder, &settings);
	LOGE("BCMESPlayer::BCMESPlayer::SetVolume: VolL = 0x%x VolR = 0x%x VolC = 0x%x", settings.primary.volumeMatrix[NEXUS_AudioChannel_eLeft][NEXUS_AudioChannel_eLeft], settings.primary.volumeMatrix[NEXUS_AudioChannel_eRight][NEXUS_AudioChannel_eRight], 
			settings.primary.volumeMatrix[NEXUS_AudioChannel_eCenter][NEXUS_AudioChannel_eCenter]);
	settings.primary.volumeMatrix[NEXUS_AudioChannel_eLeft][NEXUS_AudioChannel_eLeft] = audioVolume;
	settings.primary.volumeMatrix[NEXUS_AudioChannel_eRight][NEXUS_AudioChannel_eRight] = audioVolume;
	settings.primary.volumeMatrix[NEXUS_AudioChannel_eCenter][NEXUS_AudioChannel_eCenter] = audioVolume;
	rc = NEXUS_SimpleAudioDecoder_SetSettings(resources->simpleAudioDecoder, &settings);
	if (rc == NEXUS_SUCCESS)	
		LOGE("Audio Status: NEXUS_AudioDecoder_SetSettings succeeded!!");
}

// Expose the private C++ methods via the C functions
extern "C" 
{
// Create an object and store the handle
void BRCMProxy_Create(int id)
{
 	LOGE("BCMESPlayer::BRCMProxy_Create: Creating a BCMESPlayer object...");
#ifdef FEATURE_AUDIO_HWCODEC
	g_Private = new BCMESPlayer(id, kVideoTypeH264, kAudioTypeAAC);	
#else
	g_Private = new BCMESPlayer(id, kVideoTypeH264, kAudioTypeNone);
#endif
	LOGE("BCMESPlayer::BRCMProxy_Create: g_Private = %p", g_Private);
}

bool BRCMProxy_Init()
{
	bool b_Init = false;

    b_Init = g_Private->Init();
 	LOGE("BCMESPlayer::BRCMProxy_Init: b_Init = %d", b_Init);

	return b_Init;
}

void BRCMProxy_Destroy()
{
 	LOGE("BCMESPlayer::BRCMProxy_Destroy: Deleting...");
	delete g_Private;
	LOGE("BCMESPlayer::BRCMProxy_Destroy: Done!!");
}

bool BRCMProxy_CanAcceptVidES(uint32_t time)
{
    return g_Private->CanAcceptES(time);
}

bool BRCMProxy_SendVideoES(uint32_t time, uint8_t* pBuffer, int bufferSize)
{
    return g_Private->SendVideoES(time, pBuffer, bufferSize);
}

void BRCMProxy_VideoStart()
{
    g_Private->VideoStart();
}

bool BRCMProxy_Flush()
{
    return g_Private->Flush();
}

bool BRCMProxy_Play()
{
	return g_Private->Play();
}

bool BRCMProxy_Pause()
{
    return g_Private->Pause();
}

bool BRCMProxy_SetVideoRegion(int32_t x, int32_t y, int32_t w, int32_t h)
{
    return g_Private->SetVideoRegion(x, y, w, h);
}

bool BRCMProxy_CanAcceptAudES(uint32_t time)
{
    return g_Private->CanAcceptES(time);
}

bool BRCMProxy_SendAudioES(uint32_t time, uint8_t* pBuffer, int bufferSize)
{
    return g_Private->SendAudioES(time, pBuffer, bufferSize);
}

bool BRCMProxy_SetAudioCodec(int codecType)
{
    return g_Private->SetAudioCodec((AudioType)codecType);
}

void BRCMProxy_SetVolume(int volume)
{
//	LOGE("BCMESPlayer::BRCMProxy_SetVolume: Enter... vol = %d", volume);
//    return g_Private->SetVolume(volume);
}

void BRCMProxy_SetSeek()
{
	//g_Private->SetSeek();
}

unsigned long long BRCMProxy_GetCurrentPTS()
{
	return g_Private->GetCurrentPTS();
}
} // extern "C"

#if 0
bool BrcmStreamPlayerProxy::Stop(bool videoPaused)
{
    return m_private->Stop(videoPaused);
	//return false;

}

bool BrcmStreamPlayerProxy::NeedsDecimation()
{
	return m_private->CanAcceptES(0);
	//return false;
}

bool BrcmStreamPlayerProxy::SetState(const char * caller,State  value)
{
   // return m_private->SetState(caller, value);
	PlayState 
	return false;
}

State  BrcmStreamPlayerProxy::GetState()
{
   // return m_private->GetState();
	return PlayState;
}
#endif
#endif
