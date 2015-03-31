/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *****************************************************************************/

#define LOG_TAG "nxservice"
//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include <utils/Vector.h>
#include "cutils/properties.h"

#include "nexusnxservice.h"
#if NEXUS_HAS_CEC
#include "nexusnxcecservice.h"
#endif

#include "nexus_video_window.h"
#include "blst_list.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_playback.h"
#include "nexus_video_decoder_extra.h"
#include "nexus_base_mmap.h"
#if NEXUS_HAS_HDMI_INPUT
#include "nexus_hdmi_input.h"
#endif
#include "nexusirmap.h"

#include "nexus_ipc_priv.h"
#include "nxclient.h"

#ifdef UINT32_C
#undef UINT32_C
#define UINT32_C(x)  (x ## U)
#endif

typedef struct NexusClientContext {
    BDBG_OBJECT(NexusClientContext)
    BLST_D_ENTRY(NexusClientContext) link;
    struct {
        NEXUS_ClientHandle nexusClient;
        unsigned connectId;
    } ipc;
    b_refsw_client_client_name clientName;
    unsigned                   clientPid;
    b_refsw_client_client_info info;
} NexusClientContext;


BDBG_OBJECT_ID(NexusClientContext);

typedef struct NexusNxServerContext : public NexusServerContext {
    NexusNxServerContext()  { LOGV("%s: called", __PRETTY_FUNCTION__); }
    ~NexusNxServerContext() { LOGV("%s: called", __PRETTY_FUNCTION__); }

    struct StandbyMonitorThread : public android::Thread {

        enum ThreadState {
            STATE_UNKNOWN,
            STATE_STOPPED,
            STATE_RUNNING
        };

        StandbyMonitorThread() : state(STATE_STOPPED), name(NULL) {
            ALOGD("%s: called", __PRETTY_FUNCTION__);
        }

        virtual ~StandbyMonitorThread();

        virtual android::status_t run( const char* name = 0,
                                       int32_t priority = android::PRIORITY_DEFAULT,
                                       size_t stack = 0);

        virtual void stop() { state = STATE_STOPPED; }

        bool isRunning() { return (state == STATE_RUNNING); }

        const char *getName() { return name; }

    private:
        ThreadState state;
        char *name;
        bool threadLoop();

        /* Disallow copy constructor and copy operator... */
        StandbyMonitorThread(const StandbyMonitorThread &);
        StandbyMonitorThread &operator=(const StandbyMonitorThread &);
    };
    android::sp<NexusNxServerContext::StandbyMonitorThread> mStandbyMonitorThread;
} NexusNxServerContext;

NexusNxServerContext::StandbyMonitorThread::~StandbyMonitorThread()
{
    ALOGD("%s: called", __PRETTY_FUNCTION__);

    if (this->name != NULL) {
        free(name);
        this->name = NULL;
    }
}

android::status_t NexusNxServerContext::StandbyMonitorThread::run(const char* name, int32_t priority, size_t stack)
{
    android::status_t status;

    this->name = strdup(name);

    status = Thread::run(name, priority, stack);
    if (status == android::OK) {
        state = StandbyMonitorThread::STATE_RUNNING;
    }
    return android::OK;
}

bool NexusNxServerContext::StandbyMonitorThread::threadLoop()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus, prevStatus;

    LOGD("%s: Entering for client \"%s\"", __PRETTY_FUNCTION__, getName());

    NxClient_GetStandbyStatus(&standbyStatus);
    prevStatus = standbyStatus;

    while (isRunning()) {
        rc = NxClient_GetStandbyStatus(&standbyStatus);

        if(standbyStatus.settings.mode != prevStatus.settings.mode) {
            LOGD("%s: Acknowledge state %d\n", getName(), standbyStatus.settings.mode);
            NxClient_AcknowledgeStandby(true);
        }
        prevStatus = standbyStatus;
        BKNI_Sleep(NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS);
    }
    LOGD("%s: Exiting for client \"%s\"", __PRETTY_FUNCTION__, getName());
    return false;
}

