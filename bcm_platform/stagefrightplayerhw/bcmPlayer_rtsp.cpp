/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 * $brcm_Workfile: bcmPlayer_rtsp.cpp $
 * $brcm_Revision: 5 $
 * $brcm_Date: 1/7/13 1:14p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_rtsp.cpp $
 * 
 * 5   1/7/13 1:14p mnelis
 * SWANDROID-293: Don't call B_Os_Uninit twice
 * 
 * 4   12/14/12 2:30p kagrawal
 * SWANDROID-277: Wrapper IPC APIs for
 *  NEXUS_SimpleXXX_Acquire()/_Release()
 * 
 * 3   11/29/12 2:54p mnelis
 * SWANDROID-241: Build failed without branched playback_ip code
 * 
 * 2   11/29/12 12:41p mnelis
 * SWANDROID-241: Merge RTSP support to mainline
 * 
 * 1   10/30/12 5:23p mnelis
 * SWANDROID-241 : Add RTSP support to Android
 * 
 * 12   10/17/12 5:52p mnelis
 * SWANDROID-189: Cleanup URL parsing code, avoiding globals to store bits
 *  and pieces.
 * 
 * 11   9/24/12 6:47p mnelis
 * SWANDROID-78: Fix HTML5 streaming behavior
 * 
 * 10   9/19/12 2:02p mnelis
 * SWANDROID-78: General tidyup
 * 
 * 9   9/14/12 1:28p mnelis
 * SWANDROID-78: Use NEXUS_ANY_ID for STC channel and store playbackip in
 *  nexus_handle struct
 * 
 * 8   6/21/12 3:08p franktcc
 * SWANDROID-120: Adding error callback function
 * 
 * 7   6/5/12 2:39p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 6   2/24/12 4:12p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 5   2/8/12 2:54p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 4   12/29/11 6:45p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 3   12/13/11 10:27a franktcc
 * SW7420-1906: Adding capability of setAspectRatio to ICS
 * 
 * 2   12/10/11 7:19p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 2   11/28/11 6:11p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 6   10/19/11 4:08p franktcc
 * SW7425-1540: Correct uninit sequence.
 * 
 * 5   9/22/11 5:00p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 4   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 3   8/22/11 5:35p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 * 1   8/22/11 4:05p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 *
 *****************************************************************************/
#define LOG_TAG "bcmPlayer_ip"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

/* include IP lib header files */
#include "b_os_lib.h"
#include "b_playback_ip_lib.h"
#include "nexus_playpump.h"

#define IP_NETWORK_JITTER 300

/* 2 percent end of stream error*/
#define EOS_ERROR_PERCENTAGE 0.02

//#define BCMPLAYER_RTSP_STCCHANNEL_SUPPORT 1

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

extern "C" int get_hostname_uri_from_url(const char *url, char *hostname, int *portnum, char **uri);

/* Globals... */
B_PlaybackIpSessionOpenSettings ipSessionOpenSettings;
B_PlaybackIpSessionOpenStatus ipSessionOpenStatus;
B_PlaybackIpPsiInfo psi;

bool stop_called, eos_called;

static int bcmPlayer_init_rtsp(int iPlayerIndex) 
{
    int rc;

    rc = bcmPlayer_init_base(iPlayerIndex);

    if (rc == 0) {
        // Now connect the client resources
        b_refsw_client_client_info client_info;
        nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

        b_refsw_client_connect_resource_settings connectSettings;

        BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));
        /* TODO: Set NxClient_VideoDecoderCapabilities and NxClient_VideoWindowCapabilities of connectSettings */
        connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
        connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
        connectSettings.simpleVideoDecoder[0].windowId = iPlayerIndex; /* Main or PIP Window */
        if (nexus_handle[iPlayerIndex].bSupportsHEVC == true)
        {
            if ((nexus_handle[iPlayerIndex].maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                (nexus_handle[iPlayerIndex].maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))
            {
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
            }
            connectSettings.simpleVideoDecoder[0].decoderCaps.supportedCodecs[NEXUS_VideoCodec_eH265] = NEXUS_VideoCodec_eH265;
        }

        connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

        if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true) {
            LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
            rc = 1;
        }
    }
    return rc;
}

