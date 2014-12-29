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
 *
 * $brcm_Workfile: nexusnxservice.cpp $
 * $brcm_Revision: $
 * $brcm_Date: $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusservice/nexusnxservice.cpp $
 * 
 *****************************************************************************/

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
#if ANDROID_SUPPORTS_EMBEDDED_NXSERVER
#include "nxserverlib.h"
#include "nxclient_local.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "NexusNxService"

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
    struct {
        NEXUS_SurfaceClientHandle graphicsSurface;
        NEXUS_SurfaceClientHandle videoSurface;
    } resources;
    b_refsw_client_client_info info;
    b_refsw_client_client_configuration createConfig;
} NexusClientContext;


BDBG_OBJECT_ID(NexusClientContext);

typedef struct NexusNxServerContext : public NexusServerContext {
    NexusNxServerContext()  { LOGV("%s: called", __PRETTY_FUNCTION__); }
    ~NexusNxServerContext() { LOGV("%s: called", __PRETTY_FUNCTION__); }

#if ANDROID_SUPPORTS_EMBEDDED_NXSERVER
    BKNI_MutexHandle lock;
    nxserver_t nxserver;
#endif
    struct StandbyMonitorThread : public android::Thread {

        enum ThreadState {
            STATE_UNKNOWN,
            STATE_STOPPED,
            STATE_RUNNING
        };

        StandbyMonitorThread(NexusIrHandler& irHandler) : mIrHandler(irHandler), state(STATE_STOPPED), name(NULL) {
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
        NexusIrHandler& mIrHandler;
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
            if (standbyStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn) {
                mIrHandler.enterStandby();
            }
            else {
                mIrHandler.exitStandby();
            }
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
#if ANDROID_SUPPORTS_EMBEDDED_NXSERVER
    struct nxserver_settings settings;

    nxserver_get_default_settings(&settings);

    server->nxserver = nxserverlib_init(&settings);
    if (!server->nxserver) {
        LOGE("%s: FATAL: Cannot initialise nxserver library!", __PRETTY_FUNCTION__);
        BDBG_ASSERT(server->nxserver);
    }

    BKNI_CreateMutex(&server->lock);

    rc = nxserver_ipc_init(server->nxserver, server->lock);
    if (rc != 0) {
        LOGE("%s: FATAL: Cannot initialise nxserver ipc (rc=%d)!", __PRETTY_FUNCTION__, rc);
        BDBG_ASSERT(rc == 0);
    }

    nxclient_local_init(server->nxserver, server->lock);
#else
    FILE *key = NULL;
    char value[PROPERTY_VALUE_MAX];
    NxClient_JoinSettings joinSettings;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    strncpy(joinSettings.name, "config", NXCLIENT_MAX_NAME);

    sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
    key = fopen(value, "r");
    if (key == NULL) {
       ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, value, errno, strerror(errno));
       joinSettings.mode = NEXUS_ClientMode_eUntrusted;
    } else {
       memset(value, 0, sizeof(value));
       fread(value, PROPERTY_VALUE_MAX, 1, key);
       joinSettings.mode = NEXUS_ClientMode_eProtected;
       joinSettings.certificate.length = strlen(value);
       memcpy(joinSettings.certificate.data, value, joinSettings.certificate.length);
       fclose(key);
    }

    LOGI("%s: \"%s\"; joins %s mode", __FUNCTION__, joinSettings.name,
         (joinSettings.mode == NEXUS_ClientMode_eProtected) ? "PROTECTED" : "UNTRUSTED");

    do {
        rc = NxClient_Join(&joinSettings);

        if (rc != NEXUS_SUCCESS) {
            LOGW("NexusNxService::platformInit NxServer is not ready, waiting...");
            usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
        }
    } while (rc != NEXUS_SUCCESS);

    NexusNxServerContext *nxServer = static_cast<NexusNxServerContext *>(server);
    nxServer->mStandbyMonitorThread = new NexusNxServerContext::StandbyMonitorThread(irHandler);
    nxServer->mStandbyMonitorThread->run(&joinSettings.name[0], ANDROID_PRIORITY_NORMAL);

#endif // ANDROID_SUPPORTS_EMBEDDED_NXSERVER

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

#if ANDROID_SUPPORTS_EMBEDDED_NXSERVER
    nxclient_local_uninit();
    nxserver_ipc_uninit();
    nxserverlib_uninit(server->nxserver);
    BKNI_DestroyMutex(server->lock);
#else
    NexusNxServerContext *nxServer = static_cast<NexusNxServerContext *>(server);
    /* Cancel the standby monitor thread... */
    if (nxServer->mStandbyMonitorThread != NULL && nxServer->mStandbyMonitorThread->isRunning()) {
        nxServer->mStandbyMonitorThread->stop();
        nxServer->mStandbyMonitorThread->join();
        nxServer->mStandbyMonitorThread = NULL;
    }
    NxClient_Uninit();
#endif // ANDROID_SUPPORTS_EMBEDDED_NXSERVER
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

NexusClientContext * NexusNxService::createClientContext(const b_refsw_client_client_configuration *config)
{
    NexusClientContext * client;
    NEXUS_ClientSettings clientSettings;
    NEXUS_Error rc;

    client = (NexusClientContext *)BKNI_Malloc(sizeof(NexusClientContext));
    if (client==NULL) {
        (void)BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
        return NULL;
    }

    BKNI_Memset(client, 0, sizeof(*client));
    BDBG_OBJECT_SET(client, NexusClientContext);

    client->createConfig = *config;

    BLST_D_INSERT_HEAD(&server->clients, client, link);

    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_SurfaceComposition surfSettings;

    NxClient_GetDefaultAllocSettings(&allocSettings);
    if (config->resources.screen.required > 0) {
        allocSettings.surfaceClient = 1;
    }
    if (config->resources.videoDecoder > 0) {
        allocSettings.simpleVideoDecoder = 1;
    }
    if (config->resources.audioDecoder > 0) {
        allocSettings.simpleAudioDecoder = 1;
    }
    if (config->resources.audioPlayback > 0) {
        allocSettings.simpleAudioPlayback = 1;
    }
    if (config->resources.encoder > 0) {
        allocSettings.simpleEncoder = 1;
    }

    bool needResources = config->resources.audioDecoder || config->resources.audioPlayback ||
                         config->resources.videoDecoder || config->resources.encoder       ||
                         config->resources.screen.required;

    if (needResources)
    {
        rc = NxClient_Alloc(&allocSettings, &allocResults);
        if (rc) {
            LOGE("%s: Cannot allocate NxClient resources! (rc=%d)!", __PRETTY_FUNCTION__, rc);
            goto err_client;
        }
        else {
            if (config->resources.screen.required > 0) {
                NEXUS_SurfaceClientHandle surfaceClient;
                NEXUS_SurfaceClientHandle videoSc;

                client->info.surfaceClientId = allocResults.surfaceClient[0].id;
                surfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
                if (surfaceClient) {
                    /* creating the video window is necessasy so that SurfaceCompositor can resize the video window */
                    videoSc = NEXUS_SurfaceClient_AcquireVideoWindow(surfaceClient, 0);
                    if (videoSc == NULL) {
                        LOGE("%s: Could NOT acquire video window!", __PRETTY_FUNCTION__);
                        NxClient_Free(&allocResults);
                        goto err_client;
                    }
                }
                else {
                    LOGE("%s: Could NOT acquire top-level surface client!", __PRETTY_FUNCTION__);
                    NxClient_Free(&allocResults);
                    goto err_client;
                }
                client->resources.graphicsSurface = surfaceClient;
                client->resources.videoSurface = videoSc;
            }
            if (config->resources.videoDecoder > 0) {
                client->info.videoDecoderId = allocResults.simpleVideoDecoder[0].id;
            }
            if (config->resources.audioDecoder > 0) {
                client->info.audioDecoderId = allocResults.simpleAudioDecoder.id;
            }
            if (config->resources.audioPlayback > 0) {
                client->info.audioPlaybackId = allocResults.simpleAudioPlayback[0].id;
            }
            if (config->resources.hdmiInput > 0) {
                client->info.hdmiInputId = 1;   // NxClient_Alloc does not actually handle hdmiInput, but we will retain consistent API behaviour
            }
            if (config->resources.encoder > 0) {
                client->info.encoderId = allocResults.simpleEncoder[0].id;
            }
        }
    }

    if (powerState != ePowerState_S0) {
        NxClient_StandbySettings standbySettings;

        LOGI("We need to set Nexus Power State S0 first...");
        NxClient_GetDefaultStandbySettings(&standbySettings);
        standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn;
        rc = NxClient_SetStandbySettings(&standbySettings);

        if (rc != NEXUS_SUCCESS) {
            LOGE("Oops we couldn't set Nexus Power State to S0!");
            NxClient_Free(&allocResults);
            goto err_client;
        }
        else {
            LOGI("Successfully set Nexus Power State S0");
        }
    }

    client->ipc.nexusClient = getNexusClient(client);

    LOGI("%s: Exiting with client=%p", __PRETTY_FUNCTION__, (void *)client);
    return client;

err_client:
    /* todo fix cleanup */
    return NULL;
}

void NexusNxService::destroyClientContext(NexusClientContext * client)
{
    BDBG_OBJECT_ASSERT(client, NexusClientContext);
    NxClient_AllocResults resources;
    void *res;

    LOGI("%s: client=\"%s\"", __PRETTY_FUNCTION__, client->createConfig.name.string);

    if (client->resources.videoSurface != NULL) {
        NEXUS_SurfaceClient_ReleaseVideoWindow(client->resources.videoSurface);
    }

    if (client->resources.graphicsSurface != NULL) {
        NEXUS_SurfaceClient_Release(client->resources.graphicsSurface);
    }
   
    /* Now free any resources acquired by the createClientContext call... */
    memset(&resources, 0, sizeof(resources));
    resources.surfaceClient[0].id       = client->info.surfaceClientId;
    resources.simpleVideoDecoder[0].id  = client->info.videoDecoderId;
    resources.simpleAudioDecoder.id     = client->info.audioDecoderId;
    resources.simpleAudioPlayback[0].id = client->info.audioPlaybackId;
    resources.simpleEncoder[0].id       = client->info.encoderId;

    NxClient_Free(&resources);

    BLST_D_REMOVE(&server->clients, client, link);
    BDBG_OBJECT_DESTROY(client, NexusClientContext);
    BKNI_Free(client);
}


bool NexusNxService::addGraphicsWindow(NexusClientContext * client)
{
    char value[PROPERTY_VALUE_MAX];
    bool enable_offset = false;
    int rc;
    int xoff=0, yoff=0;
    unsigned width=0, height=0;
    b_refsw_client_client_configuration *config = &client->createConfig;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_SurfaceComposition surfSettings;

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.surfaceClient = 1;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc)
    {
        LOGE("%s: Cannot allocate NxClient graphic window (rc=%d)!", __PRETTY_FUNCTION__, rc);
        goto err_screen;
    }

    graphicSurfaceClientId = client->info.surfaceClientId = allocResults.surfaceClient[0].id;
        
    /* See if we need to tweak the graphics to fit on the screen */
    if(property_get("ro.screenresize.x", value, NULL))
    {
        xoff = atoi(value);
        if(property_get("ro.screenresize.y", value, NULL))
        {
            yoff = atoi(value);
            if(property_get("ro.screenresize.w", value, NULL))
            {
                width = atoi(value);
                if(property_get("ro.screenresize.h", value, NULL))
                {
                    height = atoi(value);
                    enable_offset = true;
                }
            }
        }
    }

    /* If user has not set gfx window size, read them from HD output's properties */
    if((config->resources.screen.position.width == 0) && (config->resources.screen.position.height == 0))
    {
        NEXUS_VideoFormatInfo fmt_info; 
        NEXUS_VideoFormat hd_fmt, sd_fmt;
        
        get_initial_output_formats_from_property(&hd_fmt, NULL);
        NEXUS_VideoFormat_GetInfo(hd_fmt, &fmt_info);

        config->resources.screen.position.width = fmt_info.width;
        config->resources.screen.position.height = fmt_info.height;
    }

    /* We *MUST* acquire the handle in order for the resources to be internally tracked */
    client->resources.graphicsSurface = NEXUS_SurfaceClient_Acquire(graphicSurfaceClientId);
    NxClient_GetSurfaceClientComposition(graphicSurfaceClientId, &surfSettings);
    surfSettings.position = config->resources.screen.position;
    surfSettings.virtualDisplay.width = config->resources.screen.position.width;
    surfSettings.virtualDisplay.height = config->resources.screen.position.height;

    LOGD("######### Surface composition %d %d %d %d ###############\n",
            surfSettings.position.x, surfSettings.position.y, surfSettings.position.width, surfSettings.position.height);

    if(enable_offset)
    {
        LOGD("######### REPOSITIONING REQUIRED %d %d %d %d ###############\n",xoff,yoff,width,height);

        surfSettings.clipRect.width = surfSettings.virtualDisplay.width;
        surfSettings.clipRect.height = surfSettings.virtualDisplay.height;
        surfSettings.position.x = xoff;
        surfSettings.position.y = yoff;
        surfSettings.position.width = width;
        surfSettings.position.height = height;
    }

#if (ANDROID_USES_TRELLIS_WM==1)
    // Disable Android visibility by default in SBS mode
    surfSettings.visible = 0;
#endif

    NxClient_SetSurfaceClientComposition(graphicSurfaceClientId, &surfSettings);
    /* We *MUST* release the resource for the client of this function to subsequently acquire */
    NEXUS_SurfaceClient_Release(client->resources.graphicsSurface);
    return true;

err_screen:
    return false;
}

void NexusNxService::getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info)
{
    BDBG_OBJECT_ASSERT(client, NexusClientContext);
    *info = client->info;
}