void NexusNxService::hdmiOutputHotplugCallback(void *context __unused, int param __unused)
{
#if NEXUS_HAS_HDMI_OUTPUT
    int rc;
    NxClient_StandbyStatus standbyStatus;
    NexusNxService *pNexusNxService = reinterpret_cast<NexusNxService *>(context);

    rc = NxClient_GetStandbyStatus(&standbyStatus);
    if (rc != NEXUS_SUCCESS) {
        LOGE("%s: Could not get standby status!!!", __PRETTY_FUNCTION__);
        return;
    }

    if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
        LOGV("%s: Received HDMI%d hotplug event", __func__, param);

        NxClient_DisplayStatus status;
        rc = NxClient_GetDisplayStatus(&status);
        if (rc) {
            LOGE("%s: Could not get display status!!!", __PRETTY_FUNCTION__);
            return;
        }

        LOGD("%s: HDMI%d hotplug %s (receive device %s powered)",
             __func__, param,
             status.hdmi.status.connected ? "connected" : "disconnected", status.hdmi.status.rxPowered ? "is" : "isn't");

        Vector<sp<INexusHdmiHotplugEventListener> >::const_iterator it;

        Mutex::Autolock autoLock(pNexusNxService->server->mLock);

        for (it = pNexusNxService->server->mHdmiHotplugEventListenerList[param].begin(); it != pNexusNxService->server->mHdmiHotplugEventListenerList[param].end(); ++it) {
            LOGV("%s: Firing off HDMI%d hotplug %s event for listener %p...", __PRETTY_FUNCTION__, param,
                 (status.hdmi.status.connected && status.hdmi.status.rxPowered) ? "connected" : "disconnected", (*it).get());
            (*it)->onHdmiHotplugEventReceived(param, status.hdmi.status.connected && status.hdmi.status.rxPowered);
        }

#if ANDROID_ENABLE_HDMI_HDCP
        NxClient_DisplaySettings settings;
        NxClient_GetDisplaySettings(&settings);

        /* enable hdcp authentication if hdmi connected and powered */
        settings.hdmiPreferences.hdcp =
            (status.hdmi.status.connected && status.hdmi.status.rxPowered) ?
            NxClient_HdcpLevel_eMandatory :
            NxClient_HdcpLevel_eNone;

        rc = NxClient_SetDisplaySettings(&settings);
        if (rc) {
            LOGE("%s: Could not set display settings!!!", __PRETTY_FUNCTION__);
            return;
        }
#endif
    }
    else {
        LOGW("%s: Ignoring HDMI%d hotplug as we are in standby!", __PRETTY_FUNCTION__, param);
    }
#endif
}

void NexusNxService::hdmiOutputHdcpStateChangedCallback(void *context __unused, int param __unused)
{
#if ANDROID_ENABLE_HDMI_HDCP
    int rc;
    NxClient_StandbyStatus standbyStatus;
    NexusNxService *pNexusNxService = reinterpret_cast<NexusNxService *>(context);

    rc = NxClient_GetStandbyStatus(&standbyStatus);
    if (rc != NEXUS_SUCCESS) {
        LOGE("%s: Could not get standby status!!!", __PRETTY_FUNCTION__);
        return;
    }

    if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
        LOGV("%s: Received HDMI%d hdcp event", __func__, param);

        NxClient_DisplayStatus status;
        rc = NxClient_GetDisplayStatus(&status);
        if (rc) {
            LOGE("%s: Could not get display status!!!", __PRETTY_FUNCTION__);
            return;
        }

        LOGD("%s: HDMI%d HDCP state=%d error=%d repeater=%d hdcp1.1=%d hdcp2.2=%d\n",
             __func__, param,
             status.hdmi.hdcp.hdcpState,
             status.hdmi.hdcp.hdcpError,
             status.hdmi.hdcp.isHdcpRepeater,
             status.hdmi.hdcp.hdcp1_1Features,
             status.hdmi.hdcp.hdcp2_2Features);
    }
    else {
        LOGW("%s: Ignoring HDMI%d HDCP event as we are in standby!", __PRETTY_FUNCTION__, param);
    }
#endif
}

