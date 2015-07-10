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

#define JOURNAL
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
        scanner_mutex = STB_OSCreateMutex();
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
    void *scanner_mutex;

    class Scanner {
    public:
        Scanner() :
            m_state(INACTIVE),
            m_infoValid(false),
            m_progress(0)
        {}

        void start(bool manual) {
            m_state = manual ? MANUAL : AUTOMATIC;
            m_infoValid = false;
        }
        void stop() {
            m_state = INACTIVE;
            // preserve m_infoValid flag
        }

        bool active() const { return m_state != INACTIVE; }
        bool manual() const { return m_state == MANUAL; }
        bool automatic() const { return m_state == AUTOMATIC; }

        bool infoValid() const { return m_infoValid; }
        jchar progress() const { return m_progress; }
        void setProgress(jchar progress) {
            m_progress = progress;
            m_infoValid = true;
        };
    private:
        enum State {
            INACTIVE = 0,
            MANUAL,
            AUTOMATIC
        };
        State m_state;
        bool m_infoValid;
        jchar m_progress;
    };

    Scanner scanner;
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
    if (!pSelf->scanner.active()) {
        return false;
    }

    pSelf->scanner.setProgress(ACTL_GetSearchProgress());

    if (!stop) {
        onScanProgress(pSelf->scanner.progress());
    }

    bool justCompleted = false;
    bool manual = pSelf->scanner.manual();
    if (ACTL_IsSearchComplete()) {
        pSelf->scanner.stop();
        justCompleted = true;
    }

    if (stop && pSelf->scanner.active()) {
        pSelf->scanner.stop();
        if (manual) {
            ACTL_FinishManualSearch();
        }
        else {
            ACTL_StopServiceSearch();
        }
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

    STB_OSMutexLock(pSelf->scanner_mutex);
    if (!scannerInitUnderLock()) {
        STB_OSMutexUnlock(pSelf->scanner_mutex);
        return false;
    }

    // manual scan with network
    static const BOOLEAN retune = TRUE;
    static const BOOLEAN manual_search = FALSE;
    ADB_PrepareDatabaseForSearch(SIGNAL_COFDM, NULL, retune, manual_search);
    if (!ACTL_StartServiceSearch(SIGNAL_COFDM, ACTL_FREQ_SEARCH)) {
        STB_OSMutexUnlock(pSelf->scanner_mutex);
        return false;
    }
    pSelf->scanner.start(manual_search);
    STB_OSMutexUnlock(pSelf->scanner_mutex);
    onScanStart();
    return true;
}

static bool
satelliteTuningParamsToDTVKit(BroadcastScanParams *pParams, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    dtvkitParams.freq = pParams->freqKHz / 1000;
    dtvkitParams.u.sat.satellite = 0;
    dtvkitParams.u.sat.polarity = pParams->satellitePolarity == BroadcastScanParams::SatellitePolarity_Horizontal ? POLARITY_HORIZONTAL : POLARITY_VERTICAL;
    dtvkitParams.u.sat.symbol_rate = pParams->symK;
    switch (pParams->codeRateNumerator * 100 + pParams->codeRateDenominator) {
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
        ALOGE("%s: Unsupported FEC: %d/%d", __FUNCTION__,
                (int)pParams->codeRateNumerator,
                (int)pParams->codeRateDenominator);
        return false;
    }; 
    switch (pParams->satelliteMode) {
    case BroadcastScanParams::SatelliteMode_SatQpskLdpc: dtvkitParams.u.sat.dvb_s2 = true; dtvkitParams.u.sat.modulation = MOD_QPSK; break;
    case BroadcastScanParams::SatelliteMode_Sat8pskLdpc: dtvkitParams.u.sat.dvb_s2 = true; dtvkitParams.u.sat.modulation = MOD_8PSK; break;
    case BroadcastScanParams::SatelliteMode_SatDvb: dtvkitParams.u.sat.dvb_s2 = false; dtvkitParams.u.sat.modulation = MOD_QPSK; break;
    default:
        ALOGE("%s: Unsupported satellite mode: %d", __FUNCTION__,
                (int)pParams->satelliteMode);
        return false;
    }
    return true;
}

