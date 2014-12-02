#include "TunerHAL.h"

class BroadcastDemo_Context {
public:
    BroadcastDemo_Context() {
        frontend = NULL;
        parserBand = NEXUS_ParserBand_eInvalid;
        videoDecoder = NULL;
        stcChannel = NULL;
        connected = false;
        pidChannel = NULL;
        decoding = false;
    };
    NEXUS_FrontendHandle frontend;
    NEXUS_ParserBand parserBand;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
    NEXUS_SimpleStcChannelHandle stcChannel;
    bool connected;
    NEXUS_PidChannelHandle pidChannel;
    bool decoding;
};

BroadcastDemo_Context *pSelf;

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
    { 0, "8", "8madrid", 0x22d4, 0x0027, 0x0f3d, 618000, 0x100 },
    { 1, "13", "13tv Madrid", 0x22d4, 0x0027, 0x0f3e, 618000, 0x200 },
    { 2, "800", "ASTROCANAL SHOP", 0x22d4, 0x0027, 0x0f43, 577000, 0x700 },
    { 3, "801", "Kiss TV", 0x22d4, 0x0027, 0x0f40, 618000, 0x401 },
    { 4, "802", "INTER TV", 0x22d4, 0x0027, 0x0f3f, 618000, 0x300 },
    { 5, "803", "MGustaTV", 0x22d4, 0x0027, 0x1392, 618000, 0x1000 },
    { -1, "", "", 0, 0, 0, 0, 0 }
};

static void
FreeContext()
{
    if (pSelf) {
        if (pSelf->parserBand != NEXUS_ParserBand_eInvalid) {
            NEXUS_ParserBand_Close(pSelf->parserBand);
            pSelf->parserBand = NEXUS_ParserBand_eInvalid;
        }
        if (pSelf->frontend) {
            NEXUS_Frontend_Release(pSelf->frontend);
            pSelf->frontend = NULL;
        }
        if (pSelf->videoDecoder) {
            TunerHAL_getIPCClient()->releaseVideoDecoderHandle(pSelf->videoDecoder);
            pSelf->videoDecoder = NULL;
        }
        if (pSelf->stcChannel) {
            NEXUS_SimpleStcChannel_Destroy(pSelf->stcChannel);
            pSelf->stcChannel = NULL;
        }
        delete pSelf;
        pSelf = 0; 
    }
}

static int
Connect()
{

    if (!pSelf->connected) {
        b_refsw_client_client_info client_info;
        b_refsw_client_connect_resource_settings connectSettings;

        ALOGE("%s: connecting", __FUNCTION__);

        TunerHAL_getIPCClient()->getClientInfo(TunerHAL_getClientContext(), &client_info);

        // Now connect the client resources
        TunerHAL_getIPCClient()->getDefaultConnectClientSettings(&connectSettings);

        connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
        connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
        connectSettings.simpleVideoDecoder[0].windowId = 0;

        connectSettings.simpleVideoDecoder[0].windowCaps.type = eVideoWindowType_eMain;

        if (true != TunerHAL_getIPCClient()->connectClientResources(TunerHAL_getClientContext(), &connectSettings)) {
            ALOGE("%s: connectClientResources failed", __FUNCTION__);
            return -1;
        }

        b_video_window_settings window_settings;
        TunerHAL_getIPCClient()->getVideoWindowSettings(TunerHAL_getClientContext(), 0, &window_settings);
        window_settings.visible = true;
        window_settings.position.x = 0;
        window_settings.position.y = 0;
        window_settings.position.width = window_settings.virtualDisplay.width;
        window_settings.position.height = window_settings.virtualDisplay.height;
        TunerHAL_getIPCClient()->setVideoWindowSettings(TunerHAL_getClientContext(), 0, &window_settings);
        pSelf->connected = true;

        ALOGE("%s: connected", __FUNCTION__);
    }
    return 0;
}

static void
Disconnect()
{
    if (pSelf->connected) {
        ALOGE("%s: disconnecting", __FUNCTION__);

        TunerHAL_getIPCClient()->disconnectClientResources(TunerHAL_getClientContext());
        pSelf->connected = false;

        ALOGE("%s: disconnected", __FUNCTION__);
    }
}

static int BroadcastDemo_Stop()
{
    ALOGE("%s: Enter", __FUNCTION__);

    // Stop the video decoder
    if (pSelf->decoding) {
        NEXUS_SimpleVideoDecoder_Stop(pSelf->videoDecoder);
        pSelf->decoding = false;
    }

    // Close the PID channel
    if (pSelf->pidChannel) {
        NEXUS_PidChannel_Close(pSelf->pidChannel);
        pSelf->pidChannel = 0;
    }

    ALOGE("%s: Exit", __FUNCTION__);

    return 0;
}