int NexusNxService::platformInitHdmiOutputs()
{
    int rc = 0;
#if NEXUS_HAS_HDMI_OUTPUT

    NxClient_CallbackThreadSettings settings;

    NxClient_GetDefaultCallbackThreadSettings(&settings);
    settings.hdmiOutputHotplug.callback = hdmiOutputHotplugCallback;
    settings.hdmiOutputHotplug.context = this;
    settings.hdmiOutputHotplug.param = 0;
#if ANDROID_ENABLE_HDMI_HDCP
    settings.hdmiOutputHdcpChanged.callback = hdmiOutputHdcpStateChangedCallback;
    settings.hdmiOutputHdcpChanged.context = this;
    settings.hdmiOutputHdcpChanged.param = 0;
#endif
    rc = NxClient_StartCallbackThread(&settings);
    if (rc) {
        LOGE("%s: Could not start HDMI callback thread!!!", __PRETTY_FUNCTION__);
        return rc;
    }

    /* Self-trigger the first callback */
    hdmiOutputHotplugCallback(this, 0);
#if ANDROID_ENABLE_HDMI_HDCP
    hdmiOutputHdcpStateChangedCallback(this, 0);
#endif
#endif
    return rc;
}

void NexusNxService::platformUninitHdmiOutputs()
{
#if NEXUS_HAS_HDMI_OUTPUT
    NxClient_StopCallbackThread();
#endif
}

static bool parseNexusIrMode(const char *name, NEXUS_IrInputMode *mode)
{
    struct irmode {
        const char * name;
        NEXUS_IrInputMode value;
    };

    #define DEFINE_IRINPUTMODE(mode) { #mode, NEXUS_IrInputMode_e##mode }
    static const struct irmode ir_modes[] = {
        DEFINE_IRINPUTMODE(TwirpKbd),            /* TWIRP */
        DEFINE_IRINPUTMODE(Sejin38KhzKbd),       /* Sejin IR keyboard (38.4KHz) */
        DEFINE_IRINPUTMODE(Sejin56KhzKbd),       /* Sejin IR keyboard (56.0KHz) */
        DEFINE_IRINPUTMODE(RemoteA),             /* remote A */
        DEFINE_IRINPUTMODE(RemoteB),             /* remote B */
        DEFINE_IRINPUTMODE(CirGI),               /* Consumer GI */
        DEFINE_IRINPUTMODE(CirSaE2050),          /* Consumer SA E2050 */
        DEFINE_IRINPUTMODE(CirTwirp),            /* Consumer Twirp */
        DEFINE_IRINPUTMODE(CirSony),             /* Consumer Sony */
        DEFINE_IRINPUTMODE(CirRecs80),           /* Consumer Rec580 */
        DEFINE_IRINPUTMODE(CirRc5),              /* Consumer Rc5 */
        DEFINE_IRINPUTMODE(CirUei),              /* Consumer UEI */
        DEFINE_IRINPUTMODE(CirRfUei),            /* Consumer RF UEI */
        DEFINE_IRINPUTMODE(CirEchoStar),         /* Consumer EchoRemote */
        DEFINE_IRINPUTMODE(SonySejin),           /* Sony Sejin keyboard using UART D */
        DEFINE_IRINPUTMODE(CirNec),              /* Consumer NEC */
        DEFINE_IRINPUTMODE(CirRC6),              /* Consumer RC6 */
        DEFINE_IRINPUTMODE(CirGISat),            /* Consumer GI Satellite */
        DEFINE_IRINPUTMODE(Custom),              /* Customer specific type. See NEXUS_IrInput_SetCustomSettings. */
        DEFINE_IRINPUTMODE(CirDirectvUhfr),      /* DIRECTV uhfr (In IR mode) */
        DEFINE_IRINPUTMODE(CirEchostarUhfr),     /* Echostar uhfr (In IR mode) */
        DEFINE_IRINPUTMODE(CirRcmmRcu),          /* RCMM Remote Control Unit */
        DEFINE_IRINPUTMODE(CirRstep),            /* R-step Remote Control Unit */
        DEFINE_IRINPUTMODE(CirXmp),              /* XMP-2 */
        DEFINE_IRINPUTMODE(CirXmp2Ack),          /* XMP-2 Ack/Nak */
        DEFINE_IRINPUTMODE(CirRC6Mode0),         /* Consumer RC6 Mode 0 */
        DEFINE_IRINPUTMODE(CirRca),              /* Consumer RCA */
        DEFINE_IRINPUTMODE(CirToshibaTC9012),    /* Consumer Toshiba */
        DEFINE_IRINPUTMODE(CirXip),              /* Consumer Tech4home XIP protocol */

        /* end of table marker */
        { NULL, NEXUS_IrInputMode_eMax }
    };
    #undef DEFINE_IRINPUTMODE

    const struct irmode *ptr = ir_modes;
    while(ptr->name) {
        if (strcmp(ptr->name, name) == 0) {
            *mode = ptr->value;
            return true;
        }
        ptr++;
    }
    return false; /* not found */
}

