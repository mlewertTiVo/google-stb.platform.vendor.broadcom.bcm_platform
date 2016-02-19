/******************************************************************************
 *    (c)2010-2016 Broadcom Corporation
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
#include "bcmsidebandhdmiplayer.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "BcmSidebandHdmiPlayer"
#include <cutils/log.h>
#define ARRAY_SIZE(x) ((int) (sizeof(x) / sizeof((x)[0])))

/* Please try to keep this file in sync with the hdmi_input test app. */

#if NEXUS_HAS_HDMI_INPUT && NEXUS_HAS_SIMPLE_DECODER
#include "nxclient.h"
#include "nexus_surface_client.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_hdmi_input.h"
#include "nexus_hdmi_input_hdcp.h"
#include "nexus_hdmi_output_hdcp.h"
#include "nexus_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include "bstd.h"
#include "bkni.h"
#include "nxapps_cmdline.h"
#include "namevalue.h"

//BDBG_MODULE(hdmi_input);

static void print_usage(const struct nxapps_cmdline *cmdline)
{
    printf(
        "Usage: hdmi_input\n"
        "  --help or -h for help\n"
        "  -index #\n"
        "  -prompt\n"
        "  -pip                     sets -rect and -zorder for picture-in-picture\n"
        "  -track_source            change display format to match HDMI input format\n"
        );
    nxapps_cmdline_print_usage(cmdline);
}

BcmSidebandHdmiPlayer::BcmSidebandHdmiPlayer(int port):
    mPortIndex(port),
    videoDecoder(NULL),
    audioDecoder(NULL)
{
    ALOGV("%s", __FUNCTION__);
    g_app.hdmiInput = NULL;
    g_app.hdmiOutput = NULL;
}

BcmSidebandHdmiPlayer::~BcmSidebandHdmiPlayer()
{
    ALOGV("%s", __FUNCTION__);
}

/* changing output params to match input params is not required */
static void source_changed(void *context, int param)
{
    ALOGV("%s", __FUNCTION__);
    BcmSidebandHdmiPlayer *player = (BcmSidebandHdmiPlayer*)context;
    player->sourceChanged(NULL, param);
}
void BcmSidebandHdmiPlayer::sourceChanged(void *context, int param)
{
    ALOGV("%s", __FUNCTION__);
    NEXUS_Error rc;
    NEXUS_HdmiInputStatus hdmiInputStatus;
    NxClient_DisplaySettings displaySettings;

    BSTD_UNUSED(context);
    BSTD_UNUSED(param);

    NxClient_GetDisplaySettings(&displaySettings);
    NEXUS_HdmiInput_GetStatus(g_app.hdmiInput, &hdmiInputStatus);
    if (!hdmiInputStatus.validHdmiStatus) {
        displaySettings.hdmiPreferences.hdcp = NxClient_HdcpLevel_eNone;
    }
    else {
        NEXUS_HdmiOutputStatus hdmiOutputStatus;
        NEXUS_HdmiOutput_GetStatus(g_app.hdmiOutput, &hdmiOutputStatus);
        if (displaySettings.format != hdmiInputStatus.originalFormat && hdmiOutputStatus.videoFormatSupported[hdmiInputStatus.originalFormat]) {
            ALOGW("video format %s to %s", lookup_name(g_videoFormatStrs, displaySettings.format),
                lookup_name(g_videoFormatStrs, hdmiInputStatus.originalFormat));
            displaySettings.format = hdmiInputStatus.originalFormat;
        }

        if (hdmiInputStatus.colorSpace != displaySettings.hdmiPreferences.colorSpace)
        {
            ALOGW("color space %s -> %s", lookup_name(g_colorSpaceStrs, displaySettings.hdmiPreferences.colorSpace),
                lookup_name(g_colorSpaceStrs, hdmiInputStatus.colorSpace));
            displaySettings.hdmiPreferences.colorSpace = hdmiInputStatus.colorSpace;
        }
        if (hdmiInputStatus.colorDepth != displaySettings.hdmiPreferences.colorDepth) {
            ALOGW("color depth %u -> %u", displaySettings.hdmiPreferences.colorDepth, hdmiInputStatus.colorDepth);
            displaySettings.hdmiPreferences.colorDepth = hdmiInputStatus.colorDepth;
        }
    }
    rc = NxClient_SetDisplaySettings(&displaySettings);
    if (rc) {
        ALOGE("Unable to set Display Settings (errCode= %d)", rc);
    }
}

