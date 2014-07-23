/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
 * $brcm_Workfile: bcmPlayer_live.cpp $
 *
 *****************************************************************************/
// Verbose messages removed
// #define LOG_NDEBUG 0

#define LOG_TAG "bcmPlayer_live"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

typedef struct
{
    unsigned short a,v,p;
    NEXUS_VideoCodec v_codec;
    NEXUS_AudioCodec a_codec;
    unsigned input;
}live_config;

static live_config live_stream_info;
 

static int bcmPlayer_init_live(int iPlayerIndex) 
{
    int rc;

    rc = bcmPlayer_init_base(iPlayerIndex);

    if (rc == 0) {

        // Now connect the client resources
        b_refsw_client_client_info client_info;
        nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

        b_refsw_client_connect_resource_settings connectSettings;

        nexus_handle[iPlayerIndex].ipcclient->getDefaultConnectClientSettings(&connectSettings);
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

static void bcmPlayer_uninit_live(int iPlayerIndex) 
{
    LOGD("bcmPlayer_uninit_live");
    
    NEXUS_PidChannel_Close(nexus_handle[iPlayerIndex].videoPidChannel);
    NEXUS_PidChannel_Close(nexus_handle[iPlayerIndex].audioPidChannel);            
    
    bcmPlayer_uninit_base(iPlayerIndex);
                
    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }
    
    LOGD("bcmPlayer_uninit_live()  completed");
}

static void playback_turnoff(int iPlayerIndex){
    if(nexus_handle[iPlayerIndex].playback)
    {
        NEXUS_PlaybackStatus status;
        
        NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

        if(status.state == NEXUS_PlaybackState_ePaused)
        {
            LOGE("PLAYBACK HASN'T BEEN STARTED YET!");
        }

        if(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
        {
            NEXUS_Playback_Stop(nexus_handle[iPlayerIndex].playback);

            while(status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused)
                NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &status);

            //usleep(2000000);/* wait for playback stoping */
        }
    }

    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }

    if(nexus_handle[iPlayerIndex].playback)
        NEXUS_Playback_CloseAllPidChannels(nexus_handle[iPlayerIndex].playback);    
    if(nexus_handle[iPlayerIndex].playback)
        NEXUS_Playback_Destroy(nexus_handle[iPlayerIndex].playback);
    nexus_handle[iPlayerIndex].playback = NULL;

    if(nexus_handle[iPlayerIndex].playpump)
        NEXUS_Playpump_Close(nexus_handle[iPlayerIndex].playpump);
    nexus_handle[iPlayerIndex].playpump = NULL;
}