bool NexusNxService::platformInitIR()
{
    static const char * ir_map_path = "/system/usr/irkeymap";
    static const char * ir_map_ext = ".ikm";

    /* default values */
    static const NEXUS_IrInputMode ir_mode_default_enum = NEXUS_IrInputMode_eCirNec;
    static const char * ir_mode_default = "CirNec";
    static const char * ir_map_default = "broadcom_silver";
    static const char * ir_mask_default = "0";

    char ir_mode_property[PROPERTY_VALUE_MAX];
    char ir_map_property[PROPERTY_VALUE_MAX];
    char ir_mask_property[PROPERTY_VALUE_MAX];

    NEXUS_IrInputMode mode;
    android::sp<NexusIrMap> map;
    uint64_t mask;

    memset(ir_mode_property, 0, sizeof(ir_mode_property));
    property_get("ro.ir_remote.mode", ir_mode_property, ir_mode_default);
    if (parseNexusIrMode(ir_mode_property, &mode)) {
        LOGI("Nexus IR remote mode: %s", ir_mode_property);
    } else {
        LOGW("Unknown IR remote mode: '%s', falling back to default '%s'",
                ir_mode_property, ir_mode_default);
        mode = ir_mode_default_enum;
    }

    memset(ir_map_property, 0, sizeof(ir_map_property));
    property_get("ro.ir_remote.map", ir_map_property, ir_map_default);
    android::String8 map_path(ir_map_path);
    map_path += "/";
    map_path += ir_map_property;
    map_path += ir_map_ext;
    LOGI("Nexus IR remote map: %s (%s)", ir_map_property, map_path.string());
    status_t status = NexusIrMap::load(map_path, &map);
    if (status)
    {
        LOGE("Nexus IR map load failed: %s", map_path.string());
        return false;
    }

    memset(ir_mask_property, 0, sizeof(ir_mask_property));
    property_get("ro.ir_remote.mask", ir_mask_property, ir_mask_default);
    mask = strtoull(ir_mask_property, NULL, 0);
    LOGI("Nexus IR remote mask: %s (0x%llx)", ir_mask_property,
            (unsigned long long)mask);

    return irHandler.start(mode, map, mask);
}

void NexusNxService::platformUninitIR()
{
    irHandler.stop();
}

#define NEXUS_TRUSTED_DATA_PATH "/data/misc/nexus"
void NexusNxService::platformInit()
{
    NEXUS_Error rc;
    FILE *key = NULL;
    char value[PROPERTY_VALUE_MAX];
    NxClient_JoinSettings joinSettings;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    strncpy(joinSettings.name, "config", NXCLIENT_MAX_NAME);

    sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(value, "r");
    joinSettings.mode = NEXUS_ClientMode_eUntrusted;
    if (key == NULL) {
       ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, value, errno, strerror(errno));
    } else {
       memset(value, 0, sizeof(value));
       fread(value, PROPERTY_VALUE_MAX, 1, key);
       if (strstr(value, "trusted:") == value) {
          const char *password = &value[8];
          joinSettings.mode = NEXUS_ClientMode_eVerified;
          joinSettings.certificate.length = strlen(password);
          memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
       }
       fclose(key);
    }

    do {
        rc = NxClient_Join(&joinSettings);

        if (rc != NEXUS_SUCCESS) {
            LOGW("NexusNxService::platformInit NxServer is not ready, waiting...");
            usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
        }
    } while (rc != NEXUS_SUCCESS);

    LOGI("%s: \"%s\"; joins %s mode (%d)", __FUNCTION__, joinSettings.name,
         (joinSettings.mode == NEXUS_ClientMode_eVerified) ? "VERIFIED" : "UNTRUSTED",
         joinSettings.mode);

    NexusNxServerContext *nxServer = static_cast<NexusNxServerContext *>(server);
    nxServer->mStandbyMonitorThread = new NexusNxServerContext::StandbyMonitorThread();
    nxServer->mStandbyMonitorThread->run(&joinSettings.name[0], ANDROID_PRIORITY_NORMAL);

