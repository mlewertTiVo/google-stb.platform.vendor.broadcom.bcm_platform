#include "TunerHAL.h"

#include <stdlib.h>

extern "C" {
#include "techtype.h"
#include "app.h"
#include "stbdpc.h"
#include "ap_cfg.h"
#include "ap_dbacc.h"
#include "ap_cntrl.h"
#include "stberc.h"
#include "stbuni.h"
#include "stbhwos.h"
#include "stbheap.h"
#include "stbvtc.h"
#include "stbdsapi.h"
#include "dtvkit_platform.h"
};

#undef LOG_TAG
#define LOG_TAG "BroadcastDTVKit"
#include <cutils/log.h>

//#define DEBUG_EVENTS

//#define JOURNAL
//#define NOWNEXT

class UpdateRecord {
public:
    BroadcastProgramUpdateInfo::UpdateType type;
    U16BIT lcn;
    U16BIT event_id;
};

class BroadcastDTVKit_Context {
public:
    BroadcastDTVKit_Context() {
        scanner.active = false;
        scanner.infoValid = false;
        scanner.mutex = STB_OSCreateMutex();
        path = INVALID_RES_ID;
        s_ptr = NULL;
        vpid = 0;
        apid = 0;
        spid = 0;
        decoding = 0;
        w = 0;
        h = 0;
        time_valid = false;
        tot_search_active = false;
#ifdef JOURNAL
        epg.mutex = STB_OSCreateMutex();
        epg.backlog = 0;
#endif
    };
    struct {
        void *mutex;
        bool active;
        bool infoValid;
        jchar progress;
    } scanner;
    U8BIT path;
    void *s_ptr;
    U16BIT vpid;
    U16BIT apid;
    U16BIT spid;
    int decoding;
    U16BIT w;
    U16BIT h;
    bool time_valid;
    bool tot_search_active;
    Vector<BroadcastTrackInfo> batil;
    Vector<BroadcastTrackInfo> bstil;
#ifdef JOURNAL
    struct {
        void *mutex;
        List<UpdateRecord> queue;
        int backlog;
    } epg;
#endif
};

static BroadcastDTVKit_Context *pSelf;

static int BroadcastDTVKit_Stop()
{
    ALOGE("%s: Enter", __FUNCTION__);

    /* Turn off subtitle display and processing */
    if(ACTL_AreSubtitlesStarted())
    {
        ALOGE("%s: Stopping subtitles", __FUNCTION__);
        ACTL_StopSubtitles();
    }

    if (pSelf->path != INVALID_RES_ID) {
        ALOGE("%s: Stopping", __FUNCTION__);
        pSelf->s_ptr = 0;
        ACTL_DecodeOff(pSelf->path);
    }
    ALOGE("%s: Exit", __FUNCTION__);

    return -1;
}

/*
    U8BIT progress = ACTL_GetSearchProgress();
    ALOGE("%s:# progress %d%%", __FUNCTION__, progress);
    if (ACTL_IsSearchComplete()) {
        pSelf->scanning = false;
        if (ACTL_IsTargetRegionRequired()) {
            ALOGE("%s: target region required", __FUNCTION__);
        }
        ACTL_CompleteServiceSearch();
        ALOGE("%s: scan complete", __FUNCTION__);
        TunerHAL_onBroadcastEvent(0);
        TunerHAL_onBroadcastEvent(200);
    }
    else {
        TunerHAL_onBroadcastEvent(100 + ACTL_GetSearchProgress());
    }
*/

static void
onScanProgress(jshort progress)
{
    TunerHAL_onBroadcastEvent(SCANNING_PROGRESS, progress, 0);
}

static void
onScanComplete()
{
    TunerHAL_onBroadcastEvent(SCANNING_COMPLETE, 0, 0);
    TunerHAL_onBroadcastEvent(CHANNEL_LIST_CHANGED, 0, 0);
}

static void
onScanStart()
{
    TunerHAL_onBroadcastEvent(SCANNING_START, 0, 0);
}

static bool
scannerUpdateUnderLock(bool stop)
{
    if (!pSelf->scanner.active) {
        return false;
    }

    pSelf->scanner.progress = ACTL_GetSearchProgress();
    pSelf->scanner.infoValid = true;

    if (!stop) {
        onScanProgress(pSelf->scanner.progress);
    }

    bool justCompleted = false;
    if (ACTL_IsSearchComplete()) {
        pSelf->scanner.active = false;
        justCompleted = true;
    }

    if (stop && pSelf->scanner.active) {
        pSelf->scanner.active = false;
        ACTL_StopServiceSearch();
        justCompleted = true;
    }

    if (justCompleted) {
        //pSelf->tuningParams.biList.clear();
        if (ACTL_IsTargetRegionRequired()) {
            printf("target region required\n");
        }
        ACTL_CompleteServiceSearch();
        pSelf->tot_search_active = ACTL_StartTotSearch();
        onScanComplete();
    }

    return true;
}

static bool
scannerStopUnderLock()
{
    scannerUpdateUnderLock(true);
    return true;
}

static bool
scannerInitUnderLock()
{
    scannerStopUnderLock();
    return true;
}

static bool
startBlindScan()
{
    if (pSelf->tot_search_active) {
        ACTL_StopTotSearch();
        pSelf->tot_search_active = false;
    }

    STB_OSMutexLock(pSelf->scanner.mutex);
    if (!scannerInitUnderLock()) {
        STB_OSMutexUnlock(pSelf->scanner.mutex);
        return false;
    }

    // manual scan with network
    pSelf->scanner.infoValid = false;
    ADB_ResetDatabase();
    if (!ACTL_StartServiceSearch(SIGNAL_COFDM, ACTL_FREQ_SEARCH)) {
        STB_OSMutexUnlock(pSelf->scanner.mutex);
        return false;
    }
    pSelf->scanner.active = true;
    STB_OSMutexUnlock(pSelf->scanner.mutex);
    onScanStart();
    return true;
}

static bool
satelliteTuningParamsToDTVKit(BroadcastScanParams *pParams, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    dtvkitParams.freq = pParams->freqKHz / 1000;
    dtvkitParams.u.sat.satellite = 0;
    dtvkitParams.u.sat.polarity = pParams->polarity == BroadcastScanParams::Horizontal ? POLARITY_HORIZONTAL : POLARITY_VERTICAL;
    dtvkitParams.u.sat.symbol_rate = pParams->symK;
    switch (pParams->codeRate.numerator * 100 + pParams->codeRate.denominator) {
    case 000: dtvkitParams.u.sat.fec = FEC_AUTOMATIC; break;
    case 102: dtvkitParams.u.sat.fec = FEC_1_2; break;
    case 203: dtvkitParams.u.sat.fec = FEC_2_3; break;
    case 304: dtvkitParams.u.sat.fec = FEC_3_4; break;
    case 506: dtvkitParams.u.sat.fec = FEC_5_6; break;
    case 708: dtvkitParams.u.sat.fec = FEC_7_8; break;
        // Extra FEC modes for DVB-S2
    case 104: dtvkitParams.u.sat.fec = FEC_1_4; break;
    case 103: dtvkitParams.u.sat.fec = FEC_1_3; break;
    case 205: dtvkitParams.u.sat.fec = FEC_2_5; break;
    case 809: dtvkitParams.u.sat.fec = FEC_8_9; break;
    case 910: dtvkitParams.u.sat.fec = FEC_9_10; break;
    default:
        return false;
    }; 
    switch (pParams->satelliteMode) {
    case BroadcastScanParams::SatelliteMode_QpskLdpc: dtvkitParams.u.sat.dvb_s2 = true; dtvkitParams.u.sat.modulation = MOD_QPSK; break;
    case BroadcastScanParams::SatelliteMode_8pskLdpc: dtvkitParams.u.sat.dvb_s2 = true; dtvkitParams.u.sat.modulation = MOD_8PSK; break;
    case BroadcastScanParams::SatelliteMode_Dvb: dtvkitParams.u.sat.dvb_s2 = false; dtvkitParams.u.sat.modulation = MOD_QPSK; break;
    default:
        return false;
    }
    return true;
}