static bool
qamTuningParamsToDTVKit(BroadcastScanParams *pParams, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    dtvkitParams.freq = pParams->freqKHz;
    switch (pParams->qamMode) {
    case BroadcastScanParams::QamMode_Qam16: dtvkitParams.u.cab.mode = MODE_QAM_16; break;
    case BroadcastScanParams::QamMode_Qam32: dtvkitParams.u.cab.mode = MODE_QAM_32; break;
    case BroadcastScanParams::QamMode_Qam64: dtvkitParams.u.cab.mode = MODE_QAM_64; break;
    case BroadcastScanParams::QamMode_Qam128: dtvkitParams.u.cab.mode = MODE_QAM_128; break;
    case BroadcastScanParams::QamMode_Qam256: dtvkitParams.u.cab.mode = MODE_QAM_256; break;
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
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm1k: dtvkitParams.u.terr.mode = MODE_COFDM_1K; break;
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm2k: dtvkitParams.u.terr.mode = MODE_COFDM_2K; break;
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm4k: dtvkitParams.u.terr.mode = MODE_COFDM_4K; break;
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm8k: dtvkitParams.u.terr.mode = MODE_COFDM_8K; break;
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm16k: dtvkitParams.u.terr.mode = MODE_COFDM_16K; break;
    case BroadcastScanParams::OfdmTransmissionMode_Ofdm32k: dtvkitParams.u.terr.mode = MODE_COFDM_32K; break;
    default: dtvkitParams.u.terr.mode = MODE_COFDM_UNDEFINED; break;
    }
    switch (pParams->ofdmMode) {
    case BroadcastScanParams::OfdmMode_OfdmDvbt: dtvkitParams.u.terr.type = TERR_TYPE_DVBT; break;
    case BroadcastScanParams::OfdmMode_OfdmDvbt2: dtvkitParams.u.terr.type = TERR_TYPE_DVBT2; break;
    default: return false;
    }
    dtvkitParams.u.terr.plp_id = pParams->plpId;
    return true;
}

static bool
tuningParamsToDTVKit(BroadcastScanParams *pParams, E_STB_DP_SIGNAL_TYPE &tunerType, S_MANUAL_TUNING_PARAMS &dtvkitParams)
{
    switch (pParams->deliverySystem) {
    case BroadcastScanParams::DeliverySystem_Dvbt:
        tunerType = SIGNAL_COFDM;
        return ofdmTuningParamsToDTVKit(pParams, dtvkitParams);
    case BroadcastScanParams::DeliverySystem_Dvbs:
        tunerType = SIGNAL_QPSK;
        return satelliteTuningParamsToDTVKit(pParams, dtvkitParams);
    case BroadcastScanParams::DeliverySystem_Dvbc:
        tunerType = SIGNAL_QAM;
        return qamTuningParamsToDTVKit(pParams, dtvkitParams);
    default:
        break;
    }
    return false;
}

