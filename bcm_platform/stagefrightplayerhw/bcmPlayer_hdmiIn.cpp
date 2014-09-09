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
 * $brcm_Workfile: bcmPlayer_hdmiIn.cpp $
 *
 *****************************************************************************/
// Verbose messages removed
//#define LOG_NDEBUG 0

#define LOG_TAG "bcmPlayer_hdmiIn"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

static int bcmPlayer_init_hdmiIn(int iPlayerIndex)
{
    int rc;

    LOGV("[%d]bcmPlayer_init_hdmiIn", iPlayerIndex);

    rc = bcmPlayer_init_base(iPlayerIndex);

    LOGD("[%d]bcmPlayer_init_hdmiIn() completed (rc=%d)", iPlayerIndex, rc);
    return rc;
}
    
static void bcmPlayer_uninit_hdmiIn(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGV("[%d]bcmPlayer_uninit_hdmiIn", iPlayerIndex);
    
    bcmPlayer_uninit_base(iPlayerIndex);

#if NEXUS_HAS_HDMI_INPUT
    if (nexusHandle->hdmiInput) {
        NEXUS_HdmiInput_Close(nexusHandle->hdmiInput);
        nexusHandle->hdmiInput = NULL;
    }
#endif

    LOGD("[%d]bcmPlayer_uninit_hdmiIn() completed", iPlayerIndex);
}

static void playback_turnoff(int iPlayerIndex)
{
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    if (nexusHandle->playback) {
        NEXUS_PlaybackStatus status;
        
        NEXUS_Playback_GetStatus(nexusHandle->playback, &status);

        if (status.state == NEXUS_PlaybackState_ePaused) {
            LOGE("PLAYBACK HASN'T BEEN STARTED YET!");
        }

        if (status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused) {
            NEXUS_Playback_Stop(nexusHandle->playback);

            while (status.state == NEXUS_PlaybackState_ePlaying || status.state == NEXUS_PlaybackState_ePaused) {
                NEXUS_Playback_GetStatus(nexusHandle->playback, &status);
            }
        }
    }

    if (nexusHandle->playback) {
        NEXUS_Playback_CloseAllPidChannels(nexusHandle->playback);    
        NEXUS_Playback_Destroy(nexusHandle->playback);
        nexusHandle->playback = NULL;
    }

    if (nexusHandle->playpump) {
        NEXUS_Playpump_Close(nexusHandle->playpump);
        nexusHandle->playpump = NULL;
    }
}

#if NEXUS_HAS_HDMI_INPUT && NEXUS_HAS_HDMI_OUTPUT && !ANDROID_SUPPORTS_HDMI_LEGACY
struct hdmi_edid {
    const uint8_t *data;
    unsigned size;
    bool allocated;
};

