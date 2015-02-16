#include <stdlib.h>
#include "TunerHAL.h"
#include "Broadcast.h"
#include "nexus_frontend.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_stc_channel.h"
#include "nexus_pid_channel.h"
#include "nexus_parser_band.h"
#include "nxclient.h"
#include "nexus_surface_client.h"
#include "nexus_video_types.h"

#undef LOG_TAG
#define LOG_TAG "BroadcastDemo"
#include <cutils/log.h>

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
        memset(&m_allocResults, 0, sizeof(m_allocResults));
        m_nxClientId = 0;
        m_hSurfaceClient = NULL;
        m_surface = NULL;
        m_hVideoClient = NULL;
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
    NEXUS_SurfaceHandle              m_surface;
    NEXUS_SurfaceClientHandle        m_hVideoClient;

    struct TrackInfoList {
        Vector<BroadcastTrackInfo> video;
        Vector<BroadcastTrackInfo> subtitle;
    } trackInfoList;

    BroadcastScanInfo m_scanInfo;
};

BroadcastDemo_Context *pSelf;

#define MAX_SUBTITLES 3

struct subtitle {
    const char *language;
    uint32_t demo_color;
};

static const struct subtitle RED =    { "eng", 0xffff0000 };
static const struct subtitle GREEN =  { "fra", 0xff00ff00 };
static const struct subtitle BLUE =   { "spa", 0xff0000ff };
static const struct subtitle YELLOW = { "ita", 0xffffff00 };
static const struct subtitle WHITE =  { "deu", 0xffffffff };
static const uint32_t TRANSPARENT = 0x00000000U;

static struct {
    int id;
    BroadcastChannelInfo::Type type;
    const char *number;
    const char *name;
    int onid;
    int tsid;
    int sid;
    int freqKHz;
    int vpid;
    struct subtitle subtitle[MAX_SUBTITLES]; //language == NULL marks the end of the array
    const char *logoUrl;
} lineup[] = {
    { 0, BroadcastChannelInfo::TYPE_DVB_T, "8", "8madrid", 0x22d4, 0x0027, 0x0f3d, 618000, 0x100, {RED}, "http://static.programacion-tdt.com/imgAPP/8madrid.min.png" },
    { 1, BroadcastChannelInfo::TYPE_DVB_T, "13", "13tv Madrid", 0x22d4, 0x0027, 0x0f3e, 618000, 0x200, {GREEN, BLUE}, "http://static.programacion-tdt.com/imgAPP/13_TV.min.png" },
    { 2, BroadcastChannelInfo::TYPE_DVB_T, "800", "ASTROCANAL SHOP", 0x22d4, 0x0027, 0x0f43, 577000, 0x700, {}, "" },
    { 3, BroadcastChannelInfo::TYPE_DVB_T, "801", "Kiss TV", 0x22d4, 0x0027, 0x0f40, 618000, 0x401, {RED, GREEN, BLUE}, "http://www.ranklogos.com/wp-content/uploads/2012/04/kiss-tv-logo-1.jpg" },
    { 4, BroadcastChannelInfo::TYPE_DVB_T, "802", "INTER TV", 0x22d4, 0x0027, 0x0f3f, 618000, 0x300, {YELLOW}, "" },
    { 5, BroadcastChannelInfo::TYPE_DVB_T, "803", "MGustaTV", 0x22d4, 0x0027, 0x1392, 618000, 0x1000, {WHITE}, "" },
    { -1, BroadcastChannelInfo::TYPE_OTHER, "", "", 0, 0, 0, 0, 0, {}, "" }
};

static const size_t lineup_size = sizeof(lineup) / sizeof(lineup[0]);

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

#define NEXUS_RECT_IS_EQUAL(r1,r2) (((r1)->x == (r2)->x) && ((r1)->y == (r2)->y) && ((r1)->width == (r2)->width) && ((r1)->height == (r2)->height))

