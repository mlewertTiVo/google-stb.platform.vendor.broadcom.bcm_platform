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
  
#define LOG_TAG "NexusNxService"
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
#if NEXUS_NUM_HDMI_INPUTS
#include "nexus_hdmi_input.h"
#endif

#include "nexus_ipc_priv.h"
#include "nxclient.h"
#if ANDROID_SUPPORTS_EMBEDDED_NXSERVER
#include "nxserverlib.h"
#include "nxclient_local.h"
#endif

#define UINT32_C(x)  (x ## U)

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

typedef struct NexusServerContext {
    BLST_D_HEAD(b_refsw_client_list, NexusClientContext) clients;
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
    android::sp<NexusServerContext::StandbyMonitorThread> mStandbyMonitorThread;
} NexusServerContext;

NexusServerContext::StandbyMonitorThread::~StandbyMonitorThread()
{
    ALOGD("%s: called", __PRETTY_FUNCTION__); 

    if (this->name != NULL) {
        free(name);
        this->name = NULL;
    }
}

android::status_t NexusServerContext::StandbyMonitorThread::run(const char* name, int32_t priority, size_t stack)
{
    android::status_t status;
    
    this->name = strdup(name);

    status = Thread::run(name, priority, stack);
    if (status == android::OK) {
        state = StandbyMonitorThread::STATE_RUNNING;
    }
    return android::OK;
}

bool NexusServerContext::StandbyMonitorThread::threadLoop()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus, prevStatus;

    LOGD("%s: Entering for client \"%s\"", __FUNCTION__, getName());

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
    LOGD("%s: Exiting for client \"%s\"", __FUNCTION__, getName());
    return false;
}

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
    NxClient_JoinSettings joinSettings;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    strncpy(joinSettings.name, "config", NXCLIENT_MAX_NAME);
    do {
        rc = NxClient_Join(&joinSettings);

        if (rc != NEXUS_SUCCESS) {
            LOGW("NexusNxService::platformInit NxServer is not ready, waiting...");
            usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
        }
        else {
            LOGI("NexusNxService::platformInit NxClient_Join Succeeded!!! name = %s pid = %d", joinSettings.name, getpid());
        }
    } while (rc != NEXUS_SUCCESS);

    server->mStandbyMonitorThread = new NexusServerContext::StandbyMonitorThread();
    server->mStandbyMonitorThread->run(&joinSettings.name[0], ANDROID_PRIORITY_NORMAL);

#endif // ANDROID_SUPPORTS_EMBEDDED_NXSERVER

#if NEXUS_HAS_CEC
    unsigned i = NEXUS_NUM_CEC;
    while (i--) {
        if (isCecEnabled(i)) {
            ALOGV("%s: Instantiating CecServiceManager[%d]...", __PRETTY_FUNCTION__, i);
            mCecServiceManager[i] = CecServiceManager::instantiate(i);

            if (mCecServiceManager[i] != NULL) {
                if (mCecServiceManager[i]->platformInit() != OK) {
                    LOGE("%s: ERROR initialising CecServiceManager platform for CEC%d!", __FUNCTION__, i);
                    mCecServiceManager[i] = NULL;
                }
            }
            else {
                LOGE("%s: ERROR instantiating CecServiceManager for CEC%d!", __FUNCTION__, i);
            }
        }
    }
#endif
}

void NexusNxService::platformUninit()
{
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
    /* Cancel the standby monitor thread... */
    if (server->mStandbyMonitorThread != NULL && server->mStandbyMonitorThread->isRunning()) {
        server->mStandbyMonitorThread->stop();
        server->mStandbyMonitorThread->join();
        server->mStandbyMonitorThread = NULL;
    }
    NxClient_Uninit();
#endif // ANDROID_SUPPORTS_EMBEDDED_NXSERVER
}

void NexusNxService::instantiate()
{
    NexusNxService *nexusservice = new NexusNxService();

    nexusservice->platformInit();

    android::defaultServiceManager()->addService(
                INexusService::descriptor, nexusservice);
}

NexusNxService::NexusNxService()
{
    server = (NexusServerContext *)malloc(sizeof(NexusServerContext));

    if (server == NULL) {
        LOGE("%s: FATAL: Could not allocate memory for NexusServerContext!", __PRETTY_FUNCTION__);
        BDBG_ASSERT(server != NULL);
    }
    LOGI("NexusNxService Created");
    memset(server, 0, sizeof(NexusServerContext));
}

NexusNxService::~NexusNxService()
{
    LOGI("NexusNxService Destroyed");

    platformUninit();

    free(server);

    server = NULL;
}

NexusClientContext * NexusNxService::createClientContext(const b_refsw_client_client_configuration *config)
{
    NexusClientContext * client;
    NEXUS_ClientSettings clientSettings;
    NEXUS_Error rc;

    NEXUS_PlatformConfiguration platformConfig;

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
            LOGE("%s: Cannot allocate NxClient resources! (rc=%d)!", __FUNCTION__, rc);
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

    LOGI("%s: Exiting with client=%p", __FUNCTION__, (void *)client);
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

    LOGI("%s: client=\"%s\"", __FUNCTION__, client->createConfig.name.string);

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

void NexusNxService::getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
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

void NexusNxService::setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
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
        connectSettings.simpleEncoder[i].audio.cpuAccessible        = pConnectSettings->simpleEncoder[i].audio.cpuAccessible;
        connectSettings.simpleEncoder[i].video.cpuAccessible        = pConnectSettings->simpleEncoder[i].video.cpuAccessible;
    }

    rc = NxClient_Connect(&connectSettings, &client->ipc.connectId);
    if (rc)  {
        LOGE("%s: Could NOT connect resources (rc=%d)!", __FUNCTION__, rc);
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
        LOGE("%s: No resources to disconnect!", __FUNCTION__);
        ok = false;
    }
    return ok;
}
