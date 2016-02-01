#ifndef BROADCAST_H
#define BROADCAST_H

#include <jni.h>

#include <utils/Vector.h>
#include <utils/String8.h>

using namespace android;

class BroadcastChannelInfo {
public:
    enum Type {
         TYPE_OTHER, TYPE_DVB_T, TYPE_DVB_T2, TYPE_DVB_S,
         TYPE_DVB_S2, TYPE_DVB_C, TYPE_DVB_C2, TYPE_DVB_H,
         TYPE_DVB_SH, TYPE_ATSC_T, TYPE_ATSC_C,
         TYPE_ATSC_M_H, TYPE_ISDB_T, TYPE_ISDB_TB,
         TYPE_ISDB_S, TYPE_ISDB_C, TYPE_1SEG, TYPE_DTMB,
         TYPE_CMMB, TYPE_T_DMB, TYPE_S_DMB
    };
    String8 name;
    String8 number;
    String8 id;
    Type type;
    int onid;
    int tsid;
    int sid;
    String8 logoUrl;
};

class BroadcastProgramInfo {
public:
    String8 id;
    String8 channel_id;
    String8 title;
    String8 short_description;
    jlong start_time_utc_millis;
    jlong end_time_utc_millis;
};

class BroadcastProgramUpdateInfo: public BroadcastProgramInfo {
public:
    enum UpdateType {
        ClearAll,       /* */
        ClearChannel,   /* channel_id */
        Delete,         /* channel_id, id */
        Add,            /* all */
        Update,         /* all */
        Expire,         /* channel_id, id */
    };
    UpdateType type;
};

class BroadcastScanInfo {
public:
    jboolean busy;
    jboolean valid;
    jshort progress;
    jshort channels;
    jshort TVChannels;
    jshort dataChannels;
    jshort radioChannels;
    jshort signalStrengthPercent;
    jshort signalQualityPercent;

    BroadcastScanInfo():
        busy(false),
        valid(false),
        progress(0),
        channels(0),
        TVChannels(0),
        dataChannels(0),
        radioChannels(0),
        signalStrengthPercent(0),
        signalQualityPercent(0)
    {}
};

class BroadcastRect {
public:
    jshort x;
    jshort y;
    jshort w;
    jshort h;
};

class BroadcastTrackInfo {
public:
    jshort type;
    String8 id;
    String8 lang;
    jshort squarePixelWidth;
    jshort squarePixelHeight;
    jfloat frameRate;
    jshort channels;
    jint sampleRate; 
};

class BroadcastScanParams {
public:
    enum DeliverySystem {
        DeliverySystem_Undefined,
        DeliverySystem_Dvbt,
        DeliverySystem_Dvbs,
        DeliverySystem_Dvbc
    };
    enum ScanMode {
        ScanMode_Undefined,
        ScanMode_Blind,
        ScanMode_Single,
        ScanMode_Home
    };
    enum SatellitePolarity {
        SatellitePolarity_Undefined,
        SatellitePolarity_Horizontal,
        SatellitePolarity_Vertical
    };
    enum SatelliteMode {
        SatelliteMode_Undefined,
        SatelliteMode_Auto,
        SatelliteMode_SatQpskLdpc,
        SatelliteMode_Sat8pskLdpc,
        SatelliteMode_SatDvb
    };
    enum QamMode {
        QamMode_Undefined,
        QamMode_Auto,
        QamMode_Qam16,
        QamMode_Qam32,
        QamMode_Qam64,
        QamMode_Qam128,
        QamMode_Qam256
    };
    enum OfdmTransmissionMode {
        OfdmTransmissionMode_Undefined,
        OfdmTransmissionMode_Auto,
        OfdmTransmissionMode_Ofdm1k,
        OfdmTransmissionMode_Ofdm2k,
        OfdmTransmissionMode_Ofdm4k,
        OfdmTransmissionMode_Ofdm8k,
        OfdmTransmissionMode_Ofdm16k,
        OfdmTransmissionMode_Ofdm32k
    };
    enum OfdmMode {
        OfdmMode_Undefined,
        OfdmMode_Auto,
        OfdmMode_OfdmDvbt,
        OfdmMode_OfdmDvbt2
    };
    /* All */
    DeliverySystem deliverySystem;
    ScanMode scanMode;
    jint freqKHz;
    jboolean encrypted;
    /* DVB_S */
    SatellitePolarity satellitePolarity;
    jshort codeRateNumerator;
    jshort codeRateDenominator;
    SatelliteMode satelliteMode;
    /* DVB_C and DVB_S */
    jint symK;
    /* DVB_C */
    QamMode qamMode;
    /* DVB-T */
    jint bandwidthKHz;
    OfdmTransmissionMode ofdmTransmissionMode;
    OfdmMode ofdmMode;
    jshort plpId;
};

class BroadcastDriver {
public:
    int (*StartScan)(BroadcastScanParams *pParams);
    int (*StopScan)();
    int (*Tune)(String8);
    Vector<BroadcastChannelInfo> (*GetChannelList)();
    Vector<BroadcastProgramInfo> (*GetProgramList)(String8);
    Vector<BroadcastProgramUpdateInfo> (*GetProgramUpdateList)(jint limit);
    BroadcastScanInfo (*GetScanInfo)();
    jlong (*GetUtcTime)();
    int (*Stop)();
    int (*Release)();
    int (*SetGeometry)(BroadcastRect position, BroadcastRect clipRect, jshort gfxWidth, jshort gfxHeight, jshort zorder, jboolean visible);
    Vector<BroadcastTrackInfo> (*GetTrackInfoList)();
    int (*SelectTrack)(int, const String8 *);
    void (*SetCaptionEnabled)(bool enabled);
};

int Broadcast_Initialize(BroadcastDriver *pDriver);

#endif