static void
SetGeometry_locked(NEXUS_Rect *pPosition, NEXUS_Rect * /*pClipRect*/, unsigned gfxWidth, unsigned gfxHeight, unsigned zorder, bool visible)
{
    if (pSelf && pSelf->m_hVideoClient) {
        NEXUS_SurfaceComposition *pComposition;
        NEXUS_Error errCode;
#define SCGS
#ifdef SCGS
        NEXUS_SurfaceClientSettings settings;
        NEXUS_SurfaceClient_GetSettings(pSelf->m_hVideoClient, &settings);
        pComposition = &settings.composition;
#else
        NEXUS_SurfaceComposition composition;
        NxClient_GetSurfaceClientComposition(pSelf->m_allocResults.surfaceClient[0].id, &composition);
        pComposition = &composition;
#endif
        if ( pComposition->virtualDisplay.width != gfxWidth ||
             pComposition->virtualDisplay.height != gfxHeight ||
             !NEXUS_RECT_IS_EQUAL(&pComposition->position, pPosition) ||
             pComposition->zorder != zorder ||
             pComposition->visible != visible ||
             pComposition->contentMode != NEXUS_VideoWindowContentMode_eFull )
        {
            pComposition->virtualDisplay.width = gfxWidth;
            pComposition->virtualDisplay.height = gfxHeight;
            pComposition->position = *pPosition;
            //pComposition->position.width -= pComposition->position.x;
            pComposition->zorder = zorder; 
            pComposition->visible = visible;
            pComposition->contentMode = NEXUS_VideoWindowContentMode_eFull;
            ALOGE("%s: clientcomposition vd[%d,%d] p[%d,%d,%d,%d] c[%d,%d,%d,%d] z%d v%d cm%d",
                      __FUNCTION__,
                      pComposition->virtualDisplay.width, pComposition->virtualDisplay.height,
                      pComposition->position.x, pComposition->position.y, pComposition->position.width, pComposition->position.height,
                      pComposition->clipRect.x, pComposition->clipRect.y, pComposition->clipRect.width, pComposition->clipRect.height,
                      pComposition->zorder, pComposition->visible, pComposition->contentMode
                      );
#ifdef SCGS
            errCode = NEXUS_SurfaceClient_SetSettings(pSelf->m_hVideoClient, &settings);
#else
            errCode = NxClient_SetSurfaceClientComposition(pSelf->m_allocResults.surfaceClient[0].id, &composition);
#endif
            if (errCode != NEXUS_SUCCESS) {
                ALOGE("%s: clientcomposition failed", __FUNCTION__);
            }
        }
    }
}

static void
SetGeometry()
{
}

static int
BroadcastDemo_SetGeometry(BroadcastRect position, BroadcastRect clipRect, jshort gfxWidth, jshort gfxHeight, jshort zorder, jboolean visible)
{
    NEXUS_Rect pos, clip;
    pos.x = position.x;
    pos.y = position.y;
    pos.width = position.w;
    pos.height = position.h;
    clip.x = clipRect.x;
    clip.y = clipRect.y;
    clip.width = clipRect.w;
    clip.height = clipRect.h;
    SetGeometry_locked(&pos, &clip, gfxWidth, gfxHeight, zorder, visible);
    return 0;
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

static NEXUS_Error DrawSubtitleDemo(uint32_t argb)
{
    NEXUS_Error rc;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_SurfaceMemory mem;
    unsigned x, y;

    ALOGD("%s: argb = 0x%08x", __FUNCTION__, argb);

    NEXUS_Surface_GetCreateSettings(pSelf->m_surface, &createSettings);
    rc = NEXUS_Surface_GetMemory(pSelf->m_surface, &mem);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_Surface_GetMemory failed: %d", rc);
        return rc;
    }

    /* draw checker board */
    for (y=0;y<createSettings.height;y++) {
        uint32_t *ptr = (uint32_t *)(((uint8_t*)mem.buffer) + y * mem.pitch);
        for (x=0;x<createSettings.width;x++) {
            if ((argb == TRANSPARENT) ||
                    (x < 2*(createSettings.width/10)) ||
                    (x > 8*(createSettings.width/10)) ||
                    (y < 7*(createSettings.height/10)) ||
                    (y > 9*(createSettings.height/10))) {
                ptr[x] = TRANSPARENT;
            }
            else if ((x/10)%2 != (y/10)%2) {
                ptr[x] = 0x55AAAAAA;
            }
            else {
                ptr[x] = argb;
            }
        }
    }
    NEXUS_Surface_Flush(pSelf->m_surface);

    rc = NEXUS_SurfaceClient_SetSurface(pSelf->m_hSurfaceClient, pSelf->m_surface);
    return rc;
}

