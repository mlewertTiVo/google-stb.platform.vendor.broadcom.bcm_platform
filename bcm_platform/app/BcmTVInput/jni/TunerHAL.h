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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace android;

typedef union _tuner_context
{
    NexusClientContext *nexus_client;
}Tuner_Context;

#include <utils/Vector.h>
#include <utils/String8.h>

class ChannelInfo {
public:
    String8 name;
    String8 number;
    String8 id;
    int onid;
    int tsid;
    int sid;
};

class Tuner_Data
{
public:
    NexusIPCClientBase *ipcclient;
    NexusClientContext *nexus_client;
    void *context;
    int (*Tune)(Tuner_Data *pTD, String8);
    Vector<ChannelInfo> (*GetChannelList)(Tuner_Data *pTD);
    int (*Stop)(Tuner_Data *pTD);
};

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
