#ifndef TUNER_HAL_H
#define TUNER_HAL_H

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_playback.h"
#include "nexus_base_mmap.h"
#include "nxclient.h"
#include "nexus_frontend.h"
#include "nexus_parser_band.h"
#include "nexus_simple_video_decoder.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include <utils/KeyedVector.h>
#include <utils/Vector.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/List.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Broadcast.h"

using namespace android;

class Tuner_Data
{
public:
    jobject tunerService;
    jclass broadcastEvent;
    jclass updateType;
    JavaVM *vm;
    NexusIPCClientBase *ipcclient;
    NexusClientContext *nexus_client;
    BroadcastDriver driver;
    int surfaceClientId;
    BKNI_MutexHandle mutex;
};

/* If you want to add a new event type:
 * 1. Add a new BroadcastEvent enum in TunerService.java
 * 2. Add an enum with an identical *name* below (the value doesn't matter)
 * 3. Add a matching case in the eventName() function in TunerHAL.cpp
 */
enum BroadcastEvent {
    CHANNEL_LIST_CHANGED,
    PROGRAM_UPDATE_LIST_CHANGED,
    PROGRAM_LIST_CHANGED,
    TRACK_LIST_CHANGED,
    TRACK_SELECTED,
    VIDEO_AVAILABLE,
    SCANNING_START,
    SCANNING_PROGRESS,
    SCANNING_COMPLETE,
};

void TunerHAL_onBroadcastEvent(BroadcastEvent e, jint param, const String8 *s);

NexusIPCClientBase *TunerHAL_getIPCClient();
NexusClientContext *TunerHAL_getClientContext();

class INexusTunerService: public IInterface 
{
public:
    android_DECLARE_META_INTERFACE(NexusTunerService);
};

class BnNexusTunerService : public BnInterface<INexusTunerService>
{
};

class NexusTunerService : public BnNexusTunerService
{
public:
    NexusTunerService();
    virtual ~NexusTunerService();
    static void instantiate();
    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags);
};

#endif
