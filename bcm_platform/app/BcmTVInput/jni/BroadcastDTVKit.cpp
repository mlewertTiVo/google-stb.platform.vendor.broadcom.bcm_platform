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
#include "dtvkit_platform.h"
};

class BroadcastDTVKit_Context {
public:
    BroadcastDTVKit_Context() {
        scanner.active = false;
        scanner.infoValid = false;
        scanner.mutex = STB_OSCreateMutex();
        path = INVALID_RES_ID;
    };
    struct {
        void *mutex;
        bool active;
        bool infoValid;
        jchar progress;
    } scanner;
    U8BIT path;
};

static BroadcastDTVKit_Context *pSelf;

static int BroadcastDTVKit_Stop()
{
    ALOGE("%s: Enter", __FUNCTION__);
    if (pSelf->path != INVALID_RES_ID) {
        ALOGE("%s: Stopping", __FUNCTION__); 
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
    TunerHAL_onBroadcastEvent(100 + progress);
}

static void
onScanComplete()
{
    TunerHAL_onBroadcastEvent(201);
    TunerHAL_onBroadcastEvent(0);
}

static void
onScanStart()
{
    TunerHAL_onBroadcastEvent(99);
}

static bool
scannerUpdateUnderLock(bool stop)
{
    if (!pSelf->scanner.active) {
        return false;
    }

    jbyte lastNotifiedProgress = pSelf->scanner.infoValid ? pSelf->scanner.progress : 101;

    pSelf->scanner.progress = ACTL_GetSearchProgress();
    pSelf->scanner.infoValid = true;

    if (!stop && pSelf->scanner.progress != lastNotifiedProgress) {
        onScanProgress(pSelf->scanner.progress);
    }

    if (ACTL_IsSearchComplete()) {
        pSelf->scanner.active = false;
        if (ACTL_IsTargetRegionRequired()) {
            printf("target region required\n");
        }
        ACTL_CompleteServiceSearch();
        //pSelf->tuningParams.biList.clear();
        onScanComplete();
    }

    if (stop && pSelf->scanner.active) {
        pSelf->scanner.active =  false;
        ACTL_StopServiceSearch();
        //pSelf->tuningParams.biList.clear();
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

static int BroadcastDTVKit_Scan()
{
    int rv = -1;
    ALOGE("%s: Enter", __FUNCTION__); 
    if (startBlindScan()) {
        rv = 0;
    }
    ALOGE("%s: Exit", __FUNCTION__);
    return rv;
}

static int BroadcastDTVKit_Tune(String8 s8id)
{
    int rv = -1;

    ALOGE("%s: Enter", __FUNCTION__);
    U16BIT lcn = strtoul(s8id.string(), 0, 0);
    void *s_ptr = ADB_FindServiceByLcn(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, lcn, TRUE);
    if (s_ptr) {
        pSelf->path = ACTL_TuneToService(pSelf->path, NULL, s_ptr, TRUE, TRUE);
        if (pSelf->path == INVALID_RES_ID) {
            ALOGE("%s: Invalid resource", __FUNCTION__);
        }
        else {
            ALOGE("%s: Tuning", __FUNCTION__); 
            rv = 0;
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

            civ.push_back(ci);
        }
        ADB_ReleaseServiceList(slist, num_entries);
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return civ;
}

static void
pushprog(Vector<BroadcastProgramInfo> &rv, void *e_ptr, void *s_ptr)
{
    BroadcastProgramInfo pi;
    U8BIT *utf8;

    if (e_ptr == 0)
        return;

    pi.id = String8::format("%u", ADB_GetEventId(e_ptr));
    pi.channel_id = String8::format("%u", ADB_GetServiceLcn(s_ptr));

    utf8 = ADB_GetEventName(e_ptr);
    pi.title = utf8tostringstrippingdvbcodes(utf8);
    STB_ReleaseUnicodeString(utf8);

    pi.start_time_utc_millis = STB_GCConvertToTimestamp(ADB_GetEventStartDateTime(e_ptr)) * (jlong)1000;
    pi.end_time_utc_millis = STB_GCConvertToTimestamp(ADB_GetEventEndDateTime(e_ptr)) * (jlong)1000;
    rv.push_back(pi);
}

static Vector<BroadcastProgramInfo> BroadcastDTVKit_GetProgramList(String8 s8id)
{
    Vector<BroadcastProgramInfo> piv;

    ALOGE("%s: Enter", __FUNCTION__);

    U16BIT lcn = strtoul(s8id.string(), 0, 0);
    void *s_ptr = ADB_FindServiceByLcn(ADB_SERVICE_LIST_TV | ADB_SERVICE_LIST_RADIO, lcn, TRUE);
    if (s_ptr) {
        void *now_event;
        void *next_event;
        ADB_GetNowNextEvents(s_ptr, &now_event, &next_event);
        pushprog(piv, now_event, s_ptr);
        pushprog(piv, next_event, s_ptr);
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return piv;
}

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

static bool
scannerUpdate()
{
    STB_OSMutexLock(pSelf->scanner.mutex);
    scannerUpdateUnderLock(false);
    STB_OSMutexUnlock(pSelf->scanner.mutex);
    return true;
}

static void
event_handler(U32BIT event, void *event_data, U32BIT data_size)
{
    if (pSelf) {
        //ALOGE("%s: ev %s(%d)/%d", __FUNCTION__, evcname(EVENT_CLASS(event), EVENT_TYPE(event)), EVENT_CLASS(event), EVENT_TYPE(event));
        if (event == EVENT_CODE(EV_CLASS_UI, EV_TYPE_UPDATE)) {
            scannerUpdate();
        }
        else if (event == APP_EVENT_SERVICE_EIT_NOW_UPDATE && !pSelf->scanner.active) {
            TunerHAL_onBroadcastEvent(1);
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
            U8BIT path = STB_DPGetLivePath();
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

int
Broadcast_Initialize(BroadcastDriver *pD)
{
    ALOGE("%s: Enter", __FUNCTION__);

    pSelf = new BroadcastDTVKit_Context;

    DTVKitPlatform_SetNVMBasePath("/data/data/com.broadcom.tvinput");
    APP_InitialiseDVB(event_handler);

    U16BIT services = ADB_GetNumServicesInList(ADB_SERVICE_LIST_ALL, true);
    if (services == 0) {
        ACFG_SetCountry(COUNTRY_CODE_UK); 
    }

    pD->GetChannelList = BroadcastDTVKit_GetChannelList;
    pD->GetProgramList = BroadcastDTVKit_GetProgramList;
    pD->GetScanInfo = BroadcastDTVKit_GetScanInfo;
    pD->Tune = BroadcastDTVKit_Tune;
    pD->Scan = BroadcastDTVKit_Scan;
    pD->Stop = BroadcastDTVKit_Stop;
    pD->Release = BroadcastDTVKit_Release;
    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

