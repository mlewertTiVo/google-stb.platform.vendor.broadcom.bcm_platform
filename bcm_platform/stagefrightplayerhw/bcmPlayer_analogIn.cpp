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
 *****************************************************************************/
#define LOG_TAG "bcmPlayer_AnalogIn"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

#ifdef ANDROID_SUPPORTS_ANALOG_INPUT

/* Set below to 1 to use TVP5147 external chip for CVBS to DVI conversion */
#define EXTERNAL_TVP5147_CVBS_DVI_CHIP 1

#if EXTERNAL_TVP5147_CVBS_DVI_CHIP
#include "tvp5147_cvbs_dvi_conversion.h"
#endif

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

static int bcmPlayer_init_analogIn(int iPlayerIndex)
{
    int rc;

    LOGV("bcmPlayer_init_analogIn");
    rc = bcmPlayer_init_base(iPlayerIndex);

#if EXTERNAL_TVP5147_CVBS_DVI_CHIP
    program_external_cvbs_dvi_chip();
#endif

    LOGD("bcmPlayer_init_analogIn() completed (rc=%d)", rc);
    return rc;
}
    
static void bcmPlayer_uninit_analogIn(int iPlayerIndex) {
    LOGD("bcmPlayer_uninit_analogIn");
    
    if(nexus_handle[iPlayerIndex].simpleAudioPlayback) {
        NEXUS_SimpleAudioPlayback_Stop(nexus_handle[iPlayerIndex].simpleAudioPlayback);
        nexus_handle[iPlayerIndex].ipcclient->releaseAudioPlaybackHandle(nexus_handle[iPlayerIndex].simpleAudioPlayback);
        nexus_handle[iPlayerIndex].simpleAudioPlayback = NULL;
    }

    if(nexus_handle[iPlayerIndex].hddvi) {
        NEXUS_HdDviInput_Close(nexus_handle[iPlayerIndex].hddvi);
        nexus_handle[iPlayerIndex].hddvi = NULL;
        // disconnect the resources connected in setSource()
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);
    }
        
    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }

    bcmPlayer_uninit_base(iPlayerIndex);
    
    LOGD("bcmPlayer_uninit_analogIn() completed");
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

int bcmPlayer_setDataSource_analogIn(int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{
    int rc = 0;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings connectSettings;    
    NEXUS_HdDviInputSettings hddviSettings;
    NEXUS_SimpleAudioPlaybackStartSettings startSettings;

    LOGV("bcmPlayer_setDataSource_analogIn('%s')", url); 

    *videoWidth = 1280;
    *videoHeight = 720;

    // firstly, remove playback handle
    playback_turnoff(iPlayerIndex);

    nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

    // Now connect the client resources
    BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));
    // hdmiIn for dvi input
    connectSettings.hdmiInput.id = client_info.hdmiInputId;
    connectSettings.hdmiInput.windowId = iPlayerIndex;
    connectSettings.hdmiInput.surfaceClientId = client_info.surfaceClientId;
    connectSettings.hdmiInput.hdDvi = true;
    // audio playback for I2S input
    connectSettings.simpleAudioPlayback[0].id = client_info.audioPlaybackId;
    connectSettings.simpleAudioPlayback[0].i2s.enabled = true;        
    connectSettings.simpleAudioPlayback[0].i2s.index = 0; //TODO: where to get this from?
    if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true) {
        LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
        return 1;
    }

    /* open hddvi alias */
    NEXUS_HdDviInput_GetDefaultSettings(&hddviSettings);

    /* hddvi settings - copied from nexus/nxclient/apps/analog_input.c  */
    hddviSettings.autoFormat = false;
	hddviSettings.format = NEXUS_VideoFormat_eNtsc;
	hddviSettings.inputDataMode = 30;
	hddviSettings.enableDe = true;

	hddviSettings.colorSpace              = NEXUS_ColorSpace_eYCbCr422;
	hddviSettings.colorPrimaries          = NEXUS_ColorPrimaries_eSmpte_170M;
	hddviSettings.matrixCoef              = NEXUS_MatrixCoefficients_eSmpte_170M;
	hddviSettings.transferCharacteristics = NEXUS_TransferCharacteristics_eSmpte_170M;

	hddviSettings.external = true;
	hddviSettings.startPosition.enabled = true;
	hddviSettings.startPosition.horizontal = 95;
	hddviSettings.startPosition.vertical = 12;
    /* hddvi settings ends */    
    
    nexus_handle[iPlayerIndex].hddvi = NEXUS_HdDviInput_Open(NEXUS_ALIAS_ID + 0, &hddviSettings);
    if(nexus_handle[iPlayerIndex].hddvi == NULL) {
        LOGE("%s: NEXUS_HdDviInput_Open(%d) failed", __FUNCTION__, NEXUS_ALIAS_ID + 0);
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);
        return 1;
    } else {
        LOGW("%s: video started via analog_in", __FUNCTION__);
    }

    nexus_handle[iPlayerIndex].simpleAudioPlayback = nexus_handle[iPlayerIndex].ipcclient->acquireAudioPlaybackHandle();	
    if ( NULL == nexus_handle[iPlayerIndex].simpleAudioPlayback )    
    {       
        LOGE("%s: Unable to acquire Simple Audio Playback!", __PRETTY_FUNCTION__);
        NEXUS_HdDviInput_Close(nexus_handle[iPlayerIndex].hddvi);
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);
        return 1;
    }    

    NEXUS_SimpleAudioPlayback_GetDefaultStartSettings(&startSettings);
    startSettings.sampleRate = 44100;
    startSettings.bitsPerSample = 16;
    rc = NEXUS_SimpleAudioPlayback_Start(nexus_handle[iPlayerIndex].simpleAudioPlayback, &startSettings);
    if (rc) {
        LOGE("%s: Unable to start Simple Audio Playback!", __PRETTY_FUNCTION__);
        nexus_handle[iPlayerIndex].ipcclient->releaseAudioPlaybackHandle(nexus_handle[iPlayerIndex].simpleAudioPlayback);
        NEXUS_HdDviInput_Close(nexus_handle[iPlayerIndex].hddvi);
        nexus_handle[iPlayerIndex].ipcclient->disconnectClientResources(nexus_handle[iPlayerIndex].nexus_client);
        return 1;
    } else {
        LOGW("**************************** \n %s: Both audio - video started via BcmAnalogIn \n ****************************", __FUNCTION__);
    }
    
    return rc;
}

static int bcmPlayer_getMediaExtractorFlags_analogIn(int iPlayerIndex, unsigned *flags)
{
    *flags = CAN_PAUSE;

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void player_reg_analogIn(bcmPlayer_func_t *pFuncs){

    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_analogIn;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_analogIn;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_analogIn;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_analogIn;
}

#else
void player_reg_analogIn(bcmPlayer_func_t *pFuncs){
    /* Nothing */
}
#endif