#if NEXUS_HAS_CEC
    unsigned i = NEXUS_NUM_CEC;
    while (i--) {
        if (isCecEnabled(i)) {
            ALOGV("%s: Instantiating CecServiceManager[%d]...", __PRETTY_FUNCTION__, i);
            mCecServiceManager[i] = CecServiceManager::instantiate(this, i);

            if (mCecServiceManager[i] != NULL) {
                if (mCecServiceManager[i]->platformInit() != OK) {
                    LOGE("%s: ERROR initialising CecServiceManager platform for CEC%d!", __PRETTY_FUNCTION__, i);
                    mCecServiceManager[i] = NULL;
                }
            }
            else {
                LOGE("%s: ERROR instantiating CecServiceManager for CEC%d!", __PRETTY_FUNCTION__, i);
            }
        }
    }
#endif
    platformInitHdmiOutputs();
    platformInitIR();
}

void NexusNxService::platformUninit()
{
    platformUninitIR();
    platformUninitHdmiOutputs();

#if NEXUS_HAS_CEC
    for (unsigned i = 0; i < NEXUS_NUM_CEC; i++) {
        if (mCecServiceManager[i] != NULL) {
            mCecServiceManager[i]->platformUninit();
            mCecServiceManager[i] = NULL;
        }
    }
#endif

    NexusNxServerContext *nxServer = static_cast<NexusNxServerContext *>(server);
    /* Cancel the standby monitor thread... */
    if (nxServer->mStandbyMonitorThread != NULL && nxServer->mStandbyMonitorThread->isRunning()) {
        nxServer->mStandbyMonitorThread->stop();
        nxServer->mStandbyMonitorThread->join();
        nxServer->mStandbyMonitorThread = NULL;
    }
    NxClient_Uninit();
}

void NexusNxService::instantiate()
{
    NexusNxServerContext *server = new NexusNxServerContext();

    if (server == NULL) {
        LOGE("%s: FATAL: Could not instantiate NexusNxServerContext!!!", __PRETTY_FUNCTION__);
        BDBG_ASSERT(server != NULL);
    }

    NexusNxService *nexusservice = new NexusNxService();
    if (nexusservice != NULL) {
        nexusservice->server = server;

        nexusservice->platformInit();

        android::defaultServiceManager()->addService(
                    INexusService::descriptor, nexusservice);
    }
    else {
        LOGE("%s: Could not instantiate NexusNxService!!!", __PRETTY_FUNCTION__);
    }
}

NexusNxService::NexusNxService()
{
    LOGI("NexusNxService Created");
}

NexusNxService::~NexusNxService()
{
    LOGI("NexusNxService Destroyed");

    platformUninit();

    delete server;
    server = NULL;
}

NexusClientContext * NexusNxService::createClientContext(const b_refsw_client_client_name *pClientName, unsigned clientPid)
{
    NexusClientContext * client;
    NEXUS_ClientSettings clientSettings;
    NEXUS_Error rc;

    Mutex::Autolock autoLock(server->mLock);

    client = (NexusClientContext *)BKNI_Malloc(sizeof(NexusClientContext));
    if (client==NULL) {
        (void)BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
        return NULL;
    }

    BKNI_Memset(client, 0, sizeof(*client));
    BDBG_OBJECT_SET(client, NexusClientContext);

    client->clientName = *pClientName;
    client->clientPid = clientPid;

    BLST_D_INSERT_HEAD(&server->clients, client, link);

    if (powerState != ePowerState_S0) {
        NxClient_StandbySettings standbySettings;

        LOGI("We need to set Nexus Power State S0 first...");
        NxClient_GetDefaultStandbySettings(&standbySettings);
        standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn;
        rc = NxClient_SetStandbySettings(&standbySettings);

        if (rc != NEXUS_SUCCESS) {
            LOGE("Oops we couldn't set Nexus Power State to S0!");
            goto err_client;
        }
        else {
            LOGI("Successfully set Nexus Power State S0");
        }
    }

    client->ipc.nexusClient = getNexusClient(client->clientPid, client->clientName.string);

    LOGI("%s: Exiting with client=%p", __PRETTY_FUNCTION__, (void *)client);
    return client;

err_client:
    /* todo fix cleanup */
    return NULL;
}

