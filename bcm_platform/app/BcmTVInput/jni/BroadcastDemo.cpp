#include "BroadcastDemo.h"

class BroadcastDemo_Context {
public:
    NEXUS_FrontendHandle frontend;
    NEXUS_ParserBand parserBand;
#ifdef DECODE
    NEXUS_SimpleStcChannelHandle stcChannel;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
    NEXUS_PidChannelHandle pidChannel;
#endif
};

static struct {
    int id;
    const char *number;
    const char *name;
    int onid;
    int tsid;
    int sid;
    int freqKHz;
    int vpid;
} lineup[] = {
    { 0, "8", "8madrid", 0x22d4, 0x0027, 0x0f3d, 577000, 0x100 },
    { 1, "13", "13tv Madrid", 0x22d4, 0x0027, 0x0f3e, 577000, 0x200 },
    { 2, "800", "ASTROCANAL SHOP", 0x22d4, 0x0027, 0x0f43, 577000, 0x700 },
    { 3, "801", "Kiss TV", 0x22d4, 0x0027, 0x0f40, 577000, 0x401 },
    { 4, "802", "INTER TV", 0x22d4, 0x0027, 0x0f3f, 577000, 0x300 },
    { 5, "803", "MGustaTV", 0x22d4, 0x0027, 0x1392, 577000, 0x1000 },
    { -1, "", "", 0, 0, 0, 0, 0 }
};

static int BroadcastDemo_Tune(Tuner_Data *pTD, String8 s8id)
{
#ifdef DECODE
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
#endif
    NEXUS_FrontendOfdmSettings ofdmSettings;
    NEXUS_FrontendUserParameters userParams;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_VideoCodec video_codec = NEXUS_VideoCodec_eMpeg2;
    NEXUS_Error rc;
    int video_pid;

    BroadcastDemo_Context *context = (BroadcastDemo_Context *)pTD->context;

    int channel_id = strtoul(s8id.string(), 0, 0);
    unsigned i;
    for (i = 0; lineup[i].id >= 0; i++) {
        if (lineup[i].id == channel_id) {
            break;
        }
    }

    if (lineup[i].id < 0) {
        ALOGE("%s: channel_id %d invalid", __FUNCTION__, channel_id);
        return -1;
    }

    // Enable the tuner
    ALOGE("%s: Tuning on frequency %d...", __FUNCTION__, lineup[i].freqKHz);

    NEXUS_Frontend_GetDefaultOfdmSettings(&ofdmSettings);
    ofdmSettings.frequency = lineup[i].freqKHz * 1000;
    ofdmSettings.acquisitionMode = NEXUS_FrontendOfdmAcquisitionMode_eAuto;
    ofdmSettings.terrestrial = true;
    ofdmSettings.spectrum = NEXUS_FrontendOfdmSpectrum_eAuto;
    ofdmSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;
    NEXUS_Frontend_GetUserParameters(context->frontend, &userParams);
    NEXUS_ParserBand_GetSettings(context->parserBand, &parserBandSettings);

    if (userParams.isMtsif)
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eMtsif;
        parserBandSettings.sourceTypeSettings.mtsif = NEXUS_Frontend_GetConnector(context->frontend); /* NEXUS_Frontend_TuneXyz() will connect this frontend to this parser band */
    }

    else
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
        parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
    }

    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    rc = NEXUS_ParserBand_SetSettings(context->parserBand, &parserBandSettings);

    if (rc)
    {
        ALOGE("%s: ParserBand Setting failed", __FUNCTION__);
        return -1;
    }

    rc = NEXUS_Frontend_TuneOfdm(context->frontend, &ofdmSettings);
    if (rc)
    {
        ALOGE("%s: Frontend TuneOfdm failed", __FUNCTION__);
        return -1;
    }

