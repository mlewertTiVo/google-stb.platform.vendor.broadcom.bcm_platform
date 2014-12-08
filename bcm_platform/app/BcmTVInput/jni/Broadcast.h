#ifndef BROADCAST_H
#define BROADCAST_H

#include <jni.h>

#include <utils/Vector.h>
#include <utils/String8.h>

using namespace android;

class BroadcastChannelInfo {
public:
    String8 name;
    String8 number;
    String8 id;
    int onid;
    int tsid;
    int sid;
};

class BroadcastProgramInfo {
public:
    String8 id;
    String8 channel_id;
    String8 title;
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

class BroadcastDriver {
public:
    int (*Scan)();
    int (*Tune)(String8);
    Vector<BroadcastChannelInfo> (*GetChannelList)();
    Vector<BroadcastProgramInfo> (*GetProgramList)(String8);
    BroadcastScanInfo (*GetScanInfo)();
    int (*Stop)();
    int (*Release)();
};

int Broadcast_Initialize(BroadcastDriver *pDriver);

#endif

