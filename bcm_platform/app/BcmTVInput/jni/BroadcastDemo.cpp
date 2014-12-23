#include <stdlib.h>
#include "Broadcast.h"
#include "nexus_frontend.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_stc_channel.h"
#include "nexus_pid_channel.h"
#include "nexus_parser_band.h"
#include "nxclient.h"
#include "nexus_surface_client.h"

class BroadcastDemo_Context {
public:
    BroadcastDemo_Context() {
        frontend = NULL;
        parserBand = NEXUS_ParserBand_eInvalid;
        m_hSimpleVideoDecoder = NULL;
        stcChannel = NULL;
        connected = false;
        pidChannel = NULL;
        decoding = false;
    };
    NEXUS_FrontendHandle frontend;
    NEXUS_ParserBand parserBand;
    NEXUS_SimpleVideoDecoderHandle m_hSimpleVideoDecoder;
    NEXUS_SimpleStcChannelHandle stcChannel;
    bool connected;
    NEXUS_PidChannelHandle pidChannel;
    bool decoding;
    /**/
    NxClient_AllocResults            m_allocResults;
    unsigned                         m_nxClientId;
    NEXUS_SurfaceClientHandle        m_hSurfaceClient;
    NEXUS_SurfaceClientHandle        m_hVideoClient;
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
        if (pSelf->stcChannel) {
            NEXUS_SimpleStcChannel_Destroy(pSelf->stcChannel);
            pSelf->stcChannel = NULL;
        }
        delete pSelf;
        pSelf = 0; 
    }
}


static void
SetGeometry()
{
    if ( pSelf && pSelf->m_hSurfaceClient ) {
        NEXUS_SurfaceComposition composition;
        NEXUS_SurfaceClientSettings settings;
        NEXUS_Error errCode;

        NxClient_GetSurfaceClientComposition(pSelf->m_allocResults.surfaceClient[0].id, &composition);
        //composition.virtualDisplay.width = pPosition->width;
        //composition.virtualDisplay.height = pPosition->height;
        //composition.position = *pPosition;
        //composition.zorder = zorder;
        //composition.visible = visible;
        composition.visible = true;
        composition.position.x = 0;
        composition.position.y = 0;
        composition.position.width = composition.virtualDisplay.width;
        composition.position.height = composition.virtualDisplay.height;
        //composition.contentMode = NEXUS_VideoWindowContentMode_eBox;
        errCode = NxClient_SetSurfaceClientComposition(pSelf->m_allocResults.surfaceClient[0].id, &composition);
        if ( errCode )
        {
            ALOGE("%s: NxClient_SetSurfaceClientComposition failed (%d)", __FUNCTION__, errCode);
            return;
        }

        NEXUS_SurfaceClient_GetSettings(pSelf->m_hVideoClient, &settings);
        settings.composition = composition;
        //settings.composition.clipRect = *pClipRect; // Add in video source clipping
        errCode = NEXUS_SurfaceClient_SetSettings(pSelf->m_hVideoClient, &settings);
        if ( errCode )
        {
            ALOGE("%s: NEXUS_SurfaceClient_SetSettings failed(%d)", __FUNCTION__, errCode);
            return;
        }

#if 0
        if ( !m_setSurface )
        {
            errCode = NEXUS_SurfaceClient_SetSurface(m_hSurfaceClient, m_hAlphaSurface);
            if ( errCode )
            {
                (void)BOMX_BERR_TRACE(errCode);
                return;
            }
            m_setSurface = true;
        }
#endif

    }
}