static int BroadcastDemo_Stop()
{
    ALOGE("%s: Enter", __FUNCTION__);

    // clear subtitles
    DrawSubtitleDemo(TRANSPARENT);

    // Stop the video decoder
    if (pSelf->decoding) {
        NEXUS_SimpleVideoDecoder_Stop(pSelf->m_hSimpleVideoDecoder);
        TunerHAL_onBroadcastEvent(VIDEO_AVAILABLE, 0, 0);
        pSelf->decoding = false;
    }

    // Close the PID channel
    if (pSelf->pidChannel) {
        NEXUS_PidChannel_Close(pSelf->pidChannel);
        pSelf->pidChannel = 0;
    }

    pSelf->trackInfoList.video.clear();
    pSelf->trackInfoList.subtitle.clear();

    ALOGE("%s: Exit", __FUNCTION__);

    return 0;
}

static int BroadcastDemo_StartBlindScan()
{
    ALOGE("%s: Enter", __FUNCTION__);

    pSelf->m_scanInfo.valid = true;
    pSelf->m_scanInfo.busy = false;
    pSelf->m_scanInfo.progress = 100;
    pSelf->m_scanInfo.channels = lineup_size - 1;
    pSelf->m_scanInfo.TVChannels = lineup_size - 1;
    pSelf->m_scanInfo.radioChannels = 0;
    pSelf->m_scanInfo.dataChannels = 0;

    TunerHAL_onBroadcastEvent(SCANNING_START, 0, 0);
    TunerHAL_onBroadcastEvent(SCANNING_PROGRESS, 100, 0);
    TunerHAL_onBroadcastEvent(SCANNING_COMPLETE, 0, 0);
    TunerHAL_onBroadcastEvent(CHANNEL_LIST_CHANGED, 0, 0);
    ALOGE("%s: Exit", __FUNCTION__);
    return -1;
}

static int BroadcastDemo_StopScan()
{
    ALOGE("%s: Enter", __FUNCTION__);
    TunerHAL_onBroadcastEvent(SCANNING_COMPLETE, 0, 0);
    TunerHAL_onBroadcastEvent(CHANNEL_LIST_CHANGED, 0, 0);
    ALOGE("%s: Exit", __FUNCTION__);
    return -1;
}

static int channelIndex(int channel_id)
{
    for (int i = 0; lineup[i].id >= 0; i++) {
        if (lineup[i].id == channel_id) {
            return i;
        }
    }
    return -1; //not found
}