void NexusNxService::getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *pComposition)
{
    NEXUS_SurfaceCompositorClientId surfaceClientId;

    if (pComposition == NULL) {
        LOGE("%s: FATAL: pComposition is NULL!!!", __PRETTY_FUNCTION__);
        return;
    }

    /* NULL client context means we want to get the default main graphics surface composition */
    if (client == NULL) {
        surfaceClientId = graphicSurfaceClientId;
    }
    else {
        surfaceClientId = client->info.surfaceClientId;
    }
    NxClient_GetSurfaceClientComposition(surfaceClientId, pComposition);
}

void NexusNxService::setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *pComposition)
{
    NEXUS_SurfaceCompositorClientId surfaceClientId;

    if (pComposition == NULL) {
        LOGE("%s: FATAL: pComposition is NULL!!!", __PRETTY_FUNCTION__);
        return;
    }

    /* NULL client context means we want to set the default main graphics surface composition */
    if (client == NULL) {
        surfaceClientId = graphicSurfaceClientId;
    }
    else {
        surfaceClientId = client->info.surfaceClientId;
    }
    LOGD("%s: position: %dx%d@%d,%d virtualDisplay: %dx%d", __func__,
            pComposition->position.width, pComposition->position.height,
            pComposition->position.x, pComposition->position.y,
            pComposition->virtualDisplay.width, pComposition->virtualDisplay.height);

    NxClient_SetSurfaceClientComposition(surfaceClientId, pComposition);
    return;
}