static void get_hdmi_output_edid(NEXUS_HdmiOutputHandle hdmiOutput, struct hdmi_edid *edid)
{
    NEXUS_Error rc;
    uint8_t *attachedRxEdid;
    size_t attachedRxEdidSize;
    NEXUS_HdmiOutputBasicEdidData hdmiOutputBasicEdidData;
    NEXUS_HdmiOutputEdidBlock edidBlock;
    NEXUS_HdmiOutputStatus status;
    unsigned i;

    static const uint8_t sampleEdid[] =
    {
        0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x08, 0x6D, 0x74, 0x22, 0x05, 0x01, 0x11, 0x20,
        0x00, 0x14, 0x01, 0x03, 0x80, 0x00, 0x00, 0x78, 0x0A, 0xDA, 0xFF, 0xA3, 0x58, 0x4A, 0xA2, 0x29,
        0x17, 0x49, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
        0x45, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18, 0x71, 0x1C, 0x16, 0x20,
        0x58, 0x2C, 0x25, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x9E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x42,
        0x43, 0x4D, 0x37, 0x34, 0x32, 0x32, 0x2F, 0x37, 0x34, 0x32, 0x35, 0x0A, 0x00, 0x00, 0x00, 0xFD,
        0x00, 0x17, 0x3D, 0x0F, 0x44, 0x0F, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x89,

        0x02, 0x03, 0x3C, 0x71, 0x7F, 0x03, 0x0C, 0x00, 0x40, 0x00, 0xB8, 0x2D, 0x2F, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xE3, 0x05, 0x1F, 0x01, 0x49, 0x90, 0x05, 0x20, 0x04, 0x03, 0x02, 0x07,
        0x06, 0x01, 0x29, 0x09, 0x07, 0x01, 0x11, 0x07, 0x00, 0x15, 0x07, 0x00, 0x01, 0x1D, 0x00, 0x72,
        0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x1E, 0x8C, 0x0A,
        0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xBA, 0x88, 0x21, 0x00, 0x00, 0x18,
        0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x0B, 0x88, 0x21, 0x00,
        0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9D
    };

    memset(edid, 0, sizeof(*edid));

    /* default to sample edid if not connect or there's any failure */
    edid->data = sampleEdid;
    edid->size = sizeof(sampleEdid);

    if (hdmiOutput) {
        NEXUS_HdmiOutput_GetStatus(hdmiOutput, &status);
    }
    else {
        status.connected = false;
    }
    if (!status.connected) {
        return;
    }

    rc = NEXUS_HdmiOutput_GetBasicEdidData(hdmiOutput, &hdmiOutputBasicEdidData);
    if (rc!=NEXUS_SUCCESS) {
        ALOGE("%s: Unable to get downstream EDID; Use default EDID for repeater's EDID", __FUNCTION__);
        return;
    }
    /* allocate space to hold the EDID blocks */
    attachedRxEdidSize = (hdmiOutputBasicEdidData.extensions + 1) * sizeof(edidBlock.data);
    attachedRxEdid = (uint8_t *)BKNI_Malloc(attachedRxEdidSize);
    for (i = 0; i <= hdmiOutputBasicEdidData.extensions; i++) {
        unsigned j;

        rc = NEXUS_HdmiOutput_GetEdidBlock(hdmiOutput, i, &edidBlock);
        if (rc!=NEXUS_SUCCESS) {
            ALOGE("%s: Error retrieving EDID Block %d from attached receiver;", __FUNCTION__, i);
            BKNI_Free(attachedRxEdid);
            return;
        }

        /* copy EDID data */
        for (j=0; j < sizeof(edidBlock.data); j++)  {
            assert(i*sizeof(edidBlock.data)+j < attachedRxEdidSize);
            attachedRxEdid[i*sizeof(edidBlock.data)+j] = edidBlock.data[j];
        }
    }
    edid->data = attachedRxEdid;
    edid->allocated = true;
    edid->size = attachedRxEdidSize;
    return;
}

static void free_hdmi_output_edid(struct hdmi_edid *edid)
{
    if (edid->allocated && edid->data) {
        BKNI_Free((void*)edid->data);
        edid->data = NULL;
    }
}
#endif

int bcmPlayer_setDataSource_hdmiIn(int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader)
{
    int rc = 0;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings connectSettings;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGV("[%d]bcmPlayer_setDataSource_hdmiIn('%s')", iPlayerIndex, url); 

    *videoWidth = 1280;
    *videoHeight = 720;
    
    // firstly, remove playback handle
    playback_turnoff(iPlayerIndex);

    nexusHandle->ipcclient->getClientInfo(nexusHandle->nexus_client, &client_info);

    // Now connect the client resources
    nexusHandle->ipcclient->getDefaultConnectClientSettings(&connectSettings);
    connectSettings.hdmiInput.id = client_info.hdmiInputId;
    connectSettings.hdmiInput.windowId = iPlayerIndex;
    connectSettings.hdmiInput.surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

#if !ANDROID_SUPPORTS_HDMI_LEGACY
    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = iPlayerIndex; /* Main or PIP Window */
    connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 0;
    connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 0;
#endif

    if (nexusHandle->ipcclient->connectClientResources(nexusHandle->nexus_client, &connectSettings) != true) {
        LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexusHandle->ipcclient->getClientName());
        rc = 1;
    }
    else {
#if NEXUS_HAS_HDMI_INPUT && NEXUS_HAS_HDMI_OUTPUT && !ANDROID_SUPPORTS_HDMI_LEGACY
        /* With URSR 14.2, NxClient HDMI Input architecture has completely changed and we need to open the HDMI input
           in the same process that starts the simple A/V decoders (HDMI). */
        struct hdmi_edid edid;
        unsigned index = 0;
        NEXUS_HdmiOutputHandle hdmiOutput;
        NEXUS_HdmiInputHandle hdmiInput;
        NEXUS_HdmiInputSettings hdmiInputSettings;

        hdmiOutput = NEXUS_HdmiOutput_Open(NEXUS_ALIAS_ID + 0, NULL);
        get_hdmi_output_edid(hdmiOutput, &edid);
        if (hdmiOutput) {
            NEXUS_HdmiOutput_Close(hdmiOutput);
        }

        NEXUS_HdmiInput_GetDefaultSettings(&hdmiInputSettings);
        hdmiInputSettings.timebase = NEXUS_Timebase_e0;
        hdmiInputSettings.frontend.hpdDisconnected = false;
        hdmiInputSettings.useInternalEdid = true;
        nexusHandle->hdmiInput = NEXUS_HdmiInput_OpenWithEdid(index, &hdmiInputSettings, edid.data, edid.size);
        if (!nexusHandle->hdmiInput) {
            ALOGE("%s: HdmiInput %d not available", __FUNCTION__, index);
            rc = NEXUS_NOT_AVAILABLE;
        }
        free_hdmi_output_edid(&edid);
#endif
    }
    return rc;
}