static void hdmiInputHdcpStateChanged_callback(void *context, int param)
{
    ALOGV("%s", __FUNCTION__);
    BcmSidebandHdmiPlayer *player = (BcmSidebandHdmiPlayer*)context;
    player->hdmiInputHdcpStateChanged(NULL, param);
}
void BcmSidebandHdmiPlayer::hdmiInputHdcpStateChanged(void *context, int param)
{
    ALOGV("%s", __FUNCTION__);
    NEXUS_HdmiInputHdcpStatus hdmiInputHdcpStatus;
    NEXUS_HdmiOutputHdcpStatus outputHdcpStatus;
    NEXUS_Error rc;
    bool hdcp = false;

    BSTD_UNUSED(context);
    BSTD_UNUSED(param);

    /* check the authentication state and process accordingly */
    rc = NEXUS_HdmiInput_HdcpGetStatus(g_app.hdmiInput, &hdmiInputHdcpStatus);
    ALOG_ASSERT(!rc);

    switch (hdmiInputHdcpStatus.eAuthState) {
    case NEXUS_HdmiInputHdcpAuthState_eKeysetInitialization :
        ALOGW("Change in HDCP Key Set detected: %u", hdmiInputHdcpStatus.eKeyStorage);
        break;

    case NEXUS_HdmiInputHdcpAuthState_eWaitForKeyloading :
        /* TODO: nxclient lacks ability for client to re-authenticate output if already authenticated.
        must add new function. */
        ALOGW("Upstream HDCP Authentication request ...");
        hdcp = true;
        break;

    case NEXUS_HdmiInputHdcpAuthState_eWaitForDownstreamKsvs :
        ALOGW("Downstream FIFO Request; Start hdmiOutput Authentication...");
        NEXUS_HdmiOutput_GetHdcpStatus(g_app.hdmiOutput, &outputHdcpStatus);
        if ((outputHdcpStatus.hdcpState != NEXUS_HdmiOutputHdcpState_eWaitForRepeaterReady)
            && (outputHdcpStatus.hdcpState != NEXUS_HdmiOutputHdcpState_eCheckForRepeaterReady))
        {
            hdcp = true;
        }
        break;

    default:
        ALOGW("Unknown State %d", hdmiInputHdcpStatus.eAuthState);
        break;
    }

    if (hdcp) {
        NxClient_DisplaySettings displaySettings;
        NxClient_GetDisplaySettings(&displaySettings);
        if (displaySettings.hdmiPreferences.hdcp != NxClient_HdcpLevel_eMandatory) {
            displaySettings.hdmiPreferences.hdcp = NxClient_HdcpLevel_eMandatory;
            rc = NxClient_SetDisplaySettings(&displaySettings);
            ALOG_ASSERT(!rc);
        }
    }
}

void BcmSidebandHdmiPlayer::hotplug_callback()
{
    ALOGV("%s", __FUNCTION__);
    NEXUS_HdmiOutputStatus status;
    NEXUS_HdmiOutputBasicEdidData hdmiOutputBasicEdidData;
    NEXUS_HdmiOutputEdidBlock edidBlock;
    uint8_t *attachedRxEdid = NULL;
    uint16_t attachedRxEdidSize;
    unsigned i, j;
    NEXUS_Error rc;

    NEXUS_HdmiOutput_GetStatus(g_app.hdmiOutput, &status);
    ALOGI("hotplug_callback: %s\n", status.connected ? "DEVICE CONNECTED" : "DEVICE REMOVED");

    if ( !status.connected ) {
        /* device disconnected. Load internal EDID. */
        rc = NEXUS_HdmiInput_LoadEdidData(g_app.hdmiInput, NULL, 0);
        ALOG_ASSERT(!rc);
        return;
    }

    /* Get EDID of attached receiver*/
    rc = NEXUS_HdmiOutput_GetBasicEdidData(g_app.hdmiOutput, &hdmiOutputBasicEdidData);
    if (rc) {
        ALOGE("Unable to get downstream EDID; Use declared EDID in app for repeater's EDID");
        goto load_edid;
    }

    /* allocate space to hold the EDID blocks */
    attachedRxEdidSize = (hdmiOutputBasicEdidData.extensions + 1) * sizeof(edidBlock.data);
    attachedRxEdid = new uint8_t[attachedRxEdidSize];
    if (!attachedRxEdid) {
        ALOG_ASSERT(attachedRxEdid);
        goto load_edid;
    }
    for (i = 0; i <= hdmiOutputBasicEdidData.extensions; i++) {
        rc = NEXUS_HdmiOutput_GetEdidBlock(g_app.hdmiOutput, i, &edidBlock);
        if (rc) {
            ALOGE("Error retrieving EDID Block %d from attached receiver;", i);
            delete attachedRxEdid;
            attachedRxEdid = NULL;
            goto load_edid;
        }
        for (j=0; j < sizeof(edidBlock.data); j++) {
            attachedRxEdid[i*sizeof(edidBlock.data)+j] = edidBlock.data[j];
        }
    }

load_edid:
    /* TODO: manipulate EDID to add/remove capabilities */
    rc = NEXUS_HdmiInput_LoadEdidData(g_app.hdmiInput, attachedRxEdid, attachedRxEdidSize);
    ALOG_ASSERT(!rc);
    if (attachedRxEdid) {
        delete attachedRxEdid;
    }

    ALOGW("Toggle Rx HOT PLUG to force upstream re-authentication...");
    NEXUS_HdmiInput_ToggleHotPlug(g_app.hdmiInput);
}