static bool
qamTuningParamsToDTVKit(BroadcastScanParams *pParams, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    dtvkitParams.freq = pParams->freqKHz;
    switch (pParams->qamMode) {
    case BroadcastScanParams::QamMode_16: dtvkitParams.u.cab.mode = MODE_QAM_16; break;
    case BroadcastScanParams::QamMode_32: dtvkitParams.u.cab.mode = MODE_QAM_32; break;
    case BroadcastScanParams::QamMode_64: dtvkitParams.u.cab.mode = MODE_QAM_64; break;
    case BroadcastScanParams::QamMode_128: dtvkitParams.u.cab.mode = MODE_QAM_128; break;
    case BroadcastScanParams::QamMode_256: dtvkitParams.u.cab.mode = MODE_QAM_256; break;
    default: dtvkitParams.u.cab.mode = MODE_QAM_AUTO; break;
    }
    dtvkitParams.u.cab.symbol_rate = pParams->symK;
    return true;
}

static bool
ofdmTuningParamsToDTVKit(BroadcastScanParams *pParams, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    dtvkitParams.freq = pParams->freqKHz;
    switch (pParams->bandwidthKHz) {
    case 8000: dtvkitParams.u.terr.bwidth = TBWIDTH_8MHZ; break;
    case 7000: dtvkitParams.u.terr.bwidth = TBWIDTH_7MHZ; break;
    case 6000: dtvkitParams.u.terr.bwidth = TBWIDTH_6MHZ; break;
    case 5000: dtvkitParams.u.terr.bwidth = TBWIDTH_5MHZ; break;
    case 10000: dtvkitParams.u.terr.bwidth = TBWIDTH_10MHZ; break;
    default: return false;
    }
    switch (pParams->ofdmTransmissionMode) {
    case BroadcastScanParams::OfdmTransmissionMode_1k: dtvkitParams.u.terr.mode = MODE_COFDM_1K; break;
    case BroadcastScanParams::OfdmTransmissionMode_2k: dtvkitParams.u.terr.mode = MODE_COFDM_2K; break;
    case BroadcastScanParams::OfdmTransmissionMode_4k: dtvkitParams.u.terr.mode = MODE_COFDM_4K; break;
    case BroadcastScanParams::OfdmTransmissionMode_8k: dtvkitParams.u.terr.mode = MODE_COFDM_8K; break;
    case BroadcastScanParams::OfdmTransmissionMode_16k: dtvkitParams.u.terr.mode = MODE_COFDM_16K; break;
    case BroadcastScanParams::OfdmTransmissionMode_32k: dtvkitParams.u.terr.mode = MODE_COFDM_32K; break;
    default: dtvkitParams.u.terr.mode = MODE_COFDM_UNDEFINED; break;
    }
    switch (pParams->ofdmMode) {
    case BroadcastScanParams::OfdmMode_Dvbt: dtvkitParams.u.terr.type = TERR_TYPE_DVBT; break;
    case BroadcastScanParams::OfdmMode_Dvbt2: dtvkitParams.u.terr.type = TERR_TYPE_DVBT2; break;
    default: return false;
    }
    dtvkitParams.u.terr.plp_id = pParams->plpId;
    return true;
}

static bool
tuningParamsToDTVKit(BroadcastScanParams *pParams, E_STB_DP_SIGNAL_TYPE &tunerType, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    switch (pParams->deliverySystem) {
    case BroadcastScanParams::DVB_T:
        tunerType = SIGNAL_COFDM;
        return ofdmTuningParamsToDTVKit(pParams, dtvkitParams);
    case BroadcastScanParams::DVB_S:
        tunerType = SIGNAL_QPSK;
        return satelliteTuningParamsToDTVKit(pParams, dtvkitParams);
    case BroadcastScanParams::DVB_C:
        tunerType = SIGNAL_QAM;
        return qamTuningParamsToDTVKit(pParams, dtvkitParams);
    }
    return false;
}

static bool
startManualScan(BroadcastScanParams *pParams)
{
    S_MANUAL_TUNING_PARAMS dtvkitParams;
    E_STB_DP_SIGNAL_TYPE tunerType;

    if (!tuningParamsToDTVKit(pParams, tunerType, dtvkitParams)) {
        return false;
    }

    if (pSelf->tot_search_active) {
        ACTL_StopTotSearch();
        pSelf->tot_search_active = false;
    }

    STB_OSMutexLock(pSelf->scanner.mutex);

    if (!scannerInitUnderLock()) {
        STB_OSMutexUnlock(pSelf->scanner.mutex);
        return false;
    }

    // manual scan with network
    pSelf->scanner.infoValid = false;
    if (!ACTL_StartManualSearch(tunerType, &dtvkitParams, pParams->mode == BroadcastScanParams::ScanMode_Home ? ACTL_NETWORK_SEARCH : ACTL_FREQ_SEARCH)) {
        STB_OSMutexUnlock(pSelf->scanner.mutex);
        return false;
    }
    pSelf->scanner.active = true;
    STB_OSMutexUnlock(pSelf->scanner.mutex);
    onScanStart();
    return true;
}

static int BroadcastDTVKit_StartScan(BroadcastScanParams *pParams)
{
    int rv = -1;
    ALOGE("%s: Enter", __FUNCTION__); 

    if (pParams == 0 || (pParams->deliverySystem == BroadcastScanParams::DVB_T && pParams->mode == BroadcastScanParams::ScanMode_Blind)) {
        /* DVB-T blind scan */
        startBlindScan();
        rv = 0;
    }
    else if (pParams->mode == BroadcastScanParams::ScanMode_Blind) {
        /* no other blind scan */
        rv = -1;
    }
    else {
        if (startManualScan(pParams)) {
            rv = 0;
        }
    }
    ALOGE("%s: Exit", __FUNCTION__); 
    return rv;
}

static int BroadcastDTVKit_StopScan()
{
    int rv = -1;
    ALOGE("%s: Enter", __FUNCTION__); 

    STB_OSMutexLock(pSelf->scanner.mutex);

    if (scannerStopUnderLock()) {
        rv = 0;
    }

    STB_OSMutexUnlock(pSelf->scanner.mutex);
    ALOGE("%s: Exit", __FUNCTION__);
    return rv;
}

static void
resetDecodeState(void)
{
    pSelf->vpid = 0;
    pSelf->apid = 0;
    pSelf->spid = 0;
    pSelf->decoding = 0;
    pSelf->w = 0;
    pSelf->h = 0;
    pSelf->batil.clear();
    pSelf->bstil.clear();
}

static int BroadcastDTVKit_Tune(String8 s8id)
{
    int rv = -1;

    ALOGE("%s: Enter", __FUNCTION__);

    if (pSelf->tot_search_active) {
        ACTL_StopTotSearch();
        pSelf->tot_search_active = false;
    }

    U16BIT lcn = strtoul(s8id.string(), 0, 0); 
    void *s_ptr = ADB_FindServiceByLcn(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, lcn, TRUE);
    pSelf->s_ptr = s_ptr;
    resetDecodeState();
    if (s_ptr) {
        pSelf->path = ACTL_TuneToService(pSelf->path, NULL, s_ptr, TRUE, TRUE);
        if (pSelf->path == INVALID_RES_ID) {
            ALOGE("%s: Invalid resource", __FUNCTION__);
        }
        else {
            ALOGE("%s: Tuning", __FUNCTION__);
            TunerHAL_onBroadcastEvent(VIDEO_AVAILABLE, 0, 0);
            rv = 0;

            /* Turn on subtitle display and processing */
            ALOGD("%s: Starting subtitles", __FUNCTION__);
            if (ACTL_StartSubtitles()) {
                ALOGI("%s: Subtitles started", __FUNCTION__);
            } else {
                ALOGE("%s: Starting subtitles FAILED!", __FUNCTION__);
            }
        }
    }
    else {
        ALOGE("%s: LCN %d not found", __FUNCTION__, lcn);
    }

    ALOGE("%s: Exit", __FUNCTION__);

    return rv;
}