static void bcmPlayer_uninit_rtsp(int iPlayerIndex) {
    B_PlaybackIpError rc;
    NEXUS_PlaypumpStatus playpumpStatus;

    LOGD("bcmPlayer_uninit_rtsp");

    if(nexus_handle[iPlayerIndex].playbackIp){
        rc = B_PlaybackIp_SessionStop(nexus_handle[iPlayerIndex].playbackIp);
        LOGD("B_PlaybackIp_SessionStop rc = %d\n", rc);
    }

    if(nexus_handle[iPlayerIndex].playbackIp) {
        rc = B_PlaybackIp_SessionClose(nexus_handle[iPlayerIndex].playbackIp);
        LOGD("B_PlaybackIp_SessionClose rc = %d\n", rc);
    }

    if(nexus_handle[iPlayerIndex].playbackIp){
        rc = B_PlaybackIp_Close(nexus_handle[iPlayerIndex].playbackIp);
        LOGD("B_PlaybackIp_Close rc = %d\n", rc);
        B_Os_Uninit();
    }

    nexus_handle[iPlayerIndex].playbackIp = NULL;

    if(nexus_handle[iPlayerIndex].playpump) {
        NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &playpumpStatus);
        if(playpumpStatus.started){
            NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].playpump);
            NEXUS_Playpump_CloseAllPidChannels(nexus_handle[iPlayerIndex].playpump);
        }
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
    }

    nexus_handle[iPlayerIndex].playpump = NULL;

    if(nexus_handle[iPlayerIndex].simpleVideoDecoder) {
        NEXUS_SimpleVideoDecoder_Stop(nexus_handle[iPlayerIndex].simpleVideoDecoder);
        nexus_handle[iPlayerIndex].ipcclient->releaseVideoDecoderHandle(nexus_handle[iPlayerIndex].simpleVideoDecoder);
    }

    if(nexus_handle[iPlayerIndex].simpleAudioDecoder) {
        NEXUS_SimpleAudioDecoder_Stop(nexus_handle[iPlayerIndex].simpleAudioDecoder);
        nexus_handle[iPlayerIndex].ipcclient->releaseAudioDecoderHandle(nexus_handle[iPlayerIndex].simpleAudioDecoder);       
    }

    if(nexus_handle[iPlayerIndex].stcChannel)
        NEXUS_StcChannel_Close(nexus_handle[iPlayerIndex].stcChannel);

    nexus_handle[iPlayerIndex].stcChannel = NULL;
}

static void playbackIpEventCallback(void *appCtx, B_PlaybackIpEventIds eventId)
{
    LOGD("%s: Got EventId %d from IP library, appCtx %p", __FUNCTION__, eventId, appCtx);
}