static bool
startManualScan(BroadcastScanParams *pParams)
{
    S_MANUAL_TUNING_PARAMS dtvkitParams;
    E_STB_DP_SIGNAL_TYPE tunerType;

    ALOGI("%s: Enter", __FUNCTION__);

    if (!tuningParamsToDTVKit(pParams, tunerType, dtvkitParams)) {
        ALOGE("%s: Exit - unable to convert parameters", __FUNCTION__);
        return false;
    }

    if (pSelf->tot_search_active) {
        ACTL_StopTotSearch();
        pSelf->tot_search_active = false;
    }

    STB_OSMutexLock(pSelf->scanner_mutex);

    if (!scannerInitUnderLock()) {
        STB_OSMutexUnlock(pSelf->scanner_mutex);
        ALOGE("%s: Exit - unable to init scanner", __FUNCTION__);
        return false;
    }

    // manual scan with network
    static const BOOLEAN retune = pParams->scanMode != BroadcastScanParams::ScanMode_Single;
    static const BOOLEAN manual_search = TRUE;
    ADB_PrepareDatabaseForSearch(tunerType, NULL, retune, manual_search);
    if (!ACTL_StartManualSearch(tunerType, &dtvkitParams, pParams->scanMode == BroadcastScanParams::ScanMode_Home ? ACTL_NETWORK_SEARCH : ACTL_FREQ_SEARCH)) {
        STB_OSMutexUnlock(pSelf->scanner_mutex);
        ALOGE("%s: Exit - unable to start scan", __FUNCTION__);
        return false;
    }
    pSelf->scanner.start(manual_search);
    STB_OSMutexUnlock(pSelf->scanner_mutex);
    onScanStart();

    ALOGI("%s: Exit - success", __FUNCTION__);
    return true;
}