void NexusNxService::destroyClientContext(NexusClientContext * client)
{
    BDBG_OBJECT_ASSERT(client, NexusClientContext);
    void *res;

    Mutex::Autolock autoLock(server->mLock);

    LOGI("%s: client=\"%s\"", __PRETTY_FUNCTION__, client->clientName.string);

    BLST_D_REMOVE(&server->clients, client, link);
    BDBG_OBJECT_DESTROY(client, NexusClientContext);
    BKNI_Free(client);
}

static const NxClient_VideoWindowType videoWindowTypeConversion[] =
{
    NxClient_VideoWindowType_eMain, /* full screen capable */
    NxClient_VideoWindowType_ePip,  /* reduced size only. typically quarter screen. */
    NxClient_VideoWindowType_eNone,  /* app will do video as graphics */
    NxClient_VideoWindowType_Max
};

status_t NexusNxService::addHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener>& listener)
{
    status_t status = OK;

    if (listener == 0) {
        ALOGE("%s: HDMI%d hotplug event listener is NULL!!!", __PRETTY_FUNCTION__, portId);
        status = BAD_VALUE;
    }
    else {
        ALOGV("%s: HDMI%d hotplug event listener %p", __PRETTY_FUNCTION__, portId, listener.get());

#if NEXUS_HAS_HDMI_OUTPUT
        if (portId < NEXUS_NUM_HDMI_OUTPUTS) {
            Vector<sp<INexusHdmiHotplugEventListener> >::iterator it;
            sp<IBinder> binder = listener->asBinder();

            Mutex::Autolock autoLock(server->mLock);

            for (it = server->mHdmiHotplugEventListenerList[portId].begin(); it != server->mHdmiHotplugEventListenerList[portId].end(); ++it) {
                if ((*it)->asBinder() == binder) {
                    ALOGE("%s: Already added HDMI%d hotplug event listener %p!!!", __PRETTY_FUNCTION__, portId, listener.get());
                    status = ALREADY_EXISTS;
                    break;
                }
            }

            if (status == OK) {
                server->mHdmiHotplugEventListenerList[portId].push_back(listener);
                binder->linkToDeath(this);
            }
        }
        else
#endif
        {
            ALOGE("%s: No HDMI%d output on this device!!!", __PRETTY_FUNCTION__, portId);
            status = INVALID_OPERATION;
        }
    }
    return status;
}

status_t NexusNxService::removeHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener>& listener)
{
    status_t status = BAD_VALUE;

    if (listener == 0) {
        ALOGE("%s: HDMI%d hotplug event listener is NULL!!!", __PRETTY_FUNCTION__, portId);
    }
    else {
        ALOGV("%s: HDMI%d hotplug event listener %p", __PRETTY_FUNCTION__, portId, listener.get());

#if NEXUS_HAS_HDMI_OUTPUT
        if (portId < NEXUS_NUM_HDMI_OUTPUTS) {
            Vector<sp<INexusHdmiHotplugEventListener> >::iterator it;
            sp<IBinder> binder = listener->asBinder();

            Mutex::Autolock autoLock(server->mLock);

            for (it = server->mHdmiHotplugEventListenerList[portId].begin(); it != server->mHdmiHotplugEventListenerList[portId].end(); ++it) {
                if ((*it)->asBinder() == binder) {
                    binder->unlinkToDeath(this);
                    server->mHdmiHotplugEventListenerList[portId].erase(it);
                    status = OK;
                    break;
                }
            }

            if (status == BAD_VALUE) {
                ALOGW("%s: Could NOT find HDMI%d hotplug event listener %p!!!", __PRETTY_FUNCTION__, portId, listener.get());
            }
        }
        else
#endif
        {
            ALOGE("%s: No HDMI%d output on this device!!!", __PRETTY_FUNCTION__, portId);
            status = INVALID_OPERATION;
        }
    }
    return status;
}