#ifdef DECODE
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);

    // Set up the video PID
    video_pid = lineup[i].vpid;

    videoProgram.settings.pidChannel = NEXUS_PidChannel_Open(context->parserBand, video_pid, NULL);
    videoProgram.settings.codec = video_codec;

    if (videoProgram.settings.pidChannel)
    {
        rc = NEXUS_SimpleVideoDecoder_SetStcChannel(context->videoDecoder, context->stcChannel);
        if (rc)
        {
            ALOGE("%s: SetStcChannel failed", __FUNCTION__);
            return -1;
        }
    }

    if (videoProgram.settings.pidChannel)
    {
        context->pidChannel = videoProgram.settings.pidChannel;

        rc = NEXUS_SimpleVideoDecoder_Start(context->videoDecoder, &videoProgram);
        if (rc)
        {
            ALOGE("%s: VideoDecoderStart failed", __FUNCTION__);
            return -1;
        }
    }
#endif

    ALOGE("%s: Tuner has started streaming!!", __FUNCTION__);
    return 0;
}

static Vector<ChannelInfo> BroadcastDemo_GetChannelList(Tuner_Data *pTD)
{
    Vector<ChannelInfo> civ;
    ChannelInfo ci;
    unsigned i;

    BroadcastDemo_Context *context = (BroadcastDemo_Context *)pTD->context;

    ALOGE("%s: Enter", __FUNCTION__);
    for (i = 0; lineup[i].id >= 0; i++) {
        ci.id = String8::format("%u", lineup[i].id); 
        ci.name = lineup[i].name;
        ci.number = lineup[i].number;
        ci.onid = lineup[i].onid;
        ci.tsid = lineup[i].tsid;
        ci.sid = lineup[i].sid;
        civ.push_back(ci);
    }
    ALOGE("%s: Exit", __FUNCTION__);
    return civ;
}

static int BroadcastDemo_Stop(Tuner_Data *pTD)
{
    BroadcastDemo_Context *context = (BroadcastDemo_Context *)pTD->context;

#ifdef DECODE
    // Stop the video decoder
    NEXUS_SimpleVideoDecoder_Stop(context->videoDecoder);

    // Close the PID channel
    NEXUS_PidChannel_Close(context->pidChannel);
#endif

    return 0;
}

int
Broadcast_Initialize(Tuner_Data *pTD)
{
    BroadcastDemo_Context *context;
    NEXUS_FrontendAcquireSettings frontendAcquireSettings;

    ALOGE("%s: Enter", __FUNCTION__);

    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;

    context = new BroadcastDemo_Context;

#ifdef DECODE
    context->videoDecoder = pTD->ipcclient->acquireVideoDecoderHandle();

    pTD->ipcclient->getClientInfo(pTD->nexus_client, &client_info);

    // Now connect the client resources
    pTD->ipcclient->getDefaultConnectClientSettings(&connectSettings);

    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = 0;

    connectSettings.simpleVideoDecoder[0].windowCaps.type = eVideoWindowType_eMain;

    if (true != pTD->ipcclient->connectClientResources(pTD->nexus_client, &connectSettings))
        ALOGE("%s: connectClientResources failed", __FUNCTION__);

    b_video_window_settings window_settings;
    pTD->ipcclient->getVideoWindowSettings(pTD->nexus_client, 0, &window_settings);
    window_settings.visible = true;
    pTD->ipcclient->setVideoWindowSettings(pTD->nexus_client, 0, &window_settings);

    context->stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    context->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
    if (!context->stcChannel || !context->parserBand)
    {
        ALOGE("%s: Unable to create stcChannel or parserBand", __FUNCTION__);
        return -1;
    }
#endif

#if 0
    {
        ALOGE("%s: Faking unable to find OFDM-capable frontend", __FUNCTION__);
        return -1;
    }
#endif

    NEXUS_Frontend_GetDefaultAcquireSettings(&frontendAcquireSettings);
    frontendAcquireSettings.capabilities.ofdm = true;
    context->frontend = NEXUS_Frontend_Acquire(&frontendAcquireSettings);
    if (!context->frontend)
    {
        ALOGE("%s: Unable to find OFDM-capable frontend", __FUNCTION__);
        return -1;
    }

    pTD->context = context;
    pTD->GetChannelList = BroadcastDemo_GetChannelList;
    pTD->Tune = BroadcastDemo_Tune;
    pTD->Stop = BroadcastDemo_Stop;
    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