static int bcmPlayer_setDataSource_rtsp(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader) {

    NEXUS_Error             rc;
    int portnum;
    char hostname[256], *uri = NULL;
    NEXUS_StcChannelSettings stcSettings;
    B_PlaybackIpSessionSetupSettings ipSessionSetupSettings;
    B_PlaybackIpSessionSetupStatus ipSessionSetupStatus;

    portnum = 554;
    get_hostname_uri_from_url(url, &hostname[0], &portnum, &uri);

    LOGD("Host: %s port %d\n", hostname, portnum);

    memset(&psi, 0, sizeof(struct B_PlaybackIpPsiInfo));

    B_Os_Init();

#ifdef BCMPLAYER_RTSP_STCCHANNEL_SUPPORT
    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    /* for IP stream, add more settings */
    /* offset threshold: uses upper 32 bits (183ticks/msec) of PCR clock */
    stcSettings.modeSettings.pcr.offsetThreshold = IP_NETWORK_JITTER * 183; 
    /* max pcr error: uses upper 32 bits (183ticks/msec) of PCR clock */
    stcSettings.modeSettings.pcr.maxPcrError = 0xFFFFFFFF;   /* Settop API had it set to this value */
    /* stcSettings.modeSettings.pcr.pidChannel = videoPidChannel; */ /* PCR happens to be on video pid */
    /*  PCR Offset "Jitter Adjustment" is not suitable for use with IP channels Channels, so disable it */
    stcSettings.modeSettings.pcr.disableJitterAdjustment = true;
    /* Disable Auto Timestamp correction for PCR Jitter */
    stcSettings.modeSettings.pcr.disableTimestampCorrection = true;
    /* We just configured the Timebase, so turn off auto timebase config */
    stcSettings.autoConfigTimebase = false;
    
    LOGD("NEXUS_StcChannel_Open()");
    nexus_handle[iPlayerIndex].stcChannel = NEXUS_StcChannel_Open(NEXUS_ANY_ID, &stcSettings);
    if (nexus_handle[iPlayerIndex].stcChannel == NULL)
    {
        LOGE("can not open a STC channel");
        goto NEXUS_StcChannel_Open_err;
    }
#endif

    LOGD("B_PlaybackIp_Open()");
    nexus_handle[iPlayerIndex].playbackIp = B_PlaybackIp_Open(NULL);
    if (nexus_handle[iPlayerIndex].playbackIp == NULL)
    {
        LOGE("B_PlaybackIp_Open() fail!");
        return 1;
    }
    LOGD("After B_PlaybackIp_Open()");

    /* Cache the URL and host.*/
    B_PlaybackIp_GetDefaultSessionOpenSettings(&ipSessionOpenSettings);
    BKNI_Memset(&ipSessionOpenStatus, 0, sizeof(ipSessionOpenStatus));
    strncpy(ipSessionOpenSettings.socketOpenSettings.ipAddr, (char*)hostname, sizeof(ipSessionOpenSettings.socketOpenSettings.ipAddr)-1);
    ipSessionOpenSettings.useNexusPlaypump = true;
    ipSessionOpenSettings.socketOpenSettings.port = portnum;
    ipSessionOpenSettings.socketOpenSettings.protocol = B_PlaybackIpProtocol_eRtsp;
    ipSessionOpenSettings.maxNetworkJitter = 300;   /* <=300ms jitter allowed */
    ipSessionOpenSettings.ipMode = B_PlaybackIpClockRecoveryMode_ePushWithPcrNoSyncSlip;
    ipSessionOpenSettings.networkTimeout = 1;  /* timeout in 1 sec during network outage events */
    if (uri != NULL) {
        ipSessionOpenSettings.socketOpenSettings.url = strdup((char*) uri);
    }

    if (extraHeader)
        ipSessionOpenSettings.u.rtsp.additionalHeaders = strdup(extraHeader);

    ipSessionOpenSettings.u.rtsp.userAgent = strdup("BRCM IP Applib Test App/2.0");
    ipSessionOpenSettings.u.rtsp.copyResponseHeaders = false;
    ipSessionOpenSettings.eventCallback = playbackIpEventCallback;
    ipSessionOpenSettings.appCtx = nexus_handle[iPlayerIndex].playbackIp; /* this should be app handle */
    ipSessionOpenSettings.nonBlockingMode = false;
    ipSessionOpenSettings.eventCallback = playbackIpEventCallback;

    LOGD("B_PlaybackIp_SessionOpen()");
    rc = B_PlaybackIp_SessionOpen(nexus_handle[iPlayerIndex].playbackIp, &ipSessionOpenSettings, &ipSessionOpenStatus);
    while (rc == B_ERROR_IN_PROGRESS) { /* alternatively, we could wait for the callback */
        usleep(1000000);
        rc = B_PlaybackIp_SessionOpen(nexus_handle[iPlayerIndex].playbackIp, &ipSessionOpenSettings, &ipSessionOpenStatus);
    }

    free(ipSessionOpenSettings.u.rtsp.additionalHeaders);
    free(ipSessionOpenSettings.u.rtsp.userAgent);

    if (rc != B_ERROR_SUCCESS) {
        LOGE("Session Open call failed: rc %d, RTSP Status %d\n", rc, ipSessionOpenStatus.u.rtsp.statusCode);
        goto B_PlaybackIp_SessionOpen_err;
    }
    LOGD("Session Open call succeeded, RTSP status code %d", ipSessionOpenStatus.u.rtsp.statusCode);
    if (ipSessionOpenSettings.u.rtsp.copyResponseHeaders 
        && ipSessionOpenStatus.u.rtsp.responseHeaders) {
        LOGD("Response Header is... %s \n", ipSessionOpenStatus.u.rtsp.responseHeaders);
        /* Note: App can extract any customer Response Headers here: useful to extract DLNA flags from getContentFeatures Header */
        /* now free the responseHeader */
        free(ipSessionOpenStatus.u.rtsp.responseHeaders);
    }

    B_PlaybackIp_GetDefaultSessionSetupSettings(&ipSessionSetupSettings);
    ipSessionSetupSettings.u.rtsp.userAgent = "BRCM Android LiveMedia RTSP bcmPlayer";
    /* apps can request IP library to return the RTSP Response Message incase it needs to extract any custom headers */
    ipSessionSetupSettings.u.rtsp.copyResponseHeaders = false;
    ipSessionSetupSettings.u.rtsp.psiParsingTimeLimit = 3000;   /* Wait up to 3s for PSI information */

    LOGD("B_PlaybackIp_SessionSetup()");
    rc = B_PlaybackIp_SessionSetup(nexus_handle[iPlayerIndex].playbackIp, &ipSessionSetupSettings, &ipSessionSetupStatus);
    while (rc == B_ERROR_IN_PROGRESS) { /* alternatively, we could wait for the callback */
        usleep(1000000);
        rc = B_PlaybackIp_SessionSetup(nexus_handle[iPlayerIndex].playbackIp, &ipSessionSetupSettings, &ipSessionSetupStatus);
    }

    if (rc != B_ERROR_SUCCESS) {
        LOGE("Session Setup call failed: rc %d\n", rc);
        goto B_PlaybackIp_SessionSetup_err;
    }
#ifdef LIVEMEDIA_SUPPORT
    if (ipSessionSetupStatus.u.rtsp.psi.psiValid) {
        memcpy(&psi, &ipSessionSetupStatus.u.rtsp.psi, sizeof(struct B_PlaybackIpPsiInfo));
        LOGE("%s: Video dimensions : %dx%d duration %d", __FUNCTION__, psi.videoWidth, psi.videoHeight, psi.duration);

        *videoHeight = psi.videoHeight;
        *videoWidth = psi.videoWidth;
    } else 
#endif
    {
        LOGE("%s: Couldn't get PSI information", __FUNCTION__);
        goto B_PlaybackIp_SessionSetup_err;
    }

    return 0;


B_PlaybackIp_SessionSetup_err:
    if(nexus_handle[iPlayerIndex].playbackIp)
            B_PlaybackIp_SessionClose(nexus_handle[iPlayerIndex].playbackIp);
B_PlaybackIp_SessionOpen_err:
    if(nexus_handle[iPlayerIndex].playbackIp) {
        B_PlaybackIp_Close(nexus_handle[iPlayerIndex].playbackIp);
        nexus_handle[iPlayerIndex].playbackIp = NULL;
    }

NEXUS_StcChannel_Open_err:
    return 1;
}