static int BroadcastDemo_Tune(String8 s8id)
{
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_FrontendOfdmSettings ofdmSettings;
    NEXUS_FrontendUserParameters userParams;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_VideoCodec video_codec = NEXUS_VideoCodec_eMpeg2;
    NEXUS_Error rc;
    int video_pid;

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

    if (pSelf->decoding) {
        BroadcastDemo_Stop();
    }

    // Enable the tuner
    ALOGE("%s: Tuning on frequency %d...", __FUNCTION__, lineup[i].freqKHz);

    NEXUS_Frontend_GetDefaultOfdmSettings(&ofdmSettings);
    ofdmSettings.frequency = lineup[i].freqKHz * 1000;
    ofdmSettings.acquisitionMode = NEXUS_FrontendOfdmAcquisitionMode_eAuto;
    ofdmSettings.terrestrial = true;
    ofdmSettings.spectrum = NEXUS_FrontendOfdmSpectrum_eAuto;
    ofdmSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;
    NEXUS_Frontend_GetUserParameters(pSelf->frontend, &userParams);
    NEXUS_ParserBand_GetSettings(pSelf->parserBand, &parserBandSettings);

    if (userParams.isMtsif)
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eMtsif;
        parserBandSettings.sourceTypeSettings.mtsif = NEXUS_Frontend_GetConnector(pSelf->frontend); /* NEXUS_Frontend_TuneXyz() will connect this frontend to this parser band */
    }

    else
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
        parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
    }

    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    rc = NEXUS_ParserBand_SetSettings(pSelf->parserBand, &parserBandSettings);

    if (rc)
    {
        ALOGE("%s: ParserBand Setting failed", __FUNCTION__);
        return -1;
    }

    rc = NEXUS_Frontend_TuneOfdm(pSelf->frontend, &ofdmSettings);
    if (rc)
    {
        ALOGE("%s: Frontend TuneOfdm failed", __FUNCTION__);
        return -1;
    }

    if (Connect() < 0) {
        ALOGE("%s: Failed to connect", __FUNCTION__);
        return -1;
    }

    rc = NEXUS_SimpleVideoDecoder_SetStcChannel(pSelf->videoDecoder, pSelf->stcChannel);
    if (rc)
    {
        ALOGE("%s: SetStcChannel failed", __FUNCTION__);
        return -1;
    }

    // Set up the video PID
    video_pid = lineup[i].vpid;
    pSelf->pidChannel = NEXUS_PidChannel_Open(pSelf->parserBand, video_pid, NULL);
    if (pSelf->pidChannel == NULL) {
        ALOGE("%s: Failed to open pidchannel", __FUNCTION__);
        return -1;
    }

    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram); 
    videoProgram.settings.pidChannel = pSelf->pidChannel;
    videoProgram.settings.codec = video_codec;
    rc = NEXUS_SimpleVideoDecoder_Start(pSelf->videoDecoder, &videoProgram);
    if (rc)
    {
        ALOGE("%s: VideoDecoderStart failed", __FUNCTION__);
        NEXUS_PidChannel_Close(pSelf->pidChannel);
        pSelf->pidChannel = 0;
        return -1;
    }

    pSelf->decoding = true;

    ALOGE("%s: Tuner has started streaming!!", __FUNCTION__);
    return 0;
}

static Vector<BroadcastChannelInfo>
BroadcastDemo_GetChannelList()
{
    Vector<BroadcastChannelInfo> civ;
    BroadcastChannelInfo ci;
    unsigned i;

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

static Vector<BroadcastProgramInfo>
BroadcastDemo_GetProgramList(String8)
{
    Vector<BroadcastProgramInfo> v;
    return v;
}

static int
BroadcastDemo_Release()
{
    ALOGE("%s: Enter", __FUNCTION__);

    BroadcastDemo_Stop();
    Disconnect();

    ALOGE("%s: Exit", __FUNCTION__);

    return 0;
}

int
Broadcast_Initialize(BroadcastDriver *pD)
{
    pSelf = new BroadcastDemo_Context;

    NEXUS_FrontendAcquireSettings frontendAcquireSettings;

    ALOGE("%s: Enter", __FUNCTION__);

#if 0
    {
        ALOGE("%s: Faking unable to find OFDM-capable frontend", __FUNCTION__);
        return -1;
    }
#endif

    int rv = 0;

    NEXUS_Frontend_GetDefaultAcquireSettings(&frontendAcquireSettings);
    frontendAcquireSettings.capabilities.ofdm = true;
    pSelf->frontend = NEXUS_Frontend_Acquire(&frontendAcquireSettings);
    if (!pSelf->frontend)
    {
        ALOGE("%s: Unable to find OFDM-capable frontend", __FUNCTION__);
        rv = -1;
    }

    if (rv == 0) {
        pSelf->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
        if (pSelf->parserBand == NEXUS_ParserBand_eInvalid) {
            ALOGE("%s: Unable to open parserBand", __FUNCTION__);
            rv = -1;
        }
    }

    if (rv == 0) {
        pSelf->videoDecoder = TunerHAL_getIPCClient()->acquireVideoDecoderHandle();
        if (pSelf->videoDecoder == NULL) {
            ALOGE("%s: Unable to acquire videoDecoder", __FUNCTION__);
            rv = -1;
        }
    }

    if (rv == 0) {
        pSelf->stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
        if (!pSelf->stcChannel)
        {
            ALOGE("%s: Unable to create stcChannel", __FUNCTION__);
            rv = -1;
        }
    }

    if (rv < 0) {
        FreeContext();
        return rv;
    }

    pD->GetChannelList = BroadcastDemo_GetChannelList;
    pD->GetProgramList = BroadcastDemo_GetProgramList;
    pD->Tune = BroadcastDemo_Tune;
    pD->Stop = BroadcastDemo_Stop;
    pD->Release = BroadcastDemo_Release;

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