static void
CacheTrackInfoList(int channel_id)
{
    Vector<BroadcastTrackInfo> v;
    NEXUS_VideoDecoderStatus status;
    if (NEXUS_SimpleVideoDecoder_GetStatus(pSelf->m_hSimpleVideoDecoder, &status) == NEXUS_SUCCESS && status.source.height > 0) {
        BroadcastTrackInfo info;
        info.type = 1;
        info.id = "0";
        info.squarePixelHeight = status.source.height; 
        switch (status.aspectRatio) {
        case NEXUS_AspectRatio_e4x3:
            info.squarePixelWidth = (info.squarePixelHeight * 4) / 3;
            break;
        case NEXUS_AspectRatio_e16x9:
            info.squarePixelWidth = (info.squarePixelHeight * 16) / 9;
            break;
        default:
            info.squarePixelWidth = status.source.width;
            break;
        }
        switch (status.frameRate) {
        case NEXUS_VideoFrameRate_e23_976:  info.frameRate = 23.976; break;
        case NEXUS_VideoFrameRate_e24:      info.frameRate = 24; break;
        case NEXUS_VideoFrameRate_e25:      info.frameRate = 25; break;
        case NEXUS_VideoFrameRate_e29_97:   info.frameRate = 29.97; break;
        case NEXUS_VideoFrameRate_e30:      info.frameRate = 30; break;
        case NEXUS_VideoFrameRate_e50:      info.frameRate = 50; break;
        case NEXUS_VideoFrameRate_e59_94:   info.frameRate = 59.94; break;
        case NEXUS_VideoFrameRate_e60:      info.frameRate = 60; break;
        case NEXUS_VideoFrameRate_e14_985:  info.frameRate = 14.985; break;
        case NEXUS_VideoFrameRate_e7_493:   info.frameRate = 7.493; break;
        case NEXUS_VideoFrameRate_e10:      info.frameRate = 10; break;
        case NEXUS_VideoFrameRate_e15:      info.frameRate = 15; break;
        case NEXUS_VideoFrameRate_e20:      info.frameRate = 20; break;
        case NEXUS_VideoFrameRate_e12_5:    info.frameRate = 12.5; break;
        default:                            info.frameRate = 0; break;
        }

        ALOGE("%s: %dx%d (%dx%d) fr %f", __FUNCTION__, status.source.width, status.source.height, info.squarePixelWidth, info.squarePixelHeight, info.frameRate);
        v.push_back(info);
        pSelf->trackInfoList.video = v;

        v.clear();
        int index = channelIndex(channel_id);
        if (index >= 0) {
            for (int i = 0; i < MAX_SUBTITLES; i++) {
                struct subtitle *s = &lineup[index].subtitle[i];
                if (!s->language) //end of list
                    break;
                ALOGE("%s: subtitle %d: 0x%08x  %s", __FUNCTION__, i,
                        s->demo_color, s->language);
                info.type = 2;
                info.id = String8::format("0x%08x", s->demo_color);
                info.lang = String8(s->language);
                v.push_back(info);
            }
        }
        else {
            ALOGE("%s: invalid channel_id: %d", __FUNCTION__, channel_id);
        }
        pSelf->trackInfoList.subtitle = v;
    }
}

static void sourceChangeCallback(void * /*context*/, int param)
{
    CacheTrackInfoList(param);
    TunerHAL_onBroadcastEvent(TRACK_LIST_CHANGED, 0, 0);
    if (pSelf->trackInfoList.video.size()) {
        TunerHAL_onBroadcastEvent(VIDEO_AVAILABLE, 1, 0);
        TunerHAL_onBroadcastEvent(TRACK_SELECTED, 1, &pSelf->trackInfoList.video[0].id);
    }
}

static Vector<BroadcastTrackInfo>
BroadcastDemo_GetTrackInfoList()
{
    if (pSelf->trackInfoList.video.size() == 0) {
        ALOGE("%s: no video info", __FUNCTION__); 
    }
    else {
        ALOGE("%s: video %s %dx%d fr %f", __FUNCTION__,
              pSelf->trackInfoList.video[0].id.string(),
              pSelf->trackInfoList.video[0].squarePixelWidth,
              pSelf->trackInfoList.video[0].squarePixelHeight,
              pSelf->trackInfoList.video[0].frameRate
              ); 
    }
    if (pSelf->trackInfoList.subtitle.size() == 0) {
        ALOGE("%s: no subtitle info", __FUNCTION__);
    }
    else {
        for (size_t i = 0; i < pSelf->trackInfoList.subtitle.size(); i++) {
            ALOGE("%s: subtitle %s %s", __FUNCTION__,
                  pSelf->trackInfoList.subtitle[i].id.string(),
                  pSelf->trackInfoList.subtitle[i].lang.string()
                  );
        }
    }

    Vector<BroadcastTrackInfo> all;
    all.appendVector(pSelf->trackInfoList.video);
    all.appendVector(pSelf->trackInfoList.subtitle);
    return all;
}