static int
Connect()
{

    if (!pSelf->connected) {
        ALOGE("%s: connecting", __FUNCTION__);

        NxClient_ConnectSettings connectSettings;
        NEXUS_Error errCode;

        NxClient_GetDefaultConnectSettings(&connectSettings);
        connectSettings.simpleVideoDecoder[0].id = pSelf->m_allocResults.simpleVideoDecoder[0].id;
        connectSettings.simpleVideoDecoder[0].surfaceClientId = pSelf->m_allocResults.surfaceClient[0].id;
        connectSettings.simpleVideoDecoder[0].windowId = 0;
        connectSettings.simpleVideoDecoder[0].windowCapabilities.type = NxClient_VideoWindowType_eMain; // TODO: Support Main/Pip
        connectSettings.simpleVideoDecoder[0].decoderCapabilities.supportedCodecs[NEXUS_VideoCodec_eMpeg2] = true;
        errCode = NxClient_Connect(&connectSettings, &pSelf->m_nxClientId);
        if ( errCode )
        {
            ALOGE("%s: connect failed (%d)", __FUNCTION__, errCode);
            return -1;
        }

        pSelf->m_hSimpleVideoDecoder = NEXUS_SimpleVideoDecoder_Acquire(pSelf->m_allocResults.simpleVideoDecoder[0].id);
        if ( NULL == pSelf->m_hSimpleVideoDecoder )
        {
            NxClient_Disconnect(pSelf->m_nxClientId);
            ALOGE("%s: SimpleVideoDecoder_Acquire failed", __FUNCTION__);
            return -1;
        }

        SetGeometry();

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

        if (pSelf->m_hSimpleVideoDecoder) {
            NEXUS_SimpleVideoDecoder_Release(pSelf->m_hSimpleVideoDecoder);
            pSelf->m_hSimpleVideoDecoder = NULL;
        }

        NxClient_Disconnect(pSelf->m_nxClientId);

        pSelf->connected = false;

        ALOGE("%s: disconnected", __FUNCTION__);
    }
}

static int BroadcastDemo_Stop()
{
    ALOGE("%s: Enter", __FUNCTION__);

    // Stop the video decoder
    if (pSelf->decoding) {
        NEXUS_SimpleVideoDecoder_Stop(pSelf->m_hSimpleVideoDecoder);
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

static int BroadcastDemo_StartBlindScan()
{
    ALOGE("%s: Enter", __FUNCTION__);
    ALOGE("%s: Exit", __FUNCTION__);
    return -1;
}

static int BroadcastDemo_StopScan()
{
    ALOGE("%s: Enter", __FUNCTION__);
    ALOGE("%s: Exit", __FUNCTION__);
    return -1;
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

    rc = NEXUS_SimpleVideoDecoder_SetStcChannel(pSelf->m_hSimpleVideoDecoder, pSelf->stcChannel);
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
    rc = NEXUS_SimpleVideoDecoder_Start(pSelf->m_hSimpleVideoDecoder, &videoProgram);
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
        NxClient_AllocSettings nxAllocSettings;
        NEXUS_Error errCode;
        NxClient_GetDefaultAllocSettings(&nxAllocSettings);
        nxAllocSettings.simpleVideoDecoder = 1;
        nxAllocSettings.surfaceClient = 1;
        errCode = NxClient_Alloc(&nxAllocSettings, &pSelf->m_allocResults);
        if ( errCode )
        {
            ALOGE("%s: NxClient_Alloc failed (%d)", __FUNCTION__, errCode);
            rv = -1;
        }
        if (rv == 0) {
            pSelf->m_hSurfaceClient = NEXUS_SurfaceClient_Acquire(pSelf->m_allocResults.surfaceClient[0].id);
            if ( NULL == pSelf->m_hSurfaceClient )
            {
                ALOGE("%s: Unable to acquire surface client", __FUNCTION__);
                NxClient_Free(&pSelf->m_allocResults);
                rv = -1;
            }
        }
        if (rv == 0) {
            pSelf->m_hVideoClient = NEXUS_SurfaceClient_AcquireVideoWindow(pSelf->m_hSurfaceClient, 0);
            if ( NULL == pSelf->m_hVideoClient )
            {
                ALOGE("%s: Unable to acquire video client", __FUNCTION__);
                NEXUS_SurfaceClient_Release(pSelf->m_hSurfaceClient);
                pSelf->m_hSurfaceClient = 0;
                NxClient_Free(&pSelf->m_allocResults);
                rv = -1;
            }
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
    pD->GetScanInfo = 0;
    pD->GetUtcTime = 0;
    pD->Tune = BroadcastDemo_Tune;
    pD->StartBlindScan = BroadcastDemo_StartBlindScan;
    pD->StopScan = BroadcastDemo_StopScan;
    pD->Stop = BroadcastDemo_Stop;
    pD->Release = BroadcastDemo_Release;

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