void BcmSidebandHdmiPlayer::hdmiOutputHdcpOutputChanged_callback()
{
    NEXUS_Error rc ;
    NEXUS_HdmiOutputHdcpStatus hdmiOutputHdcpStatus;
    NEXUS_HdmiHdcpDownStreamInfo downStream  ;
    NEXUS_HdmiHdcpKsv *pKsvs ;
    unsigned returnedDevices ;
    uint8_t i ;

    rc = NEXUS_HdmiOutput_GetHdcpStatus(g_app.hdmiOutput, &hdmiOutputHdcpStatus);
    ALOG_ASSERT(!rc);

    if (hdmiOutputHdcpStatus.hdcpError)
    {
        ALOGE("HdmiOutput HDCP Error: %d", hdmiOutputHdcpStatus.hdcpError) ;
        ALOG_ASSERT(!hdmiOutputHdcpStatus.hdcpError) ;
        return ;
    }

    NEXUS_HdmiOutput_HdcpGetDownstreamInfo(g_app.hdmiOutput, &downStream) ;

    /* allocate space to hold ksvs for the downstream devices */
    pKsvs =
        new NEXUS_HdmiHdcpKsv[(downStream.devices) * NEXUS_HDMI_HDCP_KSV_LENGTH] ;

    NEXUS_HdmiOutput_HdcpGetDownstreamKsvs(g_app.hdmiOutput,
        pKsvs, downStream.devices, &returnedDevices) ;

    ALOGD("hdmiOutput Downstream Levels:  %d  Devices: %d",
        downStream.depth, downStream.devices) ;

    /* display the downstream device KSVs */
    for (i = 0 ; i <= downStream.devices; i++)
   {
        ALOGD("Device %02d BKsv: %02X %02X %02X %02X %02X",
            i + 1,
            *(pKsvs->data + i + 4), *(pKsvs->data + i + 3),
            *(pKsvs->data + i + 2), *(pKsvs->data + i + 1),
            *(pKsvs->data + i )) ;
    }

    NEXUS_HdmiInput_HdcpLoadKsvFifo(g_app.hdmiInput,
        &downStream, pKsvs, downStream.devices) ;

    delete pKsvs ;
}


static void nxclient_callback(void *context, int param)
{
    ALOGV("%s", __FUNCTION__);
    BcmSidebandHdmiPlayer *player = (BcmSidebandHdmiPlayer*)context;
    switch (param) {
    case 0:
        player->hotplug_callback();
        break;
    case 2:
        player->hdmiOutputHdcpOutputChanged_callback() ;
        break;

    case 1:
        break;
    }
}

