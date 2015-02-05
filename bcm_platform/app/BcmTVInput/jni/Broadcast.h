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

class BroadcastDriver {
public:
    int (*StartBlindScan)();
    int (*StopScan)();
    int (*Tune)(String8);
    Vector<BroadcastChannelInfo> (*GetChannelList)();
    Vector<BroadcastProgramInfo> (*GetProgramList)(String8);
    BroadcastScanInfo (*GetScanInfo)();
    jlong (*GetUtcTime)();
    int (*Stop)();
    int (*Release)();
    int (*SetGeometry)(BroadcastRect position, BroadcastRect clipRect, jshort gfxWidth, jshort gfxHeight, jshort zorder, jboolean visible);
    Vector<BroadcastTrackInfo> (*GetTrackInfoList)();
    int (*SelectTrack)(int, String8);
    void (*SetCaptionEnabled)(bool enabled);
};

int Broadcast_Initialize(BroadcastDriver *pDriver);

#endif