void NexusNxService::getVideoWindowSettings(NexusClientContext * client, uint32_t window_id __unused, b_video_window_settings *settings)
{
    NEXUS_SurfaceComposition surfaceComposition;

    getClientComposition(client, &surfaceComposition);
    settings->virtualDisplay = surfaceComposition.virtualDisplay;
    settings->position       = surfaceComposition.position;
    settings->clipRect       = surfaceComposition.clipRect;
    settings->visible        = surfaceComposition.visible;
    settings->contentMode    = surfaceComposition.contentMode;
    settings->autoMaster     = true;
    settings->zorder         = surfaceComposition.zorder;
    return;
}

void NexusNxService::setVideoWindowSettings(NexusClientContext * client, uint32_t window_id __unused, b_video_window_settings *settings)
{
    NEXUS_SurfaceComposition surfaceComposition;

    getClientComposition(client, &surfaceComposition);

    surfaceComposition.virtualDisplay = settings->virtualDisplay;
    surfaceComposition.position       = settings->position;
    surfaceComposition.clipRect       = settings->clipRect;
    surfaceComposition.visible        = settings->visible;
    surfaceComposition.contentMode    = settings->contentMode;
    surfaceComposition.zorder         = settings->zorder;

    LOGD ("%s: position:%dx%d@%d,%d virtual display=%dx%d", __func__,
            surfaceComposition.position.width, surfaceComposition.position.height,
            surfaceComposition.position.x, surfaceComposition.position.y, 
            surfaceComposition.virtualDisplay.width, surfaceComposition.virtualDisplay.height);

    if ((surfaceComposition.position.width < 2) || (surfaceComposition.position.height < 1))
    {
      LOGE ("window width %d x height %d is too small", surfaceComposition.position.width, surfaceComposition.position.height);
      // interlaced content min is 2x2
      surfaceComposition.position.width = 2;
      surfaceComposition.position.height = 2;
    }
    setClientComposition(client, &surfaceComposition);
    return;
}