int BcmSidebandHdmiPlayer::start(int x, int y, int w, int h)
{
    ALOGV("%s", __FUNCTION__);
    char argv0[] = LOG_TAG;
    char argv1[] = "-rect";
    char argv2[100];
    sprintf(argv2, "%d,%d,%d,%d", x, y, w, h);
    const char *argv[] = {&argv0[0], &argv1[0], &argv2[0], NULL};
    int argc = ARRAY_SIZE(argv) - 1;
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    //NxClient_AllocResults allocResults;
    NxClient_ConnectSettings connectSettings;
    NEXUS_SurfaceClientHandle surfaceClient, videoSurfaceClient;
    //NEXUS_SimpleVideoDecoderHandle videoDecoder;
    //NEXUS_SimpleAudioDecoderHandle audioDecoder;
    NEXUS_HdmiInputSettings hdmiInputSettings;
    NEXUS_HdmiInputHdcpSettings hdmiInputHdcpSettings;
    NxClient_CallbackThreadSettings callbackThreadSettings;
    //unsigned connectId;
    int curarg = 1;
    int rc;
    bool prompt = false;
    unsigned index = 0;
    struct nxapps_cmdline cmdline;
    int n;
    NxClient_VideoWindowType videoWindowType = NxClient_VideoWindowType_eMain;
    bool track_source = false;

    nxapps_cmdline_init(&cmdline);
    nxapps_cmdline_allow(&cmdline, nxapps_cmdline_type_SurfaceComposition);

    while (curarg < argc) {
        if (!strcmp(argv[curarg], "-h") || !strcmp(argv[curarg], "--help")) {
            print_usage(&cmdline);
            return 0;
        }
        else if (!strcmp(argv[curarg], "-prompt")) {
            prompt = true;
        }
        else if (!strcmp(argv[curarg], "-index") && curarg+1 < argc) {
            index = atoi(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-pip")) {
            const char *argv[] = {"-rect","960,0,960,540","-zorder","1"};
            nxapps_cmdline_parse(0, 2, argv, &cmdline);
            nxapps_cmdline_parse(2, 4, argv, &cmdline);
            videoWindowType = NxClient_VideoWindowType_ePip;
        }
        else if (!strcmp(argv[curarg], "-track_source")) {
            track_source = true;
        }
        else if ((n = nxapps_cmdline_parse(curarg, argc, argv, &cmdline))) {
            if (n < 0) {
                print_usage(&cmdline);
                return -1;
            }
            curarg += n;
        }
        else {
            print_usage(&cmdline);
            return -1;
        }
        curarg++;
    }

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", argv[0]);
    rc = NxClient_Join(&joinSettings);
    if (rc) {
        printf("cannot join: %d\n", rc);
        return -1;
    }

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleVideoDecoder = 1;
    allocSettings.simpleAudioDecoder = 1;
    allocSettings.surfaceClient = 1; /* surface client required for video window */
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) {ALOGW("unable to alloc transcode resources"); return -1;}

    if (allocResults.simpleVideoDecoder[0].id) {
        videoDecoder = NEXUS_SimpleVideoDecoder_Acquire(allocResults.simpleVideoDecoder[0].id);
    }
    if (allocResults.simpleAudioDecoder.id) {
        audioDecoder = NEXUS_SimpleAudioDecoder_Acquire(allocResults.simpleAudioDecoder.id);
    }
    if (allocResults.surfaceClient[0].id) {
        /* surfaceClient is the top-level graphics window in which video will fit.
        videoSurfaceClient must be "acquired" to associate the video window with surface compositor.
        Graphics do not have to be submitted to surfaceClient for video to work, but at least an
        "alpha hole" surface must be submitted to punch video through other client's graphics.
        Also, the top-level surfaceClient ID must be submitted to NxClient_ConnectSettings below. */
        surfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
        videoSurfaceClient = NEXUS_SurfaceClient_AcquireVideoWindow(surfaceClient, 0);

        if (nxapps_cmdline_is_set(&cmdline, nxapps_cmdline_type_SurfaceComposition)) {
            NEXUS_SurfaceComposition comp;
            NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
            nxapps_cmdline_apply_SurfaceComposition(&cmdline, &comp);
            NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
        }
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = allocResults.simpleVideoDecoder[0].id;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = allocResults.surfaceClient[0].id;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.connectType = NxClient_VideoDecoderConnectType_eWindowOnly;
    connectSettings.simpleVideoDecoder[0].windowCapabilities.type = videoWindowType;
    connectSettings.simpleAudioDecoder.id = allocResults.simpleAudioDecoder.id;
    rc = NxClient_Connect(&connectSettings, &connectId);
    ALOG_ASSERT(!rc);

    mSurfaceClientId = allocResults.surfaceClient[0].id;

    NEXUS_HdmiInput_GetDefaultSettings(&hdmiInputSettings);
    /* TODO: nexus must set timebase source to eHdDvi */
    hdmiInputSettings.frontend.hpdDisconnected = false;
    g_app.hdmiInput = NEXUS_HdmiInput_Open(index, &hdmiInputSettings);
    if (!g_app.hdmiInput) {
        ALOGE("HdmiInput %d not available", index);
        return -1;
    }

    /* open a read-only alias to get EDID. any changes must go through nxclient. */
    g_app.hdmiOutput = NEXUS_HdmiOutput_Open(0 + NEXUS_ALIAS_ID, NULL);
    if (!g_app.hdmiOutput) {
        ALOGE("Can't get hdmi output read-only alias");
        return -1;
    }

    if (track_source) {
        NEXUS_HdmiInput_GetSettings(g_app.hdmiInput, &hdmiInputSettings);
        hdmiInputSettings.sourceChanged.callback = source_changed;
        hdmiInputSettings.sourceChanged.context = this;
        rc = NEXUS_HdmiInput_SetSettings(g_app.hdmiInput, &hdmiInputSettings);
        ALOG_ASSERT(!rc);
    }

    NEXUS_HdmiInput_HdcpGetDefaultSettings(g_app.hdmiInput, &hdmiInputHdcpSettings);
    /* chips with both hdmi rx and tx cores should always set repeater to TRUE */
    hdmiInputHdcpSettings.repeater = true;
    hdmiInputHdcpSettings.hdcpRxChanged.callback = hdmiInputHdcpStateChanged_callback;
    hdmiInputHdcpSettings.hdcpRxChanged.context = this;
    rc = NEXUS_HdmiInput_HdcpSetSettings(g_app.hdmiInput, &hdmiInputHdcpSettings);
    ALOG_ASSERT(!rc);

    NxClient_GetDefaultCallbackThreadSettings(&callbackThreadSettings);
    callbackThreadSettings.hdmiOutputHotplug.callback = nxclient_callback;
    callbackThreadSettings.hdmiOutputHotplug.context = this;
    callbackThreadSettings.hdmiOutputHotplug.param = 0;
    callbackThreadSettings.hdmiOutputHdcpChanged.callback = nxclient_callback;
    callbackThreadSettings.hdmiOutputHdcpChanged.context = this;
    callbackThreadSettings.hdmiOutputHdcpChanged.param = 2;
    callbackThreadSettings.displaySettingsChanged.callback = nxclient_callback;
    callbackThreadSettings.displaySettingsChanged.context = this;
    callbackThreadSettings.displaySettingsChanged.param = 1;
    rc = NxClient_StartCallbackThread(&callbackThreadSettings);
    ALOG_ASSERT(!rc);

    rc = NEXUS_SimpleVideoDecoder_StartHdmiInput(videoDecoder, g_app.hdmiInput, NULL);
    ALOG_ASSERT(!rc);
    rc = NEXUS_SimpleAudioDecoder_StartHdmiInput(audioDecoder, g_app.hdmiInput, NULL);
    ALOG_ASSERT(!rc);

    ALOGI("HdmiInput %d active", index);
    mState = STATE_PLAYING;
    return rc;

cleanup:
    reset();
    return rc;
}

void BcmSidebandHdmiPlayer::stop()
{
    ALOGV("%s", __FUNCTION__);
    NEXUS_SimpleAudioDecoder_StopHdmiInput(audioDecoder);
    NEXUS_SimpleVideoDecoder_StopHdmiInput(videoDecoder);
    reset();
}

void BcmSidebandHdmiPlayer::reset()
{
    ALOGV("%s", __FUNCTION__);
    NxClient_StopCallbackThread();
    NEXUS_HdmiOutput_Close(g_app.hdmiOutput);
    NEXUS_HdmiInput_Close(g_app.hdmiInput);
    NxClient_Disconnect(connectId);
    NxClient_Free(&allocResults);
    NxClient_Uninit();
    mState = STATE_IDLE;
}
#else
int BcmSidebandFilePlayer::start(int, int, int, int)
{
    ALOGE("This application is not supported on this platform (needs hdmi_input and simple_decoder)!");
    return 0;
}
#endif