static unsigned
utf8tocode(U8BIT *p, int32_t &code)
{
    U8BIT c = *p++;
    if (c == 0) {
        return 0;
    }
    if (c < 0x80) {
        code = c;
        return 1;
    }
    unsigned count;
    if ((c & 0xe0) == 0xc0) {
        code = c & 0x1f;
        count = 1;
    }
    else if ((c & 0xf0) == 0xe0) {
        code = c & 0xf;
        count = 2;
    }
    else if ((c & 0xf8) == 0xf0) {
        code = c & 0x7;
        count = 3;
    }
    else if ((c & 0xfc) == 0xf8) {
        code = c & 0x3;
        count = 4;
    }
    else if ((c & 0xfe) == 0xfc) {
        code = c & 0x1;
        count = 5;
    }
    else {
        code = -1;
        return 1;
    }
    for (unsigned i = 0; i < count; i++) {
        c = *p++;
        if ((c & 0xc0) != 0x80) {
            code = -1;
            return 1;
        }
        code <<= 6;
        code |= (c & 0x3f);
    }
    return count + 1;
}

static String8
utf8tostringstrippingdvbcodes(U8BIT *p)
{
    unsigned len;
    String8 s;
    if (p == 0) {
        return s;
    }
    if (*p == 0x15) {
        p++; 
    }
    do {
        int32_t code = 0;
        len = utf8tocode(p, code);
        switch (code) {
        case 0xe086:
        case 0xe087:
            p += len;
            break;
        case 0xe08a:
            s.append("\n");
            p += len;
            break;
        default:
            s.append((char *)p, len);
            p += len;
            break;
        }
    } while (len);
    return s;
}