#ifdef ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
static const NxClient_VideoWindowType videoWindowTypeConversion[] =
{
    NxClient_VideoWindowType_eMain, /* full screen capable */
    NxClient_VideoWindowType_ePip,  /* reduced size only. typically quarter screen. */
    NxClient_VideoWindowType_eNone,  /* app will do video as graphics */
    NxClient_VideoWindowType_Max
};
#endif

bool NexusNxService::connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings)
{
    NEXUS_Error rc;
    unsigned i;
    struct NxClient_ConnectSettings connectSettings;

    NxClient_GetDefaultConnectSettings(&connectSettings);

    /* Connect simple video decoder resources... */
    for (i = 0; i < NXCLIENT_MAX_IDS && i < CLIENT_MAX_IDS && pConnectSettings->simpleVideoDecoder[i].id != 0; i++) {
        connectSettings.simpleVideoDecoder[i].id                                = pConnectSettings->simpleVideoDecoder[i].id;
        connectSettings.simpleVideoDecoder[i].surfaceClientId                   = pConnectSettings->simpleVideoDecoder[i].surfaceClientId;
        connectSettings.simpleVideoDecoder[i].windowId                          = pConnectSettings->simpleVideoDecoder[i].windowId;
        connectSettings.simpleVideoDecoder[i].decoderCapabilities.fifoSize      = pConnectSettings->simpleVideoDecoder[i].decoderCaps.fifoSize;
        connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxWidth      = pConnectSettings->simpleVideoDecoder[i].decoderCaps.maxWidth;
        connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxHeight     = pConnectSettings->simpleVideoDecoder[i].decoderCaps.maxHeight;
        BKNI_Memcpy(&connectSettings.simpleVideoDecoder[i].decoderCapabilities.supportedCodecs[0],
                    &pConnectSettings->simpleVideoDecoder[i].decoderCaps.supportedCodecs[0],
                    sizeof(pConnectSettings->simpleVideoDecoder[i].decoderCaps.supportedCodecs));
        connectSettings.simpleVideoDecoder[i].decoderCapabilities.avc51Enabled  = pConnectSettings->simpleVideoDecoder[i].decoderCaps.avc51Enabled;
        connectSettings.simpleVideoDecoder[i].windowCapabilities.maxWidth       = pConnectSettings->simpleVideoDecoder[i].windowCaps.maxWidth;
        connectSettings.simpleVideoDecoder[i].windowCapabilities.maxHeight      = pConnectSettings->simpleVideoDecoder[i].windowCaps.maxHeight;
        connectSettings.simpleVideoDecoder[i].windowCapabilities.encoder        = pConnectSettings->simpleVideoDecoder[i].windowCaps.encoder;
        connectSettings.simpleVideoDecoder[i].windowCapabilities.deinterlaced   = pConnectSettings->simpleVideoDecoder[i].windowCaps.deinterlaced;
#ifdef ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
        connectSettings.simpleVideoDecoder[i].windowCapabilities.type           =
                    videoWindowTypeConversion[pConnectSettings->simpleVideoDecoder[i].windowCaps.type];
#endif
    }

    /* Connect simple audio decoder resource... */
    connectSettings.simpleAudioDecoder.id                                       = pConnectSettings->simpleAudioDecoder.id;
    connectSettings.simpleAudioDecoder.decoderCapabilities.encoder              = pConnectSettings->simpleAudioDecoder.decoderCaps.encoder;

    /* Connect simple audio playback resources... */
    for (i = 0; i < NXCLIENT_MAX_IDS && i < CLIENT_MAX_IDS && pConnectSettings->simpleAudioPlayback[i].id != 0; i++) {
        connectSettings.simpleAudioPlayback[i].id = pConnectSettings->simpleAudioPlayback[i].id;
#ifdef ANDROID_SUPPORTS_ANALOG_INPUT        
        connectSettings.simpleAudioPlayback[i].i2s.enabled = pConnectSettings->simpleAudioPlayback[i].i2s.enabled;
        connectSettings.simpleAudioPlayback[i].i2s.index = pConnectSettings->simpleAudioPlayback[i].i2s.index;        
#endif
    }

    /* Connect HDMI input resource if < URSR 14.2... */
#if ANDROID_SUPPORTS_HDMI_LEGACY
    if (pConnectSettings->hdmiInput.id != 0) {
        connectSettings.hdmiInput.id                                = pConnectSettings->hdmiInput.id;
        connectSettings.hdmiInput.surfaceClientId                   = pConnectSettings->hdmiInput.surfaceClientId;
        connectSettings.hdmiInput.windowId                          = pConnectSettings->hdmiInput.windowId;
        connectSettings.hdmiInput.windowCapabilities.maxWidth       = pConnectSettings->hdmiInput.windowCaps.maxWidth;
        connectSettings.hdmiInput.windowCapabilities.maxHeight      = pConnectSettings->hdmiInput.windowCaps.maxHeight;
        connectSettings.hdmiInput.windowCapabilities.encoder        = pConnectSettings->hdmiInput.windowCaps.encoder;
        connectSettings.hdmiInput.windowCapabilities.deinterlaced   = pConnectSettings->hdmiInput.windowCaps.deinterlaced;
#ifdef ANDROID_SUPPORTS_ANALOG_INPUT        
        connectSettings.hdmiInput.hdDvi                             = pConnectSettings->hdmiInput.hdDvi;
#endif
    }
#endif

    /* Connect Simple Encoder resources... */
    for (i = 0; i < NXCLIENT_MAX_IDS && i < CLIENT_MAX_IDS && pConnectSettings->simpleEncoder[i].id != 0; i++) {
        connectSettings.simpleEncoder[i].id                         = pConnectSettings->simpleEncoder[i].id;
        connectSettings.simpleEncoder[i].display                    = pConnectSettings->simpleEncoder[i].display;
        connectSettings.simpleEncoder[i].nonRealTime                = pConnectSettings->simpleEncoder[i].nonRealTime;
        connectSettings.simpleEncoder[i].audio.cpuAccessible        = pConnectSettings->simpleEncoder[i].audio.cpuAccessible;
        connectSettings.simpleEncoder[i].video.cpuAccessible        = pConnectSettings->simpleEncoder[i].video.cpuAccessible;
    }

    rc = NxClient_Connect(&connectSettings, &client->ipc.connectId);
    if (rc)  {
        LOGE("%s: Could NOT connect resources (rc=%d)!", __PRETTY_FUNCTION__, rc);
    }
    return (rc == 0);
}