static int BroadcastDTVKit_StartScan(BroadcastScanParams *pParams)
{
    int rv = -1;
    ALOGE("%s: Enter", __FUNCTION__); 

    if (pParams == 0 || (pParams->deliverySystem == BroadcastScanParams::DeliverySystem_Dvbt && pParams->scanMode == BroadcastScanParams::ScanMode_Blind)) {
        ALOGI("%s: DVB-T blind scan", __FUNCTION__);
        startBlindScan();
        rv = 0;
    }
    else if (pParams->scanMode == BroadcastScanParams::ScanMode_Blind) {
        ALOGE("%s: no other blind scan", __FUNCTION__);
        rv = -1;
    }
    else {
        ALOGI("%s: DVB-T/S/C manual scan", __FUNCTION__);
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

    STB_OSMutexLock(pSelf->scanner_mutex);

    if (scannerStopUnderLock()) {
        rv = 0;
    }

    STB_OSMutexUnlock(pSelf->scanner_mutex);
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
{ "BBC ONE West", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc1.png" },
{ "BBC TWO", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc2.png" },
{ "ITV", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv.png" },
{ "Channel 4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c4.png" },
{ "Channel 5", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c5.png"},
{ "ITV2", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv2.png"},
{ "BBC THREE", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc3.png"},
{ "Made In Bristol", "http://www.freeview.co.uk/wp-content/uploads/2014/10/Made-in-Bristol.png"},
{ "BBC FOUR", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc4.png"},
{ "ITV3", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv3.png"},
{ "Pick", "http://www.freeview.co.uk/wp-content/uploads/2013/10/pick.png"},
{ "Dave", "http://www.freeview.co.uk/wp-content/uploads/2013/10/dave.png"},
{ "Channel 4+1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c4+1.png"},
{ "More 4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/more4.png"},
{ "Film4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/film4.png"},
{ "QVC", "http://www.freeview.co.uk/wp-content/uploads/2013/10/qvc.png"},
{ "Really", "http://www.freeview.co.uk/wp-content/uploads/2013/10/really.png"},
{ "4Music", "http://www.freeview.co.uk/wp-content/uploads/2013/10/4music.png"},
{ "Yesterday", "http://www.freeview.co.uk/wp-content/uploads/2013/10/yesterday.png"},
{ "Drama", "http://www.freeview.co.uk/wp-content/uploads/2013/10/drama.png"},
{ "VIVA", "http://www.freeview.co.uk/wp-content/uploads/2013/10/viva.png"},
{ "Ideal World", "http://www.freeview.co.uk/wp-content/uploads/2013/10/ideal_world.png"},
{ "ITV4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv4.png"},
{ "Dave ja vu", "http://www.freeview.co.uk/wp-content/uploads/2014/12/dave-ja-vu-for-website1.png"},
{ "ITVBe", "http://www.freeview.co.uk/wp-content/uploads/2014/10/itv_be.png"},
{ "ITV2 +1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv2+1.png"},
{ "E4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/e4.png"},
{ "E4+1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/e4+1.png"},
{ "5*", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c5star.png"},
{ "5 USA", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c5usa.png"},
{ "Spike", "http://www.freeview.co.uk/wp-content/uploads/2015/04/spike.png"},
{ "Movie Mix", "http://www.freeview.co.uk/wp-content/uploads/2015/01/movie-mix.png" },
{ "ITV +1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itv+1.png"},
{ "ITV3+1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/Itv3+11.png"},
{ "QVC Beauty", "http://www.freeview.co.uk/wp-content/uploads/2013/10/qvc_beauty.png"},
{ "Create & Craft", "http://www.freeview.co.uk/wp-content/uploads/2013/10/create_and_craft.png"},
{ "QUEST", "http://www.freeview.co.uk/wp-content/uploads/2014/04/quest.png"},
{ "QUEST+1", "http://www.freeview.co.uk/wp-content/uploads/2014/04/quest+1.png"},
{ "The Store", "http://www.freeview.co.uk/wp-content/uploads/2014/01/the_store_website-logo.png" },
{ "Rocks & Co 1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/rc.png"},
{ "Food Network", "http://www.freeview.co.uk/wp-content/uploads/2013/10/food_network.png"},
{ "Travel Channel", "http://www.freeview.co.uk/wp-content/uploads/2013/10/travel_channel.png"},
{ "Gems TV", "http://www.freeview.co.uk/wp-content/uploads/2014/06/gems.png"},
{ "Channel 5+1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/c5+1.png"},
{ "Film4+1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/film4+1.png"},
{ "Challenge", "http://www.freeview.co.uk/wp-content/uploads/2013/10/challenge.png"},
{ "4seven", "http://www.freeview.co.uk/wp-content/uploads/2013/10/247.png"},
{ "movies4men", "http://www.freeview.co.uk/wp-content/uploads/2014/04/movies4men_website.png"},
{ "TJC", "http://www.freeview.co.uk/wp-content/uploads/2013/10/jewellery_channel.png" },
{ "Channel 5+24", "http://www.freeview.co.uk/wp-content/uploads/2014/02/c5+24-for-website.png"},
{ "QVC EXTRA", 0},
{ "BT Sport 1", 0},
{ "BT Showcase", 0 },
{ "True Entertainment", "http://www.freeview.co.uk/wp-content/uploads/2013/10/true_entertainment.png"},
{ "ITV4+1", 0},
{ "Community", "http://www.freeview.co.uk/wp-content/uploads/2014/01/community_channel_website.png"},
{ "TBN UK", "http://www.freeview.co.uk/wp-content/uploads/2015/01/tbn-uk.png" },
{ "CBS Reality", "http://www.freeview.co.uk/wp-content/uploads/2014/04/cbs_reality_website.png"},
{ "truTV", "http://www.freeview.co.uk/wp-content/uploads/2014/07/tru_tv_website.png"},
{ "truTV+1", "http://www.freeview.co.uk/wp-content/uploads/2014/09/tru_tv_plus1.png"},
{ "Horror Channel", "http://www.freeview.co.uk/wp-content/uploads/2015/03/horror-channel.png"},
{ "CBS Action", "http://www.freeview.co.uk/wp-content/uploads/2014/08/cbs_action-for-website.png"},
{ "Motors TV", "http://www.freeview.co.uk/wp-content/uploads/2014/10/motorstv_website.png"},
{ "ITVBe+1", 0},
{ "DAYSTAR", "http://www.freeview.co.uk/wp-content/uploads/2015/01/daystar1.png"},
{ "BBC ONE HD", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc1hd.png" },
{ "BBC TWO HD", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc2hd.png" },
{ "ITV HD", "http://www.freeview.co.uk/wp-content/uploads/2013/10/itvhd.png" },
{ "Channel 4 HD", "http://www.freeview.co.uk/wp-content/uploads/2013/10/4hd.png" },
{ "BBC THREE HD", "http://www.freeview.co.uk/wp-content/uploads/2013/12/bbc3hd.png" },
{ "BBC FOUR HD", "http://www.freeview.co.uk/wp-content/uploads/2013/12/bbc4hd.png" },
{ "BBC NEWS HD", "http://www.freeview.co.uk/wp-content/uploads/2013/12/bbc_news_hd.png" },
{ "Al Jazeera Eng HD", "http://www.freeview.co.uk/wp-content/uploads/2013/11/aljazeera_hd.png" },
{ "Community HD", 0 },
{ "Channel 4+1 HD", "http://www.freeview.co.uk/wp-content/uploads/2014/06/c4+1_hd.png" },
{ "4seven HD", "http://www.freeview.co.uk/wp-content/uploads/2014/06/4seven_hd.png" },
{ "CBBC", "http://www.freeview.co.uk/wp-content/uploads/2013/10/cbbc.png"},
{ "CBeebies", "http://www.freeview.co.uk/wp-content/uploads/2013/10/cbeebies.png"},
{ "CITV", "http://www.freeview.co.uk/wp-content/uploads/2013/10/citv.png"},
{ "CBBC HD", "http://www.freeview.co.uk/wp-content/uploads/2013/12/cbbc_hd.png" },
{ "CBeebies HD", "http://www.freeview.co.uk/wp-content/uploads/2013/12/cbeebies_hd.png" },
{ "POP", "http://www.freeview.co.uk/wp-content/uploads/2014/03/Pop_website.png"},
{ "Tiny Pop", "http://www.freeview.co.uk/wp-content/uploads/2014/11/tinypop.png"},
{ "BBC NEWS", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_news.png"},
{ "BBC Parliament", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_parliament.png"},
{ "Sky News", "http://www.freeview.co.uk/wp-content/uploads/2013/10/sky_news.png"},
{ "Al Jazeera Eng", "http://www.freeview.co.uk/wp-content/uploads/2014/11/AJ_English-Logo.png"},
{ "Al Jazeera Arabic", "http://www.freeview.co.uk/wp-content/uploads/2014/11/AJ_Arabic-Logo.png"},
{ "RT", "http://www.freeview.co.uk/wp-content/uploads/2013/10/rt.png"},
{ "ARISE News", "http://www.freeview.co.uk/wp-content/uploads/2015/01/arise-news.png"},
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
{ "BBC Red Button", "http://www.freeview.co.uk/wp-content/uploads/2013/10/red_button.png" },
{ "BBC RB 1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/red_button.png" },
{ "BBC Radio 1", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_1.png" },
{ "BBC R1X", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_1extra.png" },
{ "BBC Radio 2", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_2.png" },
{ "BBC Radio 3", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_3.png" },
{ "BBC Radio 4", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_4.png" },
{ "BBC R5L", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_5live.png" },
{ "BBC R5SX", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_5live_sports_extra.png" },
{ "BBC 6 Music", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_6music.png" },
{ "BBC Radio 4 Ex", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_4extra.png" },
{ "BBC Asian Net.", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_asian_network.png" },
{ "BBC World Sv.", "http://www.freeview.co.uk/wp-content/uploads/2013/10/bbc_radio_world_service.png" },
{ "The Hits Radio", "http://www.freeview.co.uk/wp-content/uploads/2013/10/the_hits.png" },
{ "KISS FRESH", "http://www.freeview.co.uk/wp-content/uploads/2013/10/kiss_fresh.png" },
{ "Kiss", "http://www.freeview.co.uk/wp-content/uploads/2013/03/KISS-Radio.jpg" },
{ "KISSTORY", "http://www.freeview.co.uk/wp-content/uploads/2013/10/kisstory.png" },
{ "Magic", "http://www.freeview.co.uk/wp-content/uploads/2014/09/magic_new.png" },
{ "heat", "http://www.freeview.co.uk/wp-content/uploads/2013/10/heat.png" },
{ "Kerrang!", "http://www.freeview.co.uk/wp-content/uploads/2013/10/kerrang.png" },
{ "SMOOTH RADIO", "http://www.freeview.co.uk/wp-content/uploads/2015/02/smooth-radio.png" },
{ "BBC Bristol", 0 },
{ "talkSPORT", "http://www.freeview.co.uk/wp-content/uploads/2013/10/talk_sport.png" },
{ "Capital FM", "http://www.freeview.co.uk/wp-content/uploads/2013/10/capital_fm.png" },
{ "Premier Radio", 0 },
{ "Absolute Radio", "http://www.freeview.co.uk/wp-content/uploads/2013/10/absolute_radio.png" },
{ "Heart", "http://www.freeview.co.uk/wp-content/uploads/2013/10/heart.png" },
{ "Insight Radio", "http://www.freeview.co.uk/wp-content/uploads/2014/04/Insight_Radio_website.png" },
{ "Classic FM", "http://www.freeview.co.uk/wp-content/uploads/2015/02/classic-fm.png" },
{ "LBC", "http://www.freeview.co.uk/wp-content/uploads/2015/02/lbc.png" },
{ "Trans World Radio", 0 },
{ "BT Showcase", 0 },
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
static Vector<BroadcastProgramUpdateInfo> BroadcastDTVKit_GetProgramUpdateList(jint limit)
{
    Vector<BroadcastProgramUpdateInfo> puiv;

    ALOGE("%s: Enter", __FUNCTION__);
    BroadcastProgramUpdateInfo pui;
    UpdateRecord ur;
    bool valid;
    bool empty;
    int backlog = 0;
    int found = 0;
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
    } while (!empty && (!limit || (found < limit)));
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
        case APP_SI_EIT_JOURNAL_TYPE_CLEAR: ur.type = BroadcastProgramUpdateInfo::ClearChannel; break;
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
    STB_OSMutexLock(pSelf->scanner_mutex);
    scannerUpdateUnderLock(false);
    STB_OSMutexUnlock(pSelf->scanner_mutex);
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
#ifndef JOURNAL
event_handler(U32BIT event, void */*event_data*/, U32BIT /*data_size*/)
#else
event_handler(U32BIT event, void *event_data, U32BIT /*data_size*/)
#endif
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
        else if (event == APP_EVENT_SERVICE_EIT_NOW_UPDATE && !pSelf->scanner.active()) {
            TunerHAL_onBroadcastEvent(PROGRAM_LIST_CHANGED, 0, 0);
        }
#else
        else if (event == APP_EVENT_SERVICE_EIT_SCHED_UPDATE && !pSelf->scanner.active()) {
            TunerHAL_onBroadcastEvent(PROGRAM_LIST_CHANGED, 0, 0);
        }
#endif
#else
        if (event == APP_EVENT_SERVICE_EIT_SCHED_UPDATE && !pSelf->scanner.active()) {
           S_APP_SI_EIT_SCHED_UPDATE *update = (S_APP_SI_EIT_SCHED_UPDATE *)event_data;
           PushUpdate(update->type, update->is_sched, update->allocated_lcn,
                 update->orig_net_id, update->tran_id, update->serv_id,
                 update->event_id);
        }
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
    STB_OSMutexLock(pSelf->scanner_mutex);

    if (pSelf->scanner.active()) {
        pSelf->scanner.setProgress(ACTL_GetSearchProgress());
    }
    
    if (pSelf->scanner.infoValid()) {
        scanInfo.busy = pSelf->scanner.active();
        scanInfo.valid = true;
        scanInfo.progress = pSelf->scanner.progress();
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
    
    STB_OSMutexUnlock(pSelf->scanner_mutex);
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
    if (!APP_InitialiseDVB(event_handler)) {
        ALOGE("%s: Failed to initialise the DVB stack", __FUNCTION__);
        return -1;
    }
#ifdef JOURNAL
    ASI_SetEITScheduleLimit(2 * 24);
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