int bcmPlayer_start_rtsp (int iPlayerIndex)
{
    B_PlaybackIpSessionStartSettings ipSessionStartSettings;
    B_PlaybackIpSessionStartStatus ipSessionStartStatus;
    NEXUS_PlaypumpOpenPidChannelSettings pidSettings;
    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_SimpleAudioDecoderStartSettings audioProgram;
    NEXUS_PlaypumpSettings playPumpSettings;
    B_PlaybackIpError rc;
    B_PlaybackIpStatus ipStatus;
    NEXUS_Error err;

    B_PlaybackIp_GetStatus(nexus_handle[iPlayerIndex].playbackIp, &ipStatus);

    if (ipStatus.ipState == B_PlaybackIpState_ePaused) {
        /*Resume Play.*/
        bcmPlayer_pause(iPlayerIndex);
        return 0;
    }

    NEXUS_Playpump_GetSettings(nexus_handle[iPlayerIndex].playpump, &playPumpSettings);
    playPumpSettings.transportType = psi.mpegType;
    err = NEXUS_Playpump_SetSettings(nexus_handle[iPlayerIndex].playpump, &playPumpSettings);
    if (err != NEXUS_SUCCESS) {
        LOGD("NEXUS_Playpump_SetSettings() failed, returned: %d\n", err);
        return 1;
    }

    /* Playback must be told which stream ID is used for video and for audio. */
    if (nexus_handle[iPlayerIndex].simpleVideoDecoder && psi.videoCodec)
    {
        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
        pidSettings.pidType = NEXUS_PidType_eVideo;
        nexus_handle[iPlayerIndex].videoPidChannel = NEXUS_Playpump_OpenPidChannel(nexus_handle[iPlayerIndex].playpump, psi.videoPid, &pidSettings);

        NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
        videoProgram.codec = psi.videoCodec;
        videoProgram.pidChannel = nexus_handle[iPlayerIndex].videoPidChannel;
#ifdef BCMPLAYER_RTSP_STCCHANNEL_SUPPORT
        videoProgram.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
#endif
        NEXUS_SimpleVideoDecoderStartSettings svdStartSettings;
        NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svdStartSettings);
        svdStartSettings.settings = videoProgram;
        NEXUS_SimpleVideoDecoder_Start(nexus_handle[iPlayerIndex].simpleVideoDecoder, &svdStartSettings);
        LOGD("NEXUS_VideoDecoder_Start() called\n");
    }

    if (nexus_handle[iPlayerIndex].simpleAudioDecoder && psi.audioCodec)
    {
        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidSettings);
        pidSettings.pidType = NEXUS_PidType_eAudio;
        pidSettings.pidTypeSettings.audio.codec = psi.audioCodec;
        nexus_handle[iPlayerIndex].audioPidChannel = NEXUS_Playpump_OpenPidChannel(nexus_handle[iPlayerIndex].playpump, psi.audioPid, &pidSettings);
        NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
        audioProgram.primary.codec = psi.audioCodec;
        audioProgram.primary.pidChannel = nexus_handle[iPlayerIndex].audioPidChannel;