static int bcmPlayer_start_hdmiIn(int iPlayerIndex) 
{
#if NEXUS_HAS_HDMI_INPUT && NEXUS_HAS_HDMI_OUTPUT && !ANDROID_SUPPORTS_HDMI_LEGACY
    NEXUS_Error rc;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];

    LOGV("[%d]bcmPlayer_start_hdmiIn", iPlayerIndex);

    rc = NEXUS_SimpleVideoDecoder_StartHdmiInput(nexusHandle->simpleVideoDecoder, nexusHandle->hdmiInput, NULL);
    if (rc != NEXUS_SUCCESS) {
        LOGE("%s: NEXUS_SimpleVideoDecoder_StartHdmiInput returned : %d", __FUNCTION__, rc);
        return -1;
    }

    rc = NEXUS_SimpleAudioDecoder_StartHdmiInput(nexusHandle->simpleAudioDecoder, nexusHandle->hdmiInput, NULL);
    if (rc != NEXUS_SUCCESS) {
        LOGE("%s: NEXUS_SimpleAudioDecoder_StartHdmiInput returned : %d", __FUNCTION__, rc);
        return -1;
    }
#endif
    LOGI("[%d]%s: video and audio are all started via hdmi_in", iPlayerIndex, __FUNCTION__);
    return 0;
}

static int bcmPlayer_isPlaying_hdmiIn(int iPlayerIndex)
{
    bool playing = true;
#if NEXUS_HAS_HDMI_INPUT && NEXUS_HAS_HDMI_OUTPUT && !ANDROID_SUPPORTS_HDMI_LEGACY
    NEXUS_Error rc;
    NEXUS_VideoDecoderStatus status;

    LOGV("[%d]%s", iPlayerIndex, __FUNCTION__);

    rc = NEXUS_SimpleVideoDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleVideoDecoder, &status);

    if (rc == NEXUS_SUCCESS) {
        playing = status.started;
        LOGV("[%d]%s: isPlaying=%d", iPlayerIndex, __FUNCTION__, status.started);
    }
    else {
        playing = false;
    }
#endif
    return playing;
}

int bcmPlayer_getCurrentPosition_hdmiIn(int iPlayerIndex, int *msec)
{
    *msec = 0;
    LOGV("[%d]%s: position=%dms", iPlayerIndex, __FUNCTION__, *msec);
    return 0;
}

static int bcmPlayer_getMediaExtractorFlags_hdmiIn(int iPlayerIndex, unsigned *flags)
{
    *flags = CAN_PAUSE;

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void player_reg_hdmiIn(bcmPlayer_func_t *pFuncs)
{
    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_hdmiIn;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_hdmiIn;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_hdmiIn;
    pFuncs->bcmPlayer_start_func = bcmPlayer_start_hdmiIn;
    pFuncs->bcmPlayer_isPlaying_func = bcmPlayer_isPlaying_hdmiIn;
    pFuncs->bcmPlayer_getCurrentPosition_func = bcmPlayer_getCurrentPosition_hdmiIn;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_hdmiIn;
}