bool NexusNxService::disconnectClientResources(NexusClientContext * client)
{
    bool ok = true;
    if (client->ipc.connectId != 0) {
        NxClient_Disconnect(client->ipc.connectId);
        client->ipc.connectId = 0;
    }
    else {
        LOGE("%s: No resources to disconnect!", __PRETTY_FUNCTION__);
        ok = false;
    }
    return ok;
}

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

            Mutex::Autolock autoLock(server->mLock);

            for (it = server->mHdmiHotplugEventListenerList[portId].begin(); it != server->mHdmiHotplugEventListenerList[portId].end(); ++it) {
                if ((*it)->asBinder() == listener->asBinder()) {
                    ALOGE("%s: Already added HDMI%d hotplug event listener %p!!!", __PRETTY_FUNCTION__, portId, listener.get());
                    status = ALREADY_EXISTS;
                    break;
                }
            }

            if (status == OK) {
                server->mHdmiHotplugEventListenerList[portId].push_back(listener);
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

            Mutex::Autolock autoLock(server->mLock);

            for (it = server->mHdmiHotplugEventListenerList[portId].begin(); it != server->mHdmiHotplugEventListenerList[portId].end(); ++it) {
                if ((*it)->asBinder() == listener->asBinder()) {
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

bool NexusNxService::getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)
{
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
#ifdef ANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS
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
#else
#warning Reference software does not support obtaining HDMI output status in NxClient mode
    rc = NEXUS_SUCCESS;
#endif
    return (rc == NEXUS_SUCCESS);

}