static int bcmPlayer_setDataSource_live(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader) {
    LOGV("bcmPlayer_setDataSource_live('%s')", url); 

    NEXUS_SimpleStcChannelSettings stcSettings;
    
/*    memset(&live_stream_info, 0x0, sizeof(live_config));*/
    
    *videoWidth = 1280;
    *videoHeight = 720;

    /*
     * trying to play a/v which source is tunner. it is live playing.
     * audio and video decoders have already been opened.
     * video/audio decoder outputs have also been connected to HDMI.
     * Here we should:
     * 1. close playback.
     * 2. set live stream data path. inband->parserband->pidchannel->decoder.
     *
     * The program infos are stored in a file, which should consist following 
     * information at least.
     * 1. input band number that connect to demo output.
     * 2. parser band which is freed.
     * 3. audio pid number.
     * 4. video pid number.
     * 5. pcr pid number.
     * 6. audio codec for nexus.
     * 7. video codec for nexus.
     *
     * We are not consider conditional Access support so far, so just ignore ECM/EMM PIDs. 
     * */
    NEXUS_ParserBand parserBand;
    NEXUS_ParserBandSettings parserBandSettings;

    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_AudioDecoderStartSettings audioProgram;
    NEXUS_VideoDecoderSettings videoDecoderSettings;

    /*firstly, remove playback handle*/
    playback_turnoff(iPlayerIndex);
    /* Map a parser band to the demod's input band. */
    parserBand = NEXUS_ParserBand_e0;
    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = live_stream_info.input;  /* Platform initializes this to input band */
    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);

    nexus_handle[iPlayerIndex].videoPidChannel = NEXUS_PidChannel_Open(parserBand, live_stream_info.v, NULL);
    nexus_handle[iPlayerIndex].audioPidChannel = NEXUS_PidChannel_Open(parserBand, live_stream_info.a, NULL);
    NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
    videoProgram.codec = live_stream_info.v_codec;
    videoProgram.pidChannel = nexus_handle[iPlayerIndex].videoPidChannel;
    NEXUS_AudioDecoder_GetDefaultStartSettings(&audioProgram);
    audioProgram.codec = live_stream_info.a_codec;
    audioProgram.pidChannel = nexus_handle[iPlayerIndex].audioPidChannel;

    if (nexus_handle[iPlayerIndex].simpleStcChannel) {
        NEXUS_SimpleStcChannel_GetSettings(nexus_handle[iPlayerIndex].simpleStcChannel, &stcSettings);
        stcSettings.mode = NEXUS_StcChannelMode_ePcr; /* live */
        stcSettings.modeSettings.pcr.pidChannel = nexus_handle[iPlayerIndex].videoPidChannel; /* PCR happens to be on video pid */
        stcSettings.modeSettings.pcr.offsetThreshold = 0xFF;
        NEXUS_SimpleStcChannel_SetSettings(nexus_handle[iPlayerIndex].simpleStcChannel, &stcSettings);
    }

    NEXUS_SimpleVideoDecoder_SetStcChannel(nexus_handle[iPlayerIndex].simpleVideoDecoder, nexus_handle[iPlayerIndex].simpleStcChannel);
    NEXUS_SimpleAudioDecoder_SetStcChannel(nexus_handle[iPlayerIndex].simpleAudioDecoder, nexus_handle[iPlayerIndex].simpleStcChannel);

    NEXUS_SimpleVideoDecoderStartSettings svdStartSettings;
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svdStartSettings);
    NEXUS_SimpleVideoDecoder_GetSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderSettings);
    svdStartSettings.settings = videoProgram;
    if ((iPlayerIndex == 0) && 
        (videoDecoderSettings.supportedCodecs[NEXUS_VideoCodec_eH265]) &&
        ((nexus_handle[iPlayerIndex].maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
            (nexus_handle[iPlayerIndex].maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))) {
            svdStartSettings.maxWidth  = 3840;
            svdStartSettings.maxHeight = 2160;
        }
    NEXUS_SimpleVideoDecoder_Start(nexus_handle[iPlayerIndex].simpleVideoDecoder, &svdStartSettings);

    NEXUS_SimpleAudioDecoderStartSettings sadStartSettings;
    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&sadStartSettings);
    sadStartSettings.primary = audioProgram;
    NEXUS_SimpleAudioDecoder_Start(nexus_handle[iPlayerIndex].simpleAudioDecoder, &sadStartSettings);
    LOGE("video and audio are all started\n");

    return 0;
}

void bcmPlayer_config_live(int iPlayerIndex, const char* flag,const char* value)
{
    int output;
    
    if(strncmp(flag, "input", 5) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.input = output;

        LOGV("input band:%d",live_stream_info.input);
        return;
    }
 
    if(strncmp(flag, "v_codec", 7) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.v_codec = (NEXUS_VideoCodec)output;
        LOGV("video codec:%d",live_stream_info.v_codec);
        return;
    }

    if(strncmp(flag, "a_codec", 7) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.a_codec = (NEXUS_AudioCodec)output;
        LOGV("audio codec:%d",live_stream_info.a_codec);
        return;
    }

    if(strncmp(flag, "a_pid", 5) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.a = output;
        LOGV("audio pid:%d",live_stream_info.a);
        return;
    }

    if(strncmp(flag, "v_pid", 5) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.v = output;
        LOGV("video pid:%d",live_stream_info.v);
        return;
    }

    if(strncmp(flag, "p_pid", 5) == 0)
    {
        sscanf(value,"%d",&output);
        live_stream_info.p = output;
        LOGV("pcr pid:%d",live_stream_info.p);
        return;
    }

    LOGV("invalid live stream configuration\n");
}

static int bcmPlayer_getMediaExtractorFlags_live(int iPlayerIndex, unsigned *flags)
{
    *flags = CAN_PAUSE;

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void player_reg_live(bcmPlayer_func_t *pFuncs){

    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_live;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_live;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_live;
    pFuncs->bcmPlayer_config_func = bcmPlayer_config_live;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_live;
}