static struct {
    const char *display_name;
    const char *url;
} freeviewLogoMap[] = {
{ "BBC ONE West", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_one.png" },
{ "BBC TWO", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_two_uk.png" },
{ "ITV", "http://www.lyngsat-logo.com/logo/tv/ii/itv_uk.png" },
{ "Channel 4", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_uk.png" },
{ "Channel 5", "http://www.lyngsat-logo.com/logo/tv/cc/channel5_uk.png" },
{ "ITV2", "http://www.lyngsat-logo.com/logo/tv/ii/itv2.png" },
{ "BBC THREE", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_three.png" },
{ "Made In Bristol", "http://www.lyngsat-logo.com/logo/tv/mm/made_tv_uk_bristol.png" },
{ "BBC FOUR", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_four_uk.png" },
{ "ITV3", "http://www.lyngsat-logo.com/logo/tv/ii/itv3.png" },
{ "Pick", "http://www.lyngsat-logo.com/logo/tv/pp/pick_tv_uk.png" },
{ "Dave", "http://www.lyngsat-logo.com/logo/tv/dd/dave_uktv.png" },
{ "Channel 4+1", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_uk_plus1.png" },
{ "More 4", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_more4_uk.png" },
{ "Film4", "http://www.lyngsat-logo.com/logo/tv/ff/film4.png" },
{ "QVC", "http://www.lyngsat-logo.com/logo/tv/qq/qvc_uk.png" },
{ "Really", "http://www.lyngsat-logo.com/logo/tv/rr/really_uktv.png" },
{ "4Music", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_music.png" },
{ "Yesterday", "http://www.lyngsat-logo.com/logo/tv/yy/yesterday_uktv.png" },
{ "Drama", "http://www.lyngsat-logo.com/logo/tv/dd/drama_uktv.png" },
{ "VIVA", "http://www.lyngsat-logo.com/logo/tv/vv/viva_uk.png" },
{ "Ideal World", "http://www.lyngsat-logo.com/logo/tv/ii/ideal_world.png" },
{ "ITV4", "http://www.lyngsat-logo.com/logo/tv/ii/itv4.png" },
{ "Dave ja vu", "http://www.lyngsat-logo.com/logo/tv/dd/dave_ja_vu.png" },
{ "ITVBe", "http://www.lyngsat-logo.com/logo/tv/ii/itv_uk_be.png" },
{ "ITV2 +1", "http://www.lyngsat-logo.com/logo/tv/ii/itv2_plus1.png" },
{ "E4", "http://www.lyngsat-logo.com/logo/tv/ee/e4_uk.png" },
{ "E4+1", "http://www.lyngsat-logo.com/logo/tv/ee/e4_uk_plus1.png" },
{ "5*", "http://www.lyngsat-logo.com/logo/tv/cc/channel5_star.png" },
{ "5 USA", "http://www.lyngsat-logo.com/logo/tv/cc/channel5_usa.png" },
{ "Movie Mix", 0 },
{ "ITV +1", "http://www.lyngsat-logo.com/logo/tv/ii/itv_uk.png" },
{ "ITV3+1", "http://www.lyngsat-logo.com/logo/tv/ii/itv3_plus1.png" },
{ "QVC Beauty", "http://www.lyngsat-logo.com/logo/tv/qq/qvc_beauty_uk.png" },
{ "Create & Craft", "http://www.lyngsat-logo.com/logo/tv/cc/create_and_craft.png" },
{ "QUEST", "http://www.lyngsat-logo.com/logo/tv/qq/quest.png" },
{ "QUEST+1", "http://www.lyngsat-logo.com/logo/tv/qq/quest_plus1.png" },
{ "The Store", 0 },
{ "Rocks & Co 1", "http://www.lyngsat-logo.com/logo/tv/rr/rocks_and_co.png" },
{ "Food Network", "http://www.lyngsat-logo.com/logo/tv/ff/food_network_uk.png" },
{ "Travel Channel", "http://www.lyngsat-logo.com/logo/tv/tt/travel_channel_uk.png" },
{ "Gems TV", "http://www.lyngsat-logo.com/logo/tv/gg/gems_tv.png" },
{ "Channel 5+1", 0 },
{ "Film4+1", "http://www.lyngsat-logo.com/logo/tv/ff/film4_plus1.png" },
{ "Challenge", "http://www.lyngsat-logo.com/logo/tv/cc/challenge_tv_uk.png" },
{ "4seven", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_seven_uk.png" },
{ "movies4men", "http://www.lyngsat-logo.com/logo/tv/mm/movies_4_men.png" },
{ "Jewellery Ch.", 0 },
{ "Channel 5+24", "http://www.lyngsat-logo.com/logo/tv/cc/channel5_uk_plus24.png" },
{ "QVC EXTRA", "http://www.lyngsat-logo.com/logo/tv/qq/qvc_extra_uk.png" },
{ "BT Sport 1", "http://www.lyngsat-logo.com/logo/tv/bb/bt_sport_1.png" },
{ "BT Sport 2", "http://www.lyngsat-logo.com/logo/tv/bb/bt_sport_2.png" },
{ "True Entertainment", "http://www.lyngsat-logo.com/logo/tv/tt/true_entertainment.png" },
{ "ITV4+1", "http://www.lyngsat-logo.com/logo/tv/ii/itv4_plus1.png" },
{ "Community", "http://www.lyngsat-logo.com/logo/tv/cc/community_channel.png" },
{ "TBN UK", 0 },
{ "CBS Reality", "http://www.lyngsat-logo.com/logo/tv/cc/cbs_reality_uk.png" },
{ "truTV", "http://www.lyngsat-logo.com/logo/tv/tt/tru_tv_uk.png" },
{ "truTV+1", 0 },
{ "CBS Action", "http://www.lyngsat-logo.com/logo/tv/cc/cbs_action_uk.png" },
{ "Motors TV", "http://www.lyngsat-logo.com/logo/tv/mm/motors_tv_uk.png" },
{ "DAYSTAR", 0 },
{ "BBC ONE HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_one_hd.png" },
{ "BBC TWO HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_two_uk_hd.png" },
{ "ITV HD", "http://www.lyngsat-logo.com/logo/tv/ii/itv_uk_hd.png" },
{ "Channel 4 HD", "http://www.lyngsat-logo.com/logo/tv/cc/channel4_hd.png" },
{ "BBC THREE HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_three_hd.png" },
{ "BBC FOUR HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_four_uk_hd.png" },
{ "BBC NEWS HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_news_hd.png" },
{ "Al Jazeera Eng HD", 0 },
{ "Community HD", 0 },
{ "Channel 4+1 HD", 0 },
{ "4seven HD", 0 },
{ "CBBC", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_cbbc.png" },
{ "CBeebies", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_cbeebies_uk.png" },
{ "CITV", "http://www.lyngsat-logo.com/logo/tv/cc/citv_uk.png" },
{ "CBBC HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_cbbc_hd.png" },
{ "CBeebies HD", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_cbeebies_uk_hd.png" },
{ "POP", "http://www.lyngsat-logo.com/logo/tv/pp/pop_uk.png" },
{ "Tiny Pop", "http://www.lyngsat-logo.com/logo/tv/tt/tiny_pop.png" },
{ "BBC NEWS", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_news.png" },
{ "BBC Parliament", "http://www.lyngsat-logo.com/logo/tv/bb/bbc_parliament.png" },
{ "Sky News", "http://www.lyngsat-logo.com/logo/tv/ss/sky_uk_news.png" },
{ "Al Jazeera Eng", "http://www.lyngsat-logo.com/logo/tv/aa/al_jazeera_english.png" },
{ "Al Jazeera Arabic", 0 },
{ "RT", 0 },
{ "ARISE News", "http://www.lyngsat-logo.com/logo/tv/aa/arise_news.png" },
{ "ADULT Section", 0 },
{ "Television X", 0 },
{ "ADULT smileTV2", 0 },
{ "ADULT smileTV3", 0 },
{ "ADULT Babestn", 0 },
{ "ADULT PARTY", 0 },
{ "ADULT Blue", 0 },
{ "ADULT Babestn2", 0 },
{ "ADULT Xpanded TV", 0 },
{ "ADULT Section", 0 },
{ "BBC Red Button", 0 },
{ "BBC RB 1", 0 },
{ "BBC Radio 1", 0 },
{ "BBC R1X", 0 },
{ "BBC Radio 2", 0 },
{ "BBC Radio 3", 0 },
{ "BBC Radio 4", 0 },
{ "BBC R5L", 0 },
{ "BBC R5SX", 0 },
{ "BBC 6 Music", 0 },
{ "BBC Radio 4 Ex", 0 },
{ "BBC Asian Net.", 0 },
{ "BBC World Sv.", 0 },
{ "The Hits Radio", 0 },
{ "KISS FRESH", 0 },
{ "Kiss", 0 },
{ "KISSTORY", 0 },
{ "Magic", 0 },
{ "heat", 0 },
{ "Kerrang!", 0 },
{ "SMOOTH RADIO", 0 },
{ "BBC Bristol", 0 },
{ "talkSPORT", 0 },
{ "Capital FM", 0 },
{ "Premier Radio", 0 },
{ "Absolute Radio", 0 },
{ "Heart", 0 },
{ "Insight Radio", 0 },
{ "Community", 0 },
{ 0, 0 }
};

static Vector<BroadcastChannelInfo> BroadcastDTVKit_GetChannelList()
{
    Vector<BroadcastChannelInfo> civ;

    ALOGE("%s: Enter", __FUNCTION__);

    void **slist;
    U16BIT num_entries;
    ADB_GetServiceListIncludingHidden(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, &slist, &num_entries, true);
    if (slist) {
        for (unsigned i = 0; i < num_entries; i++) {
            BroadcastChannelInfo ci;
            void *t_ptr;
             
            ci.id = String8::format("%u", ADB_GetServiceLcn(slist[i]));

            U8BIT *name = ADB_GetServiceFullName(slist[i], FALSE);
            ci.name = utf8tostringstrippingdvbcodes(name);
            STB_ReleaseUnicodeString(name);

            ci.number = ci.id;

            U16BIT onid, tsid, sid;
            ADB_GetServiceIds(slist[i], &onid, &tsid, &sid);
            ci.onid = onid;
            ci.tsid = tsid;
            ci.sid = sid;
            ci.type = BroadcastChannelInfo::TYPE_OTHER;
            t_ptr = ADB_GetServiceTransportPtr(slist[i]);
            if (t_ptr) {
                switch (ADB_GetTransportSignalType(t_ptr)) {
                case SIGNAL_COFDM: {
                    E_STB_DP_TTYPE terr_type;
                    U32BIT freq_hz;
                    E_STB_DP_TMODE mode;
                    E_STB_DP_TBWIDTH bwidth;
                    U8BIT plp_id;
                    ADB_GetTransportTerrestrialTuningParams(t_ptr, &terr_type, &freq_hz, &mode, &bwidth, &plp_id);
                    switch (terr_type) {
                    case TERR_TYPE_DVBT: ci.type = BroadcastChannelInfo::TYPE_DVB_T; break;
                    case TERR_TYPE_DVBT2: ci.type = BroadcastChannelInfo::TYPE_DVB_T2; break;
                    default:
                        break;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            ci.logoUrl = ""; 
            if (ci.onid == 9018) {
                for (int li = 0; freeviewLogoMap[li].display_name != 0; li++) {
                    if (ci.name == freeviewLogoMap[li].display_name) {
                        if (freeviewLogoMap[li].url != 0) {
                            ALOGE("%s: %s matches %d %s", __FUNCTION__, ci.name.string(), li, freeviewLogoMap[li].url);
                            ci.logoUrl = freeviewLogoMap[li].url; 
                        }
                        break;
                    }
                }
            }

            civ.push_back(ci);
        }
        ADB_ReleaseServiceList(slist, num_entries);
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return civ;
}

static void
buildprog(BroadcastProgramInfo &pi, void *e_ptr, void *s_ptr)
{
    U8BIT *utf8;

    pi.id = String8::format("%u", ADB_GetEventId(e_ptr));
    pi.channel_id = String8::format("%u", ADB_GetServiceLcn(s_ptr));

    utf8 = ADB_GetEventName(e_ptr);
    pi.title = utf8tostringstrippingdvbcodes(utf8);
    STB_ReleaseUnicodeString(utf8);

    utf8 = ADB_GetEventDescription(e_ptr);
    pi.short_description = utf8tostringstrippingdvbcodes(utf8);
    STB_ReleaseUnicodeString(utf8);

    pi.start_time_utc_millis = STB_GCConvertToTimestamp(ADB_GetEventStartDateTime(e_ptr)) * (jlong)1000;
    pi.end_time_utc_millis = STB_GCConvertToTimestamp(ADB_GetEventEndDateTime(e_ptr)) * (jlong)1000;
}

static void
pushprog(Vector<BroadcastProgramInfo> &rv, void *e_ptr, void *s_ptr)
{
    BroadcastProgramInfo pi;
    U8BIT *utf8;

    if (e_ptr == 0)
        return;

    buildprog(pi, e_ptr, s_ptr);
    rv.push_back(pi);
}

static Vector<BroadcastProgramInfo> BroadcastDTVKit_GetProgramList(String8 s8id)
{
    Vector<BroadcastProgramInfo> piv;

    ALOGE("%s: Enter", __FUNCTION__);

    U16BIT lcn = strtoul(s8id.string(), 0, 0);
    void *s_ptr = ADB_FindServiceByLcn(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, lcn, TRUE);
    if (s_ptr) {
#ifdef NOWNEXT
        void *now_event;
        void *next_event;
        ADB_GetNowNextEvents(s_ptr, &now_event, &next_event);
        pushprog(piv, now_event, s_ptr);
        pushprog(piv, next_event, s_ptr);
#else
        void **e_list;
        U16BIT num_e_entries;
        ADB_GetEventSchedule(FALSE, s_ptr, &e_list, &num_e_entries);
        for (unsigned ei = 0; ei < num_e_entries; ei++) {
            pushprog(piv, e_list[ei], s_ptr);
        }
        ADB_ReleaseEventList(e_list, num_e_entries);
#endif
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return piv;
}

#ifdef JOURNAL
static Vector<BroadcastProgramUpdateInfo> BroadcastDTVKit_GetProgramUpdateList()
{
    Vector<BroadcastProgramUpdateInfo> puiv;

    ALOGE("%s: Enter", __FUNCTION__);
    BroadcastProgramUpdateInfo pui;
    UpdateRecord ur;
    bool valid;
    bool empty;
    int backlog = 0;
    int found = 0;
    int requested = 10;
    do {
        STB_OSMutexLock(pSelf->epg.mutex);
        empty = pSelf->epg.queue.empty(); 
        if (!empty) {
            ur = *pSelf->epg.queue.begin();
            pSelf->epg.queue.erase(pSelf->epg.queue.begin());
            pSelf->epg.backlog--;
            backlog = pSelf->epg.backlog;
            valid = true;
        }
        else {
            valid = false;
        }
        STB_OSMutexUnlock(pSelf->epg.mutex);
        if (valid) {
            ALOGE("%s: pop %d %d %d [%d]", __FUNCTION__, ur.type, ur.lcn, ur.event_id, backlog);
            pui.type = ur.type;
            switch (ur.type) {
            case BroadcastProgramUpdateInfo::ClearAll:
                break;
            case BroadcastProgramUpdateInfo::ClearChannel:
                pui.channel_id = String8::format("%u", ur.lcn);
                break;
            case BroadcastProgramUpdateInfo::Delete:
            case BroadcastProgramUpdateInfo::Expire:
                pui.channel_id = String8::format("%u", ur.lcn);
                pui.id = String8::format("%u", ur.event_id);
                break;
            case BroadcastProgramUpdateInfo::Add:
            case BroadcastProgramUpdateInfo::Update: {
                void *s_ptr = ADB_FindServiceByLcn(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, ur.lcn, TRUE);
                if (s_ptr) {
                    void *e_ptr = ADB_GetEvent(s_ptr, ur.event_id);
                    if (e_ptr) {
                        buildprog(pui, e_ptr, s_ptr); 
                    }
                    else {
                        valid = false;
                    }
                }
                else {
                    valid = false;
                }
                break;
            }
            default:
                valid = false;
                break;
            }
            if (valid) {
                puiv.push_back(pui);
                found++; 
            }
        }
    } while (!empty && found < requested);
    ALOGE("%s: Exit", __FUNCTION__);
    return puiv;
}

void
PushUpdate(E_APP_SI_EIT_JOURNAL_TYPE type, BOOLEAN isSchedule, U16BIT allocated_lcn, U16BIT /* onid */, U16BIT /* tsid */, U16BIT /* sid */, U16BIT event_id)
{
    if (isSchedule) {
        UpdateRecord ur;
        bool empty;
        ur.lcn = allocated_lcn;
        ur.event_id = event_id;
        switch (type) {
        case APP_SI_EIT_JOURNAL_TYPE_CLEAR_ALL: ur.type = BroadcastProgramUpdateInfo::ClearAll; break;
        case APP_SI_EIT_JOURNAL_TYPE_CLEAR_SERVICE: ur.type = BroadcastProgramUpdateInfo::ClearChannel; break;
        case APP_SI_EIT_JOURNAL_TYPE_DELETE: ur.type = BroadcastProgramUpdateInfo::Delete; break;
        case APP_SI_EIT_JOURNAL_TYPE_ADD: ur.type = BroadcastProgramUpdateInfo::Add; break;
        case APP_SI_EIT_JOURNAL_TYPE_UPDATE: ur.type = BroadcastProgramUpdateInfo::Update; break;
        case APP_SI_EIT_JOURNAL_TYPE_EXPIRE: ur.type = BroadcastProgramUpdateInfo::Expire; break;
        default:
            return;
        }
        ALOGE("%s: %d %d %d", __FUNCTION__, ur.type, allocated_lcn, event_id);
        STB_OSMutexLock(pSelf->epg.mutex);
        empty = pSelf->epg.queue.empty();
        pSelf->epg.queue.push_back(ur);
        pSelf->epg.backlog++;
        STB_OSMutexUnlock(pSelf->epg.mutex);
        if (empty) {
            TunerHAL_onBroadcastEvent(PROGRAM_UPDATE_LIST_CHANGED, 0, 0);
        }
    }
}
#endif

static int BroadcastDTVKit_Release()
{
    ALOGE("%s: Enter", __FUNCTION__);

    if (pSelf->path != INVALID_RES_ID) {
        BroadcastDTVKit_Stop();
        STB_DPReleasePath(pSelf->path, RES_OWNER_NONE);
        pSelf->path = INVALID_RES_ID;
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

//#define DEBUG_EVENTS
#ifdef DEBUG_EVENTS
static const char *
evcname(unsigned c, unsigned t)
{
    switch (c) {
    case EV_CLASS_HSET: return "EV_CLASS_HSET";
    case EV_CLASS_KEYP: return "EV_CLASS_KEYP";
    case EV_CLASS_TUNE: 
        switch(t) {
        case EV_TYPE_LOCKED: return "STB_EVENT_TUNE_LOCKED";
        case EV_TYPE_NOTLOCKED: return "STB_EVENT_TUNE_NOTLOCKED";
        case EV_TYPE_STOPPED: return "STB_EVENT_TUNE_STOPPED";
        case EV_TYPE_SIGNAL_DATA_BAD: return "STB_EVENT_TUNE_SIGNAL_DATA_BAD";
        case EV_TYPE_SIGNAL_DATA_OK: return "STB_EVENT_TUNE_SIGNAL_DATA_OK";
        default: return "EV_CLASS_TUNE";
        }
        break;
    case EV_CLASS_DECODE:
        switch (t) {
        case EV_TYPE_AD_STARTED: return "STB_EVENT_AD_DECODE_STARTED";
        case EV_TYPE_AD_STOPPED: return "STB_EVENT_AD_DECODE_STOPPED";
        case EV_TYPE_AUDIO_STARTED: return "STB_EVENT_AUDIO_DECODE_STARTED";
        case EV_TYPE_AUDIO_STOPPED: return "STB_EVENT_AUDIO_DECODE_STOPPED";
        case EV_TYPE_AUDIO_UNDERFLOW: return "STB_EVENT_AUDIO_DECODE_UNDERFLOW";
        case EV_TYPE_VIDEO_STARTED: return "STB_EVENT_VIDEO_DECODE_STARTED";
        case EV_TYPE_VIDEO_STOPPED: return "STB_EVENT_VIDEO_DECODE_STOPPED";
        case EV_TYPE_VIDEO_UNDERFLOW: return "STB_EVENT_VIDEO_DECODE_UNDERFLOW";
        case EV_TYPE_SAMPLE_STARTED: return "STB_EVENT_SAMPLE_DECODE_STARTED";
        case EV_TYPE_SAMPLE_STOPPED: return "STB_EVENT_SAMPLE_DECODE_STOPPED";
        case EV_TYPE_LOCKED: return "STB_EVENT_DECODE_LOCKED";
        return "EV_CLASS_DECODE";
        }
        break;
    case EV_CLASS_SEARCH: return "EV_CLASS_SEARCH";
    case EV_CLASS_LNB: return "EV_CLASS_LNB";
    case EV_CLASS_SKEW: return "EV_CLASS_SKEW";
    case EV_CLASS_SCART: return "EV_CLASS_SCART";
    case EV_CLASS_UI: return "EV_CLASS_UI";
    case EV_CLASS_MHEG: return "EV_CLASS_MHEG";
    case EV_CLASS_MHEG_TUNE_REQUEST: return "EV_CLASS_MHEG_TUNE_REQUEST";
    case EV_CLASS_PVR: return "EV_CLASS_PVR";
    case EV_CLASS_TIMER: return "EV_CLASS_TIMER";
    case EV_CLASS_APPLICATION: return "EV_CLASS_APPLICATION";
    case EV_CLASS_DVD: return "EV_CLASS_DVD";
    case EV_CLASS_MHEG_DISPLAY_INFO: return "EV_CLASS_MHEG_DISPLAY_INFO";
    case EV_CLASS_CI: return "EV_CLASS_CI";
    case EV_CLASS_DISK: return "EV_CLASS_DISK";
    case EV_CLASS_HDMI: return "EV_CLASS_HDMI";
    case EV_CLASS_MHEG_STREAMING_REQUEST: return "EV_CLASS_MHEG_STREAMING_REQUEST";
    case EV_CLASS_MHEG_ACTION_REQUEST: return "EV_CLASS_MHEG_ACTION_REQUEST";
    case EV_CLASS_PRIVATE: return "EV_CLASS_PRIVATE";
    }
    return "?";
}
#endif

static bool
scannerUpdate()
{
    STB_OSMutexLock(pSelf->scanner.mutex);
    scannerUpdateUnderLock(false);
    STB_OSMutexUnlock(pSelf->scanner.mutex);
    return true;
}

static void
checkVideoDecodeState(int videoStarted, int *tlchanged, int *tchanged, int *achanged)
{
    if (pSelf->s_ptr) {
        U16BIT vpid = ADB_GetServiceVideoPid(pSelf->s_ptr);
        if (pSelf->vpid != vpid) {
            pSelf->vpid = vpid;
            *tchanged = 1;
        }
        if (pSelf->vpid && videoStarted) {
            U16BIT w;
            U16BIT h;
            E_ASPECT_RATIO ar = STB_VTGetVideoAspectRatio();
            int decoding;

            STB_VTGetVideoResolution(&w, &h);
            switch (ar) {
            case ASPECT_RATIO_4_3:
                w = (h * 4) / 3;
                break;
            case ASPECT_RATIO_16_9:
                w = (h * 16) / 9;
                break;
            default:
                break;
            }

            decoding = w != 0 && h != 0;
            if (pSelf->decoding != decoding) {
                pSelf->decoding = decoding;
                *tlchanged = 1;
                *achanged = 1;
            }
            if (w != pSelf->w || h != pSelf->w) {
                pSelf->w = w;
                pSelf->h = h;
                *tlchanged = 1;
            }
        }
    }
}

static String8
dtvkitThreeLetterCodeToString8(U32BIT dtvkitCode)
{
    return String8::format("%c%c%c", (int)((dtvkitCode >> 16) & 0x7f), (int)((dtvkitCode >> 8) & 0x7f), (int)((dtvkitCode >> 0) & 0x7f));
}

static Vector<BroadcastTrackInfo>
buildBroadcastAudioTrackInfoListFromStreamList(void **streamlist, U16BIT len)
{
    Vector<BroadcastTrackInfo> batil;
    for (unsigned i = 0; i < len; i++) {
        BroadcastTrackInfo bati;
        U16BIT pid = ADB_GetStreamPID(streamlist[i]);
        if (pid == 0) {
            continue;
        }
        bati.type = 0;
        bati.id = String8::format("%u", pid);
        bati.lang = dtvkitThreeLetterCodeToString8(ADB_GetAudioStreamLangCode(streamlist[i]));
        bati.channels = 2;
        bati.sampleRate = 44100;
        batil.push_back(bati);
    }
    return batil;
}

static Vector<BroadcastTrackInfo>
buildBroadcastSubtitleTrackInfoListFromStreamList(void **streamlist, U16BIT len)
{
    Vector<BroadcastTrackInfo> btil;
    for (unsigned i = 0; i < len; i++) {
        BroadcastTrackInfo bti;
        U16BIT pid = ADB_GetStreamPID(streamlist[i]);
        if (pid == 0) {
            continue;
        }
        bti.type = 2;
        bti.id = String8::format("%u", pid);
        bti.lang = dtvkitThreeLetterCodeToString8(ADB_GetSubtitleStreamLangCode(streamlist[i]));
        btil.push_back(bti);
    }
    return btil;
}

static bool
sameTrackInfo(const BroadcastTrackInfo &a, const BroadcastTrackInfo &b)
{
    if (a.type != b.type) {
        return false;
    }
    if (a.id != b.id) {
        return false;
    }
    switch (a.type) {
    case 0:
    case 2:
        // Audio + Subt
        if (a.lang != b.lang) {
            return false;
        }
        break;
    default:
        break;
    }
    switch (a.type) {
    case 1:
        // Video
        if ((a.squarePixelWidth != b.squarePixelWidth) || (a.squarePixelHeight != b.squarePixelHeight) || (a.frameRate != b.frameRate)) {
            return false;
        }
        break;
    default:
        break;
    }
    switch (a.type) {
    case 0:
        // Audio
        if ((a.channels != b.channels) || (a.sampleRate != b.sampleRate)) {
            return false;
        }
        break;
    default:
        break;
    }
    return true;
}

static bool
sameTrackInfoList(const Vector<BroadcastTrackInfo> &a, const Vector<BroadcastTrackInfo> &b)
{
    if (a.size() != b.size()) {
        return 0;
    }
    for (unsigned i = 0; i < a.size(); i++) {
        if (!sameTrackInfo(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

static void
checkAudioDecodeState(int *tlchanged, int *tchanged)
{
    if (pSelf->s_ptr) {
        Vector<BroadcastTrackInfo> batil;
        void **slist;
        U16BIT num_entries;
        ADB_GetStreamList(pSelf->s_ptr, ADB_AUDIO_LIST_STREAM, &slist, &num_entries);
        if (num_entries) {
            batil = buildBroadcastAudioTrackInfoListFromStreamList(slist, num_entries);
        }
        ADB_ReleaseStreamList(slist, num_entries);
        if (!sameTrackInfoList(batil, pSelf->batil)) {
            ALOGD("%s: new audio track list", __FUNCTION__);
            for (unsigned i = 0; i < batil.size(); i++) {
                ALOGD("%s: bati %s %s %d %d", __FUNCTION__, batil[i].id.string(), batil[i].lang.string(), batil[i].channels, batil[i].sampleRate);
            }
            pSelf->batil = batil;
            *tlchanged = 1;
        }
        U16BIT apid = ADB_GetServiceAudioPid(pSelf->s_ptr);
        if (apid != pSelf->apid) {
            ALOGD("%s: new audio track %d", __FUNCTION__, apid);
            pSelf->apid = apid;
            *tchanged = 1;
        }
    }
}

static void
checkSubtitleDecodeState(int *tlchanged, int *tchanged)
{
    if (pSelf->s_ptr) {
        Vector<BroadcastTrackInfo> bstil;
        void **slist;
        U16BIT num_entries;
        ADB_GetStreamList(pSelf->s_ptr, ADB_SUBTITLE_LIST_STREAM, &slist, &num_entries);
        if (num_entries) {
            bstil = buildBroadcastSubtitleTrackInfoListFromStreamList(slist, num_entries);
        }
        ADB_ReleaseStreamList(slist, num_entries);
        if (!sameTrackInfoList(bstil, pSelf->bstil)) {
            ALOGD("%s: new subtitle track list", __FUNCTION__);
            for (unsigned i = 0; i < bstil.size(); i++) {
                ALOGD("%s: bstil %s %s", __FUNCTION__, bstil[i].id.string(), bstil[i].lang.string());
            }
            pSelf->bstil = bstil;
            *tlchanged = 1;
        }

        BOOLEAN running;
        U16BIT spid;
        U16BIT comp_id;
        U16BIT anc_id;
        BOOLEAN enabled;
        STB_SUBReadSettings(&running, &spid, &comp_id, &anc_id, &enabled);

        if (!enabled) {
            spid = 0;
        }

        if (spid != pSelf->spid) {
            ALOGD("%s: new subtitle track %d", __FUNCTION__, spid);
            pSelf->spid = spid;
            *tchanged = 1;
        } else {
            ALOGD("%s: subtitle track %d", __FUNCTION__, spid);
        }
    }
}

static void
updateTrackList(int videoStarted)
{
    int tlchanged = 0;
    int vachanged = 0;
    int atchanged = 0;
    int vtchanged = 0;
    int stchanged = 0;

    checkVideoDecodeState(videoStarted, &tlchanged, &vtchanged, &vachanged);
    checkAudioDecodeState(&tlchanged, &atchanged);
    checkSubtitleDecodeState(&tlchanged, &stchanged);

    if (tlchanged) {
        TunerHAL_onBroadcastEvent(TRACK_LIST_CHANGED, 0, 0);
    }
    if ((tlchanged || vachanged) && pSelf->decoding) {
        TunerHAL_onBroadcastEvent(VIDEO_AVAILABLE, 1, 0);
    }
    if (tlchanged || vtchanged) {
        String8 vpid = String8::format("%u", pSelf->vpid);
        TunerHAL_onBroadcastEvent(TRACK_SELECTED, 1,
                pSelf->vpid > 0 ? &vpid : 0);
    }
    if (tlchanged || atchanged) {
        String8 apid = String8::format("%u", pSelf->apid);
        TunerHAL_onBroadcastEvent(TRACK_SELECTED, 0,
                pSelf->apid > 0 ? &apid : 0);
    }
    if (tlchanged || stchanged) {
        String8 spid = String8::format("%u", pSelf->spid);
        TunerHAL_onBroadcastEvent(TRACK_SELECTED, 2,
                pSelf->spid > 0 ? &spid : 0);
    }
}

static void
event_handler(U32BIT event, void */*event_data*/, U32BIT /*data_size*/)
{
    if (pSelf) {
        int videoStarted = 0;
        int update = 0;
#ifdef DEBUG_EVENTS
        ALOGE("%s: ev %s(%d)/%d", __FUNCTION__, evcname(EVENT_CLASS(event), EVENT_TYPE(event)), EVENT_CLASS(event), EVENT_TYPE(event));
#endif
        if (event == EVENT_CODE(EV_CLASS_UI, EV_TYPE_UPDATE)) {
            scannerUpdate();
        }
#ifndef JOURNAL
#ifdef NOWNEXT
        else if (event == APP_EVENT_SERVICE_EIT_NOW_UPDATE && !pSelf->scanner.active) {
            TunerHAL_onBroadcastEvent(PROGRAM_LIST_CHANGED, 0, 0);
        }
#else
        else if (event == APP_EVENT_SERVICE_EIT_SCHED_UPDATE && !pSelf->scanner.active) {
            TunerHAL_onBroadcastEvent(PROGRAM_LIST_CHANGED, 0, 0);
        }
#endif
#endif
        else if (event == STB_EVENT_VIDEO_DECODE_STARTED) {
            videoStarted = 1;
            update = 1;
        }
        else if (event == STB_EVENT_AUDIO_DECODE_STARTED) {
            update = 1;
        }
        else if (event == APP_EVENT_SERVICE_STREAMS_CHANGED) {
            update = 1;
        }
        else if (event == APP_EVENT_TIME_CHANGED) {
            pSelf->time_valid = true;
        }
        if (update) {
            updateTrackList(videoStarted);
        }
    }
}

BroadcastScanInfo
BroadcastDTVKit_GetScanInfo()
{
    BroadcastScanInfo scanInfo;
    memset(&scanInfo, 0, sizeof(scanInfo));
    STB_OSMutexLock(pSelf->scanner.mutex);

    if (pSelf->scanner.active) {
        pSelf->scanner.progress = ACTL_GetSearchProgress();
        pSelf->scanner.infoValid = true;
    }
    
    if (pSelf->scanner.infoValid) {
        scanInfo.busy = pSelf->scanner.active;
        scanInfo.valid = true;
        scanInfo.progress = pSelf->scanner.progress;
        {
            void **slist = 0;
            U16BIT ns;
            unsigned newCount = 0;
            unsigned newTV = 0;
            unsigned newRadio = 0;
            unsigned newData = 0;
            ADB_GetServiceListIncludingHidden(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, &slist, &ns, true);
            if (slist) {
                U32BIT *newFlag = ADB_GetServiceListNewFlag(slist, ns);
                unsigned i;
                for (i = 0; i < ns; i++) {
                    if (newFlag[i]) {
                        newCount++;
                        switch (ADB_GetServiceType(slist[i])) {
                        case ADB_SERVICE_TYPE_TV:
                        case ADB_SERVICE_TYPE_MPEG2_HD:
                        case ADB_SERVICE_TYPE_AVC_SD_TV:
                        case ADB_SERVICE_TYPE_HD_TV:
                        case ADB_SERVICE_TYPE_UHD_TV:
                            newTV++;
                            break;
                        case ADB_SERVICE_TYPE_RADIO:
                        case ADB_SERVICE_TYPE_AVC_RADIO:
                            newRadio++;
                            break;
                        case ADB_SERVICE_TYPE_DATA:
                            newData++;
                            break;
                        default:
                            break;
                        }
                    }
                }
                STB_AppFreeMemory(newFlag);
                ADB_ReleaseServiceList(slist, ns);
            }
            scanInfo.channels = newCount;
            scanInfo.TVChannels = newTV;
            scanInfo.radioChannels = newRadio;
            scanInfo.dataChannels = newData;
        }
        {
            U8BIT path = ACTL_GetServiceSearchPath();
            if (path != INVALID_RES_ID)
            {
                U8BIT tuner_path = STB_DPGetPathTuner(path);
                if (tuner_path != INVALID_RES_ID)
                {
                    scanInfo.signalStrengthPercent = STB_TuneGetSignalStrength(tuner_path);
                    scanInfo.signalQualityPercent = STB_TuneGetDataIntegrity(tuner_path);
                }
            }
        }
#if 0
        scanInfo.setSatelliteId(pSelf->scanner.artemisInfo.Satellite);
        scanInfo.setSatellites(pSelf->scanner.artemisInfo.NoOfSatellites);
        scanInfo.setScannedSatellites(pSelf->scanner.artemisInfo.NoOfScannedSatellites);
        scanInfo.setTransponders(pSelf->scanner.artemisInfo.NoOfTransponders);
        scanInfo.setScannedTransponders(pSelf->scanner.artemisInfo.NoOfScannedTransponders);
#endif
    } else {
        scanInfo.valid = false;
    }
    
    STB_OSMutexUnlock(pSelf->scanner.mutex);
    return scanInfo;
}

jlong
BroadcastDTVKit_GetUtcTime()
{
    U32BIT now;
    if (!pSelf->time_valid) {
        return 0;
    }
    now = STB_GCConvertToTimestamp(STB_GCNowDHMSGmt());
    ALOGE("%s: %d", __FUNCTION__, now);
    return (jlong)now;
}

int
BroadcastDTVKit_SetGeometry(BroadcastRect position, BroadcastRect /*clipRect*/,
        jshort gfxWidth, jshort gfxHeight, jshort /*zorder*/,
        jboolean /*visible*/)
{
    if ((position.y == 0 && position.h == gfxHeight) || (position.x == 0 && position.w == gfxWidth)) {
        // fullscreen - let DTVKit choose the window - hopefully it gets the same answer
        ALOGE("%s: fullscreen", __FUNCTION__);
        ACTL_SetVideoWindow(0, 0, 0, 0); 
    }
    else {
        ALOGE("%s: app scaling", __FUNCTION__);
        ACTL_SetVideoWindow(position.x, position.y, position.w, position.h);
    }
    return 0;
}

Vector<BroadcastTrackInfo>
BroadcastDTVKit_GetTrackInfoList()
{
    Vector<BroadcastTrackInfo> v;

    if (pSelf->h > 0) {
        BroadcastTrackInfo info;
        info.type = 1;
        info.id = String8::format("%u", pSelf->vpid);
        info.squarePixelHeight = pSelf->h; 
        info.squarePixelWidth = pSelf->w;
        switch (0) {
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

        ALOGE("%s: (%dx%d)", __FUNCTION__, info.squarePixelWidth, info.squarePixelHeight);
        v.push_back(info);
    }
    v.appendVector(pSelf->batil);
    v.appendVector(pSelf->bstil);
    return v; 
}

static int
BroadcastDTVKit_SelectAudioTrack(int uid)
{
    if (uid < 0) {
        ADB_SetReqdAudioStreamSettings(pSelf->s_ptr, FALSE, 0, ADB_AUDIO_TYPE_UNDEFINED, ADB_AUDIO_STREAM);
    }
    else {
        void **streamlist;
        U16BIT num_entries;
        unsigned i;
        ADB_GetStreamList(pSelf->s_ptr, ADB_AUDIO_LIST_STREAM, &streamlist, &num_entries);
        for (i = 0; i < num_entries; i++) {
            if (ADB_GetStreamPID(streamlist[i]) == uid) {
                break;
            }
        }
        if (i < num_entries) {
            ADB_SetReqdAudioStreamSettings(pSelf->s_ptr, TRUE,
                ADB_GetAudioStreamLangCode(streamlist[i]),
                ADB_GetAudioStreamType(streamlist[i]),
                ADB_GetStreamType(streamlist[i]));
        }
        else {
            ADB_SetReqdAudioStreamSettings(pSelf->s_ptr, FALSE, 0, ADB_AUDIO_TYPE_UNDEFINED, ADB_AUDIO_STREAM);
        }
        ADB_ReleaseStreamList(streamlist, num_entries);
    }
    ACTL_ReTuneAudio();
    return 0;
}

static int
BroadcastDTVKit_SelectSubtitleTrack(int uid)
{
    ALOGE("%s: pid %d", __FUNCTION__, uid);
    if (uid < 0) {
        ADB_SetReqdSubtitleStreamSettings(pSelf->s_ptr, FALSE, 0, ADB_SUBTITLE_TYPE_DVB);
        void ACTL_PauseSubtitles(void);
    }
    else {
        void **streamlist;
        U16BIT num_entries;
        unsigned i;
        ADB_GetStreamList(pSelf->s_ptr, ADB_SUBTITLE_LIST_STREAM, &streamlist, &num_entries);
        for (i = 0; i < num_entries; i++) {
            if (ADB_GetStreamPID(streamlist[i]) == uid) {
                break;
            }
        }
        if (i < num_entries) {
            ALOGE("%s: pid %d found, selecting", __FUNCTION__, uid);
            ADB_SetReqdSubtitleStreamSettings(pSelf->s_ptr, TRUE,
                ADB_GetSubtitleStreamLangCode(streamlist[i]),
                ADB_GetSubtitleStreamType(streamlist[i]));
        }
        else {
            ADB_SetReqdSubtitleStreamSettings(pSelf->s_ptr, FALSE, 0, ADB_SUBTITLE_TYPE_DVB);
        }
        ADB_ReleaseStreamList(streamlist, num_entries);
        void ACTL_ResumeSubtitles(void);
        updateTrackList(0);
    }
    return 0;
}

int
BroadcastDTVKit_SelectTrack(int type, const String8 *id)
{
    int result = -1;

    ALOGD("%s: %d %s", __FUNCTION__, type, id ? id->string() : "NULL");
    if (pSelf->s_ptr) {
        int uid = id ? strtol(id->string(), 0, 0) : -1;
        switch (type) {
        case 0:
            result = BroadcastDTVKit_SelectAudioTrack(uid);
            break;
        case 2:
            result = BroadcastDTVKit_SelectSubtitleTrack(uid);
            break;
        case 1: //video track
        default:
            break;
        }
    }
    return result;
}

void
BroadcastDTVKit_SetCaptionEnabled(bool enabled)
{
    ALOGD("%s: %s", __FUNCTION__, enabled ? "enabled" : "disabled");
    if (enabled) {
        ACTL_ResumeSubtitles();
    } else {
        ACTL_PauseSubtitles();
    }
}

int
Broadcast_Initialize(BroadcastDriver *pD)
{
    ALOGE("%s: Enter", __FUNCTION__);

    pSelf = new BroadcastDTVKit_Context;

    DTVKitPlatform_SetNVMBasePath("/data/data/com.broadcom.tvinput");
    APP_InitialiseDVB(event_handler);
#ifdef JOURNAL
    ASI_SetEITScheduleLimit(2 * 24);
    ASI_SetEitJournalFunction(PushUpdate);
#else
    ASI_SetEITScheduleLimit(1 * 24);
#endif

    U16BIT services = ADB_GetNumServicesInList(ADB_SERVICE_LIST_ALL, true);
    if (services == 0) {
        ACFG_SetCountry(COUNTRY_CODE_UK); 
    }
    else {
        pSelf->tot_search_active = ACTL_StartTotSearch();
    }

    pD->GetChannelList = BroadcastDTVKit_GetChannelList;
    pD->GetProgramList = BroadcastDTVKit_GetProgramList;
#ifdef JOURNAL
    pD->GetProgramUpdateList = BroadcastDTVKit_GetProgramUpdateList;
#else
    pD->GetProgramUpdateList = 0;
#endif
    pD->GetScanInfo = BroadcastDTVKit_GetScanInfo;
    pD->GetUtcTime = BroadcastDTVKit_GetUtcTime;
    pD->Tune = BroadcastDTVKit_Tune;
    pD->StartScan = BroadcastDTVKit_StartScan;
    pD->StopScan = BroadcastDTVKit_StopScan;
    pD->Stop = BroadcastDTVKit_Stop;
    pD->Release = BroadcastDTVKit_Release;
    pD->SetGeometry = BroadcastDTVKit_SetGeometry;
    pD->GetTrackInfoList = BroadcastDTVKit_GetTrackInfoList;
    pD->SelectTrack = BroadcastDTVKit_SelectTrack;
    pD->SetCaptionEnabled = BroadcastDTVKit_SetCaptionEnabled;
    BDBG_SetModuleLevel("stbhwav", BDBG_eMsg);
    //BDBG_SetModuleLevel("stbhwosd", BDBG_eMsg);
    //BDBG_SetModuleLevel("stbhwosd_nsc", BDBG_eMsg);
    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