static int
BroadcastDemo_SelectTrack(int type, const String8 *id)
{
    int result = -1;
    if (type == 2) { //subtitle
        ALOGI("%s: subtitle track %s", __FUNCTION__, id ? id->string() : "NULL");
        uint32_t argb = id ? strtoul(id->string(), 0, 0) : TRANSPARENT;
        DrawSubtitleDemo(argb);
        TunerHAL_onBroadcastEvent(TRACK_SELECTED, 2, id);
        result = 0;
    }
    return result;
}

static void
BroadcastDemo_SetCaptionEnabled(bool enabled)
{
    ALOGI("%s: %s", __FUNCTION__, enabled ? "enabled" : "disabled");
    if (!enabled)
        DrawSubtitleDemo(TRANSPARENT);
}

static int BroadcastDemo_Tune(String8 s8id)
{
    NEXUS_VideoDecoderSettings settings;
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_FrontendOfdmSettings ofdmSettings;
    NEXUS_FrontendUserParameters userParams;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_VideoCodec video_codec = NEXUS_VideoCodec_eMpeg2;
    NEXUS_Error rc;
    int video_pid;

    const int channel_id = strtoul(s8id.string(), 0, 0);
    int i = channelIndex(channel_id);
    if (i < 0) {
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

    NEXUS_SimpleVideoDecoder_GetSettings(pSelf->m_hSimpleVideoDecoder, &settings);

    settings.sourceChanged.callback = sourceChangeCallback;
    settings.sourceChanged.context = pSelf;
    settings.sourceChanged.param = channel_id;

    rc = NEXUS_SimpleVideoDecoder_SetSettings(pSelf->m_hSimpleVideoDecoder, &settings);
    if (rc)
    {
        ALOGE("%s: SetSettings failed", __FUNCTION__);
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
        ci.logoUrl = lineup[i].logoUrl;
        ci.type = lineup[i].type;
        civ.push_back(ci);
        ALOGI("%s: %s %s", __FUNCTION__, ci.number.string(), ci.name.string());
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

static BroadcastScanInfo
BroadcastDemo_GetScanInfo()
{
    ALOGE("%s: Enter", __FUNCTION__);
    ALOGE("%s: Exit", __FUNCTION__);
    return pSelf->m_scanInfo;
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
            ALOGE("%s: SDB ID %d", __FUNCTION__, pSelf->m_allocResults.surfaceClient[0].id);
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

        if (rv == 0) {
            NEXUS_SurfaceCreateSettings createSettings;

            NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
            createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
            createSettings.width = 720;
            createSettings.height = 480;
            pSelf->m_surface = NEXUS_Surface_Create(&createSettings);
            rv = DrawSubtitleDemo(TRANSPARENT);
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
    pD->GetScanInfo = BroadcastDemo_GetScanInfo;
    pD->GetUtcTime = 0;
    pD->Tune = BroadcastDemo_Tune;
    pD->StartBlindScan = BroadcastDemo_StartBlindScan;
    pD->StopScan = BroadcastDemo_StopScan;
    pD->Stop = BroadcastDemo_Stop;
    pD->Release = BroadcastDemo_Release;
    pD->SetGeometry = BroadcastDemo_SetGeometry;
    pD->GetTrackInfoList = BroadcastDemo_GetTrackInfoList;
    pD->SelectTrack = BroadcastDemo_SelectTrack;
    pD->SetCaptionEnabled = BroadcastDemo_SetCaptionEnabled;

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