void NexusNxService::binderDied(const wp<IBinder>& who)
{
#if NEXUS_HAS_HDMI_OUTPUT
    IBinder *binder = who.unsafe_get();
    if (binder != NULL) {
        Vector<sp<INexusHdmiHotplugEventListener> >::iterator it;

        Mutex::Autolock autoLock(server->mLock);

        for (unsigned portId = 0; portId < NEXUS_NUM_HDMI_OUTPUTS; portId++) {
            for (it = server->mHdmiHotplugEventListenerList[portId].begin(); it != server->mHdmiHotplugEventListenerList[portId].end(); ++it) {
                if ((*it)->asBinder() == binder) {
                    ALOGD("%s: Removing HDMI%d hotplug event listener...", __PRETTY_FUNCTION__, portId);
                    server->mHdmiHotplugEventListenerList[portId].erase(it);
                    return;
                }
            }
        }
    }
#endif
}

bool NexusNxService::getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)
{
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
#if NEXUS_HAS_HDMI_OUTPUT
    memset(pHdmiOutputStatus, 0, sizeof(*pHdmiOutputStatus));

    if (portId < NEXUS_NUM_HDMI_OUTPUTS) {
        unsigned loops;
        NxClient_DisplayStatus status;

        for (loops = 0; loops < 4; loops++) {
            NEXUS_Error rc2;
            NxClient_StandbyStatus standbyStatus;

            rc2 = NxClient_GetStandbyStatus(&standbyStatus);
            if (rc2 == NEXUS_SUCCESS && standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
                rc = NxClient_GetDisplayStatus(&status);
                if ((rc == NEXUS_SUCCESS) && status.hdmi.status.connected) {
                    break;
                }
            }
            LOGV("%s: Waiting for HDMI%d output to be connected...", __PRETTY_FUNCTION__, portId);
            usleep(250 * 1000);
        }

        if (rc == NEXUS_SUCCESS) {
            if (status.hdmi.status.connected) {
                pHdmiOutputStatus->connected            = status.hdmi.status.connected;
                pHdmiOutputStatus->rxPowered            = status.hdmi.status.rxPowered;
                pHdmiOutputStatus->hdmiDevice           = status.hdmi.status.hdmiDevice;
                pHdmiOutputStatus->videoFormat          = status.hdmi.status.videoFormat;
                pHdmiOutputStatus->preferredVideoFormat = status.hdmi.status.preferredVideoFormat;
                pHdmiOutputStatus->aspectRatio          = status.hdmi.status.aspectRatio;
                pHdmiOutputStatus->colorSpace           = status.hdmi.status.colorSpace;
                pHdmiOutputStatus->audioFormat          = status.hdmi.status.audioFormat;
                pHdmiOutputStatus->audioSamplingRate    = status.hdmi.status.audioSamplingRate;
                pHdmiOutputStatus->audioSamplingSize    = status.hdmi.status.audioSamplingSize;
                pHdmiOutputStatus->audioChannelCount    = status.hdmi.status.audioChannelCount;
                pHdmiOutputStatus->physicalAddress[0]   = status.hdmi.status.physicalAddressA << 4 | status.hdmi.status.physicalAddressB;
                pHdmiOutputStatus->physicalAddress[1]   = status.hdmi.status.physicalAddressC << 4 | status.hdmi.status.physicalAddressD;
            }
        }
        else {
            LOGE("%s: Could not get HDMI%d output status!!!", __PRETTY_FUNCTION__, portId);
        }
    }
    else
#endif
    {
        LOGE("%s: No HDMI%d output on this device!!!", __PRETTY_FUNCTION__, portId);
    }
    return (rc == NEXUS_SUCCESS);
}