#ifdef BCMPLAYER_RTSP_STCCHANNEL_SUPPORT
        audioProgram.primary.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
#endif
        NEXUS_SimpleAudioDecoder_Start(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioProgram);
        LOGD("NEXUS_SimpleAudioDecoder_Start() called\n");
    }

    NEXUS_Playpump_Start(nexus_handle[iPlayerIndex].playpump);

    /* for IP stream, start Playback IP session */
    BKNI_Memset(&ipSessionStartSettings, 0, sizeof(ipSessionStartSettings));
    ipSessionStartSettings.u.rtsp.mediaTransportProtocol = B_PlaybackIpProtocol_eRtp;
    ipSessionStartSettings.u.rtsp.keepAliveInterval = 10;  /* send rtsp heart beats (keepalive) every 10sec */
    ipSessionStartSettings.nexusHandles.playpump = nexus_handle[iPlayerIndex].playpump;
    ipSessionStartSettings.nexusHandles.simpleVideoDecoder = nexus_handle[iPlayerIndex].simpleVideoDecoder;
#ifdef BCMPLAYER_RTSP_STCCHANNEL_SUPPORT
    ipSessionStartSettings.nexusHandles.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
#endif
    ipSessionStartSettings.nexusHandles.simpleAudioDecoder = nexus_handle[iPlayerIndex].simpleAudioDecoder;
    ipSessionStartSettings.mpegType = psi.mpegType;
    ipSessionStartSettings.nexusHandlesValid = true;
    LOGD("B_PlaybackIp_SessionStart()");
    rc = B_PlaybackIp_SessionStart(nexus_handle[iPlayerIndex].playbackIp, &ipSessionStartSettings, &ipSessionStartStatus);
    while (rc == B_ERROR_IN_PROGRESS) { /* alternatively, we could wait for the callback */
        usleep(100000);
        rc = B_PlaybackIp_SessionStart(nexus_handle[iPlayerIndex].playbackIp, &ipSessionStartSettings, &ipSessionStartStatus);
    }
    if (rc != B_ERROR_SUCCESS) {
        LOGE("Session start call failed: rc %d\n", rc);
        goto B_PlaybackIp_SessionStart_err;
    }

    stop_called = false;
    eos_called = false;

    return 0;

B_PlaybackIp_SessionStart_err:
    if(nexus_handle[iPlayerIndex].playbackIp)
            B_PlaybackIp_SessionClose(nexus_handle[iPlayerIndex].playbackIp);

	return 1;
}

static int bcmPlayer_isPlaying_rtsp(int iPlayerIndex) {

    B_PlaybackIpStatus ipStatus;
    unsigned int position_w_error;

    if (stop_called || eos_called) {
        return 0;
    }

    B_PlaybackIp_GetStatus(nexus_handle[iPlayerIndex].playbackIp, &ipStatus);

    if (ipStatus.ipState == B_PlaybackIpState_ePaused)
        return 0;
    else if( psi.duration == 0)
        return 1;

    position_w_error = (unsigned int)(ipStatus.position + (EOS_ERROR_PERCENTAGE * psi.duration));

    if (position_w_error >= psi.duration) {
        LOGD("%s Sending EOS", __FUNCTION__);
        bcmPlayer_endOfStreamCallback(NULL, iPlayerIndex);
        eos_called = true;
    }

    return (ipStatus.ipState == B_PlaybackIpState_ePlaying);
}

static int bcmPlayer_stop_rtsp(int iPlayerIndex) {

    LOGE("bcmPlayer_stop_rtsp()");
    stop_called = true;

    if(nexus_handle[iPlayerIndex].playpump) {
        NEXUS_Playpump_Stop(nexus_handle[iPlayerIndex].playpump);
        NEXUS_Playpump_CloseAllPidChannels(nexus_handle[iPlayerIndex].playpump);
    }

    return 0;
}

int bcmPlayer_pause_rtsp(int iPlayerIndex) {

    B_PlaybackIpTrickModesSettings trickModeSettings;
    B_PlaybackIpError err;

    LOGE("bcmPlayer_pause_rtsp()");

    if (nexus_handle[iPlayerIndex].playbackIp) {
        if ((err = B_PlaybackIp_GetTrickModeSettings(nexus_handle[iPlayerIndex].playbackIp, &trickModeSettings)) == 0)
        {
            if ((err = B_PlaybackIp_Pause(nexus_handle[iPlayerIndex].playbackIp, &trickModeSettings)) != 0)
            {
                LOGE("bcmPlayer_pause_rtsp() : Error calling B_PlaybackIp_Pause : %d", err);
                return 1;
            }
        }
        else
        {
                LOGE("bcmPlayer_pause_rtsp() : Error calling B_PlaybackIp_GetTrickModeSettings : %d", err);
                return 1;
        }
    }

    return 0;
}


int bcmPlayer_getMediaExtractorFlags_rtsp(int iPlayerIndex, unsigned *flags)
{
    *flags = 0; // Cannot perform trick modes with RTSP using playback_ip yet

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void player_reg_rtsp(bcmPlayer_func_t *pFuncs){
    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_rtsp;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_rtsp;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_rtsp;
    pFuncs->bcmPlayer_start_func = bcmPlayer_start_rtsp;
    pFuncs->bcmPlayer_isPlaying_func = bcmPlayer_isPlaying_rtsp;
    pFuncs->bcmPlayer_stop_func = bcmPlayer_stop_rtsp;
    pFuncs->bcmPlayer_pause_func = bcmPlayer_pause_rtsp;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_rtsp;
}

