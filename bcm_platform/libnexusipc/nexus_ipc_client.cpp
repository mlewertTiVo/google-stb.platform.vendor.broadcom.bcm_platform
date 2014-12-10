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
 * $brcm_Workfile: nexus_ipc_client.cpp $
 * 
 * Module Description:
 * This file contains the implementation of the class that uses binder IPC
 * communication with the Nexus Service.  A client process should NOT instantiate
 * an object of this type directly.  Instead, they should use the "getClient()"
 * method of the NexusIPCClientFactory abstract factory class.
 * On the client side, the definition of these API functions simply encapsulate
 * the API into a command + param format and sends the command over binder to
 * the server side for actual execution.
 *
 *****************************************************************************/
  
#define LOG_TAG "NexusIPCClient"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <dirent.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include "cutils/properties.h"

// required for binder api calls
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "nexus_interface.h"
#include "nexusservice.h"
#include "nexus_ipc_priv.h"
#include "nexus_ipc_client.h"

#include <cutils/sockets.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>

#ifdef SBS_USES_TRELLIS_INPUT_EVENTS
#include <linux/input.h>
#include <fcntl.h>
#include "trellis_keymap.h"

static int fd;
#endif

class BpNexusClient: public android::BpInterface<INexusClient>
{
public:
    BpNexusClient(const android::sp<android::IBinder>& impl)
        : android::BpInterface<INexusClient>(impl)
    {
    }

    void NexusHandles(NEXUS_TRANSACT_ID eTransactId, int32_t *pHandle)
    {
        android::Parcel data, reply;
        data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
        remote()->transact(eTransactId, data, &reply);
        *pHandle = reply.readInt32();
    }

    void api_over_binder(api_data *cmd)
    {
        android::Parcel data, reply;
        data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
        data.write(cmd, sizeof(api_data));
        remote()->transact(API_OVER_BINDER, data, &reply);
        reply.read(&cmd->param, sizeof(cmd->param));
    }

    IBinder* get_remote()
    {
        return remote();
    }
};
android_IMPLEMENT_META_INTERFACE(NexusClient, NEXUS_INTERFACE_NAME)

/* Initialise static data members... */
NEXUS_ClientHandle NexusIPCClient::clientHandle(NULL);
unsigned NexusIPCClient::mBindRefCount(0);
android::sp<INexusClient> NexusIPCClient::iNC;

NexusIPCClient::NexusIPCClient(const char *clientName) : NexusIPCClientBase(clientName)
{
    LOGV("++++++ %s: \"%s\" ++++++", __PRETTY_FUNCTION__, getClientName());

    bindNexusService();
}

NexusIPCClient::~NexusIPCClient()
{
    LOGV("~~~~~~ %s: \"%s\" ~~~~~~", __PRETTY_FUNCTION__, getClientName());

    unbindNexusService();
}

void NexusIPCClient::bindNexusService()
{
    android::Mutex::Autolock autoLock(mLock);

    if (mBindRefCount == 0) {
        LOGV("++++++ %s: \"%s\" ++++++", __PRETTY_FUNCTION__, getClientName());

        android::sp<android::IServiceManager> sm = android::defaultServiceManager();
        android::sp<android::IBinder> binder;
        do {
            binder = sm->getService(android::String16(NEXUS_INTERFACE_NAME));
            if (binder != 0) {
                break;
            }
            LOGW("%s: client \"%s\" waiting for NexusService...", __PRETTY_FUNCTION__, getClientName());
            usleep(500000);
        } while(true);

        LOGI("%s: client \"%s\" acquired NexusService...", __PRETTY_FUNCTION__, getClientName());
        iNC = android::interface_cast<INexusClient>(binder);
    }
    mBindRefCount++;
}

void NexusIPCClient::unbindNexusService()
{
    android::Mutex::Autolock autoLock(mLock);

    if (mBindRefCount > 0) {
        mBindRefCount--;

        if (mBindRefCount == 0) {
            LOGV("~~~~~~ %s: \"%s\" ~~~~~~", __PRETTY_FUNCTION__, getClientName());
        }
    }
    else {
        LOGE("%s: Already unbound from Nexus Service!!", __PRETTY_FUNCTION__);
    }
}

NEXUS_Error NexusIPCClient::clientJoin()
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    android::Mutex::Autolock autoLock(mLock);

    if (mJoinRefCount == 0) {
        api_data cmd;

        LOGV("++++ %s: \"%s\" ++++", __PRETTY_FUNCTION__, getClientName());

        BKNI_Memset(&cmd, 0, sizeof(cmd));
        cmd.api = api_clientJoin;
        BKNI_Snprintf(&cmd.param.clientJoin.in.clientName.string[0], CLIENT_MAX_NAME, "%s", getClientName());
        iNC->api_over_binder(&cmd);

        clientHandle = cmd.param.clientJoin.out.clientHandle;

        if (clientHandle != NULL) {
            NEXUS_ClientAuthenticationSettings *pAuthentication = &cmd.param.clientJoin.in.clientAuthenticationSettings;

            LOGI("%s: Authentication returned successfully for client \"%s\", certificate=%s", __PRETTY_FUNCTION__,
                    getClientName(), pAuthentication->certificate.data);

            do {
                rc = NEXUS_ABSTRACTED_JOIN(pAuthentication);
                if(rc != NEXUS_SUCCESS)
                {
                    LOGE("%s: NEXUS_Platform_AuthenticatedJoin failed for client \"%s\"!!!\n", __PRETTY_FUNCTION__, getClientName());
                    sleep(1);
                } else {
                    LOGI("%s: NEXUS_Platform_AuthenticatedJoin succeeded for client \"%s\".\n", __PRETTY_FUNCTION__, getClientName());
                }
            } while (rc != NEXUS_SUCCESS);
        }
        else {
            LOGE("%s: FATAL: could not obtain join client \"%s\" with server!!!", __PRETTY_FUNCTION__, getClientName());
            rc = NEXUS_NOT_INITIALIZED;
        }
    }

    if (rc == NEXUS_SUCCESS) {
        mJoinRefCount++;
        LOGV("*** %s: incrementing join count to %d for client \"%s\". ***", __PRETTY_FUNCTION__, mJoinRefCount, getClientName());
    }
    return rc;
}

NEXUS_Error NexusIPCClient::clientUninit()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    android::Mutex::Autolock autoLock(mLock);

    if (mJoinRefCount > 0) {
        mJoinRefCount--;
        LOGV("*** %s: decrementing join count to %d for client \"%s\". ***", __PRETTY_FUNCTION__, mJoinRefCount, getClientName());

        if (mJoinRefCount == 0) {
            api_data cmd;

            LOGV("---- %s: \"%s\" ----", __PRETTY_FUNCTION__, getClientName());

            BKNI_Memset(&cmd, 0, sizeof(cmd));
            cmd.api = api_clientUninit;
            cmd.param.clientUninit.in.clientHandle = clientHandle;
            iNC->api_over_binder(&cmd);

            rc = cmd.param.clientUninit.out.status;
            if (rc == NEXUS_SUCCESS) {
                LOGI("%s: NEXUS_Platform_Uninit called for client \"%s\".", __PRETTY_FUNCTION__, getClientName());
                NEXUS_Platform_Uninit();
            }
            else {
                LOGE("%s: Failed to uninitialise client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
            }
        }
    } else {
        LOGE("%s: NEXUS is already uninitialised!", __PRETTY_FUNCTION__);
        rc = NEXUS_NOT_INITIALIZED;
    }
    return rc;
}

bool NexusIPCClient::addGraphicsWindow(NexusClientContext * client)
{
    LOGE("%s: Enter...", __PRETTY_FUNCTION__);

#ifdef SBS_USES_TRELLIS_INPUT_EVENTS
    // Open event driver to write Trellis events
    fd = open("/dev/event_write", O_RDWR);

    if (fd < 0)
        LOGE("ERROR: Couldn't open SBS event write device. Input devices will not work.");
#endif

    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_addGraphicsWindow;
    cmd.param.addGraphicsWindow.in.client = client;
    iNC->api_over_binder(&cmd);
    return cmd.param.addGraphicsWindow.out.status;
}

void NexusIPCClient::getDefaultConnectClientSettings(b_refsw_client_connect_resource_settings *settings)
{
    BKNI_Memset(settings, 0, sizeof(*settings));
}

/* Client side implementation of the APIs that are transferred to the server process over binder */
NexusClientContext * NexusIPCClient::createClientContext(const b_refsw_client_client_configuration *config)
{
    NEXUS_Error rc;
    NexusClientContext *client = NULL;
    b_refsw_client_client_configuration inConfig;
    api_data cmd;

    LOGV("%s: Client \"%s\" Num VDec=%d,ADec=%d,APlay=%d,Enc=%d,Surf=%d...", __PRETTY_FUNCTION__, getClientName(),
            config->resources.videoDecoder,
            config->resources.audioDecoder,
            config->resources.audioPlayback,
            config->resources.encoder,
            config->resources.screen.required);

    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        BKNI_Memcpy(&inConfig, config, sizeof(*config));
        inConfig.pid = getpid();
        BKNI_Memset(&cmd, 0, sizeof(cmd));
        cmd.api = api_createClientContext;
        cmd.param.createClientContext.in.createClientConfig = inConfig;
        iNC->api_over_binder(&cmd);

        client = cmd.param.createClientContext.out.client;
    }

    if (client == NULL) {
        LOGE("%s: Could not create client \"%s\" context!!!", __PRETTY_FUNCTION__, getClientName());
    }
    else {
        LOGI("%s: \"%s\" client=%p", __PRETTY_FUNCTION__, getClientName(), client);
    }
    return client;
}

void NexusIPCClient::destroyClientContext(NexusClientContext * client)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_destroyClientContext;
    cmd.param.destroyClientContext.in.client = client;
    iNC->api_over_binder(&cmd);

    clientUninit();
    return;
}

void NexusIPCClient::getClientInfo(NexusClientContext * client, b_refsw_client_client_info *info)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getClientInfo;
    cmd.param.getClientInfo.in.client = client;    
    iNC->api_over_binder(&cmd);
    *info = cmd.param.getClientInfo.out.info;
    return;
}

void NexusIPCClient::getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getClientComposition;
    cmd.param.getClientComposition.in.client = client;    
    iNC->api_over_binder(&cmd);
    *composition = cmd.param.getClientComposition.out.composition;
    return;
}

void NexusIPCClient::setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setClientComposition;
    cmd.param.setClientComposition.in.client = client;    
    cmd.param.setClientComposition.in.composition = *composition;
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getVideoWindowSettings;
    cmd.param.getVideoWindowSettings.in.client = client;
    cmd.param.getVideoWindowSettings.in.window_id = window_id;
    iNC->api_over_binder(&cmd);
    *settings = cmd.param.getVideoWindowSettings.out.settings;

    /* Reverse the size correction done in setVideoWindowSettings() */
    NEXUS_DisplaySettings disp_settings;
    NEXUS_VideoFormatInfo current_fmt_info;
    NEXUS_VideoFormatInfo initial_fmt_info;
    NEXUS_SurfaceComposition composition;
    NEXUS_VideoFormat initial_hd_format;    
    int16_t x, y;
    uint16_t w, h;

    get_initial_output_formats_from_property(&initial_hd_format, NULL);    
    getDisplaySettings(HD_DISPLAY, &disp_settings);
    
    NEXUS_VideoFormat_GetInfo(disp_settings.format, &current_fmt_info);
    NEXUS_VideoFormat_GetInfo(initial_hd_format, &initial_fmt_info);
    getClientComposition(NULL, &composition);

    /* Note: below equations are reverse of that in setVideoWindowSettings() */
    // reverse the display resolution change adjustment
    x = (settings->position.x * initial_fmt_info.width) / current_fmt_info.width;
    y = (settings->position.y * initial_fmt_info.height) / current_fmt_info.height;
    w = (settings->position.width * initial_fmt_info.width) / current_fmt_info.width;    
    h = (settings->position.height * initial_fmt_info.height) / current_fmt_info.height;    

    // reverse the screen position change adjustment
    x = ((x - composition.position.x) * initial_fmt_info.width) / composition.position.width;
    y = ((y - composition.position.y) * initial_fmt_info.height) / composition.position.height;
    w = (w * initial_fmt_info.width) / composition.position.width;
    h = (h * initial_fmt_info.height) / composition.position.height;

    settings->position.x = x;
    settings->position.y = y;
    settings->position.width = w;
    settings->position.height = h;    
    
    return;
}

void NexusIPCClient::setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));

    /* Videowindow size corrections to accomodate following:
        a. display resolution change from that of the boot time
        b. screen size change that user might have made */   
        
    NEXUS_DisplaySettings disp_settings;
    NEXUS_VideoFormatInfo current_fmt_info;
    NEXUS_VideoFormatInfo initial_fmt_info;
    NEXUS_SurfaceComposition composition;
    NEXUS_VideoFormat initial_hd_format;
    int16_t x, y;
    uint16_t w, h, screen_width, screen_height;
        
    get_initial_output_formats_from_property(&initial_hd_format, NULL);    
    getDisplaySettings(HD_DISPLAY, &disp_settings);
    
    NEXUS_VideoFormat_GetInfo(disp_settings.format, &current_fmt_info);
    NEXUS_VideoFormat_GetInfo(initial_hd_format, &initial_fmt_info);
    getClientComposition(NULL, &composition);

    // screen position change adjustment
    x = composition.position.x + (settings->position.x * composition.position.width) / initial_fmt_info.width; 
    y = composition.position.y + (settings->position.y * composition.position.height) / initial_fmt_info.height;
    w = (settings->position.width * composition.position.width) / initial_fmt_info.width; 
    h = (settings->position.height * composition.position.height) / initial_fmt_info.height; 

    // display resolution change adjustment
    x = (x * current_fmt_info.width) / initial_fmt_info.width;
    y = (y * current_fmt_info.height) / initial_fmt_info.height;
    w = (w * current_fmt_info.width) / initial_fmt_info.width;
    h = (h * current_fmt_info.height) / initial_fmt_info.height;

    screen_width = current_fmt_info.width;
    screen_height = current_fmt_info.height;
    
    // adjustment to prevent out of dest border error from nexus video window
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(w < 2) w = 2;
    if(h < 2) h = 2;

    if((x+w) >= screen_width)
    {
        // if screen_width is enough, we adjust width. Otherwise, we adjust x
        if(screen_width >= 2)
            w = screen_width-x;
        else
            x = screen_width-w;
    }

    if((y+h) >= screen_height)
    {
        // if screen_height is enough, we adjust height. Otherwise, we adjust y
        if(screen_height-y >= 2)
            h = screen_height-y;
        else
            y = screen_height-h;
    }

    /* Ensure the virtual display is set to the screen canvas size. */
    settings->virtualDisplay.width = screen_width;
    settings->virtualDisplay.height = screen_height;

    // videowindow size has been corrected above.. send it over to the server now

    settings->position.x = x;
    settings->position.y = y;    
    settings->position.width = w;
    settings->position.height = h;

    cmd.api = api_setVideoWindowSettings;
    cmd.param.setVideoWindowSettings.in.client = client;    
    cmd.param.setVideoWindowSettings.in.window_id = window_id;
    cmd.param.setVideoWindowSettings.in.settings = *settings;
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getDisplaySettings;
    cmd.param.getDisplaySettings.in.display_id = display_id;    
    iNC->api_over_binder(&cmd);
    *settings = cmd.param.getDisplaySettings.out.settings;
    return;
}

void NexusIPCClient::setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setDisplaySettings;
    cmd.param.setDisplaySettings.in.display_id = display_id;    
    cmd.param.setDisplaySettings.in.settings = *settings;
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::setAudioVolume(float leftVol, float rightVol)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setAudioVolume;
    cmd.param.setAudioVolume.in.leftVolume = leftVol; 
    cmd.param.setAudioVolume.in.rightVolume = rightVol; 
    iNC->api_over_binder(&cmd);
    return;
}

bool NexusIPCClient::getFrame(NexusClientContext * client, const uint32_t width, const uint32_t height, 
    const uint32_t surfacePhyAddress, const NEXUS_PixelFormat surfaceFormat, const uint32_t decoderId)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getFrame;
    cmd.param.getFrame.in.client = client;    
    cmd.param.getFrame.in.width = width;
    cmd.param.getFrame.in.height = height;
    cmd.param.getFrame.in.surfacePhyAddress = surfacePhyAddress;
    cmd.param.getFrame.in.surfaceFormat = surfaceFormat;
    cmd.param.getFrame.in.decoderId = decoderId;
    iNC->api_over_binder(&cmd);
    return cmd.param.getFrame.out.status;
}

bool NexusIPCClient::setPowerState(b_powerState pmState)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setPowerState;
    cmd.param.setPowerState.in.pmState = pmState;
    iNC->api_over_binder(&cmd);
    return cmd.param.setPowerState.out.status;
}

b_powerState NexusIPCClient::getPowerState()
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getPowerState;
    iNC->api_over_binder(&cmd);
    return cmd.param.getPowerState.out.pmState;
}

bool NexusIPCClient::setCecEnabled(uint32_t cecId __unused, bool enabled)
{
    bool success = true;
    char value[PROPERTY_VALUE_MAX];

    snprintf(value, PROPERTY_VALUE_MAX, "%d", enabled);

    if (property_set(PROPERTY_HDMI_ENABLE_CEC, value) != 0) {
        success = false;
    }
    return success;
}

bool NexusIPCClient::isCecEnabled(uint32_t cecId __unused)
{
    bool enabled = false;
#if NEXUS_HAS_CEC
    char value[PROPERTY_VALUE_MAX];

    if (property_get(PROPERTY_HDMI_ENABLE_CEC, value, NULL) && (strcmp(value,"1")==0 || strcmp(value, "true")==0)) {
        enabled = true;
    }
#endif
    return enabled;
}

bool NexusIPCClient::setCecAutoWakeupEnabled(uint32_t cecId, bool enabled)
{
    bool success = true;
    char value[PROPERTY_VALUE_MAX];

    snprintf(value, PROPERTY_VALUE_MAX, "%d", enabled);

    if (property_set(PROPERTY_HDMI_AUTO_WAKEUP_CEC, value) != 0) {
        success = false;
    }
    return success;
}

bool NexusIPCClient::isCecAutoWakeupEnabled(uint32_t cecId __unused)
{
    bool enabled = false;
    char value[PROPERTY_VALUE_MAX];

    if (property_get(PROPERTY_HDMI_AUTO_WAKEUP_CEC, value, "1") && (strcmp(value,"1")==0 || strcmp(value, "true")==0)) {
        enabled = true;
    }
    return enabled;
}

bool NexusIPCClient::setCecPowerState(uint32_t cecId, b_powerState pmState)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setCecPowerState;
    cmd.param.setCecPowerState.in.cecId = cecId;
    cmd.param.setCecPowerState.in.pmState = pmState;
    iNC->api_over_binder(&cmd);
    return cmd.param.setCecPowerState.out.status;
}

bool NexusIPCClient::getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getCecPowerStatus;
    cmd.param.getCecPowerStatus.in.cecId = cecId;
    iNC->api_over_binder(&cmd);
    *pPowerStatus = cmd.param.getCecPowerStatus.out.powerStatus;
    return cmd.param.getCecPowerStatus.out.status;
}

bool NexusIPCClient::getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getCecStatus;
    cmd.param.getCecStatus.in.cecId = cecId;
    iNC->api_over_binder(&cmd);
    *pCecStatus = cmd.param.getCecStatus.out.cecStatus;
    return cmd.param.getCecStatus.out.status;
}

bool NexusIPCClient::sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_sendCecMessage;
    cmd.param.sendCecMessage.in.cecId = cecId;
    cmd.param.sendCecMessage.in.srcAddr = srcAddr;
    cmd.param.sendCecMessage.in.destAddr = destAddr;
    cmd.param.sendCecMessage.in.length = length;
    memcpy(cmd.param.sendCecMessage.in.message, pMessage, MIN(length, NEXUS_CEC_MESSAGE_DATA_SIZE));
    iNC->api_over_binder(&cmd);
    return cmd.param.sendCecMessage.out.status;
}

bool NexusIPCClient::setCecLogicalAddress(uint32_t cecId, uint8_t addr)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setCecLogicalAddress;
    cmd.param.setCecLogicalAddress.in.cecId = cecId;
    cmd.param.setCecLogicalAddress.in.addr = addr;
    iNC->api_over_binder(&cmd);
    return cmd.param.setCecLogicalAddress.out.status;
}

bool NexusIPCClient::getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getHdmiOutputStatus;
    cmd.param.getHdmiOutputStatus.in.portId = portId;
    iNC->api_over_binder(&cmd);
    *pHdmiOutputStatus = cmd.param.getHdmiOutputStatus.out.hdmiOutputStatus;
    return cmd.param.getHdmiOutputStatus.out.status;
}

bool NexusIPCClient::connectClientResources(NexusClientContext * client, b_refsw_client_connect_resource_settings *pConnectSettings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_connectClientResources;
    cmd.param.connectClientResources.in.client = client;    
    cmd.param.connectClientResources.in.connectSettings = *pConnectSettings;
    iNC->api_over_binder(&cmd);
    return cmd.param.connectClientResources.out.status;
}

bool NexusIPCClient::disconnectClientResources(NexusClientContext * client)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_disconnectClientResources;
    cmd.param.disconnectClientResources.in.client = client;    
    iNC->api_over_binder(&cmd);
    return cmd.param.disconnectClientResources.out.status;
}

/* -----------------------------------------------------------------------------
   Trellis BPM server expects clients to acquire SimpleVideoDecoder, SimpleAudioDecoder and 
   SimpleAudioPlayback through it. An attempt to directly acquire them may fail. 
   For SBS, these API's go via RPC to Trellis BPM and return a handle. 
   For standalone Android, these are simple wrapper functions. */
NEXUS_SimpleVideoDecoderHandle NexusIPCClient::acquireVideoDecoderHandle()
{
    return NEXUS_SimpleVideoDecoder_Acquire(NEXUS_ANY_ID);
}

void NexusIPCClient::releaseVideoDecoderHandle(NEXUS_SimpleVideoDecoderHandle handle)
{
    NEXUS_SimpleVideoDecoder_Release(handle);
}

NEXUS_SimpleAudioDecoderHandle NexusIPCClient::acquireAudioDecoderHandle()
{
    return NEXUS_SimpleAudioDecoder_Acquire(NEXUS_ANY_ID);
}

void NexusIPCClient::releaseAudioDecoderHandle(NEXUS_SimpleAudioDecoderHandle handle)
{
    NEXUS_SimpleAudioDecoder_Release(handle);
}

NEXUS_SimpleAudioPlaybackHandle NexusIPCClient::acquireAudioPlaybackHandle()
{
    return NEXUS_SimpleAudioPlayback_Acquire(NEXUS_ANY_ID);
}

void NexusIPCClient::releaseAudioPlaybackHandle(NEXUS_SimpleAudioPlaybackHandle handle)
{
    NEXUS_SimpleAudioPlayback_Release(handle);
}

NEXUS_SimpleEncoderHandle NexusIPCClient::acquireSimpleEncoderHandle()
{
    return NEXUS_SimpleEncoder_Acquire(NEXUS_ANY_ID);
}

void NexusIPCClient::releaseSimpleEncoderHandle(NEXUS_SimpleEncoderHandle handle)
{
    NEXUS_SimpleEncoder_Release(handle);
}

status_t NexusIPCClient::setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener)
{
    android::Parcel data, reply;
    data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
    data.writeInt32(cecId);
    data.writeStrongBinder(listener->asBinder());
    iNC->get_remote()->transact(SET_HDMI_CEC_MESSAGE_EVENT_LISTENER, data, &reply);
    return reply.readInt32();
}

status_t NexusIPCClient::addHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener)
{
    android::Parcel data, reply;
    data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
    data.writeInt32(portId);
    data.writeStrongBinder(listener->asBinder());
    iNC->get_remote()->transact(ADD_HDMI_HOTPLUG_EVENT_LISTENER, data, &reply);
    return reply.readInt32();
}

status_t NexusIPCClient::removeHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener)
{
    android::Parcel data, reply;
    data.writeInterfaceToken(android::String16(NEXUS_INTERFACE_NAME));
    data.writeInt32(portId);
    data.writeStrongBinder(listener->asBinder());
    iNC->get_remote()->transact(REMOVE_HDMI_HOTPLUG_EVENT_LISTENER, data, &reply);
    return reply.readInt32();
}

void NexusIPCClient::getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getPictureCtrlCommonSettings;
    cmd.param.getPictureCtrlCommonSettings.in.window_id = window_id;
    iNC->api_over_binder(&cmd);
    *settings = cmd.param.getPictureCtrlCommonSettings.out.settings;
    return;
}

void NexusIPCClient::setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setPictureCtrlCommonSettings;
    cmd.param.setPictureCtrlCommonSettings.in.window_id = window_id;
    cmd.param.setPictureCtrlCommonSettings.in.settings = *settings;
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_getGraphicsColorSettings;
    cmd.param.getGraphicsColorSettings.in.display_id = display_id;
    iNC->api_over_binder(&cmd);
    *settings = cmd.param.getGraphicsColorSettings.out.settings;
    return;
}

void NexusIPCClient::setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setGraphicsColorSettings;
    cmd.param.setGraphicsColorSettings.in.display_id = display_id;
    cmd.param.setGraphicsColorSettings.in.settings = *settings;
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::setDisplayOutputs(int display)
{
    api_data cmd;
    LOGI("NexusIPCClient::setDisplayOutputs display %d",display);
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setDisplayOutputs;
    cmd.param.setDisplayOutputs.in.display = display;    
    iNC->api_over_binder(&cmd);
    return;
}

void NexusIPCClient::setAudioMute(int mute)
{
    api_data cmd;
    LOGI("NexusIPCClient::setAudioMute mute %d",mute);
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_setAudioMute;
    cmd.param.setAudioMute.in.mute = mute;    
    iNC->api_over_binder(&cmd);
    return;
}

#if ANDROID_USES_TRELLIS_WM

static bool bSockConnected = false;
static bool bAngryBirdsActive = false;
int hSockHandle = 0;
timer_t timerid = 0;
size_t iPos;
char pkg_name[255];

#define SOCKET_NAME             "broadcom_socket_server"
#define CMD_START_ANGRYBIRDS    "START_AB"
#define CMD_STOP_ANGRYBIRDS     "STOP_AB"
#define CMD_DESKTOP_HIDE        "DESKTOP_HIDE"
#define CMD_DESKTOP_SHOW        "DESKTOP_SHOW"
#define CMD_DESKTOP_SHOW_NO_APP "DESKTOP_SHOW_NO_APP"

/* -----------------------------------------------------------------------------------------
 *            IAndroidBAMEventListener Interface Implementation
 * -----------------------------------------------------------------------------------------
 */

void NexusIPCClient::setVisibility(const std::string& uid, bool visibility)
{
    LOGD("%s: Set the Visibility To %d", __PRETTY_FUNCTION__, visibility);
    NEXUS_SurfaceComposition comp;

    /* A NULL nexus client context indicates the default master graphics surface */
    getClientComposition(NULL, &comp);
    comp.visible = visibility;
    setClientComposition(NULL, &comp);
}

void NexusIPCClient::handleFocus(const std::string& uid, bool obtained)
{
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    //
    // EventHub.cpp rejects/accepts input events based on a property "input.DisableAllInput" 
    // 
    if(obtained)
    {
        // if focus obtained=true, enable all input events
        property_set("input.DisableAllInput", "0");
    } else {
        // if focus obtained=false, disable all input events
        property_set("input.DisableAllInput", "1");
    }
}


/* -------------------------------------------------------------------------------
 * On receipt of terminate(), Android is supposed set its visibility=false via
 * BPM Client
 * -------------------------------------------------------------------------------
 */
void NexusIPCClient::terminate(const std::string& uid)
{
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    setVisibility(uid,false);

    if (bSockConnected)
    {
        if (bAngryBirdsActive)
        {
            LOGD("NexusIPCClient::terminate, killing AngryBirds...");
            StopAB();
            bAngryBirdsActive = false;
        }else{
            LOGE("NexusIPCClient::terminate, Muting desktop...");
            DesktopHide();
        }
    }
}

std::string NexusIPCClient::launch(const std::string& uid)
{
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    std::string szLaunchParms = uid.c_str();
    iPos = szLaunchParms.find("angrybirds");

    if (!bSockConnected)
    {
        LOGE("NexusIPCClient::launch, Will create a timer");

        pthread_attr_t attr;
        pthread_attr_init( &attr );

        struct sched_param parm;
        int iRet;
        parm.sched_priority = 255;
        pthread_attr_setschedparam(&attr, &parm);

        struct sigevent sig;
        sig.sigev_notify = SIGEV_THREAD;
        sig.sigev_notify_function = &NexusIPCClient::TimerHandler;
        sig.sigev_value.sival_int = 20;
        sig.sigev_value.sival_ptr = this;
        sig.sigev_notify_attributes = &attr;

        // create a new timer
        iRet = timer_create(CLOCK_REALTIME, &sig, &timerid);

        if (iRet == 0)
        {
            LOGE("NexusIPCClient::launch, timer_create succeeded");

            struct itimerspec in, out;
            in.it_value.tv_sec = 1;
            in.it_value.tv_nsec = 0;
            in.it_interval.tv_sec = 0;
            in.it_interval.tv_nsec = 100000000;

            //issue the periodic timer request here.
            iRet = timer_settime(timerid, 0, &in, &out);
            if (iRet == 0)
                LOGE("NexusIPCClient::launch, timer_settime succeeded");

            else
                LOGE("NexusIPCClient::launch, timer_settime failed");
        }
    }

    if (bSockConnected)
    {
        if (iPos != std::string::npos)
        {
            if (CheckAB() == 0)
            {
                LOGD("NexusIPCClient::launch, uid = %s (launching AngryBirds...)", szLaunchParms.c_str());
                StartAB();
                bAngryBirdsActive = true;
            }

            else
            {
                LOGD("NexusIPCClient::launch, uid = %s (Cannot launch AngryBirds (not installed, unmuting desktop)...)", szLaunchParms.c_str());
                DesktopShowNoApp();
            }
        }else{
            LOGE("NexusIPCClient::launch, uid = %s (Unmuting desktop...)", szLaunchParms.c_str());
            DesktopShow();
        }
    }

    return uid;
}

bool NexusIPCClient::handleEvent(const std::string& uid, Trellis::Application::IWindow::Event * event)
{
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
#ifdef SBS_USES_TRELLIS_INPUT_EVENTS
    struct input_event ie;
    int rel_x, rel_y, n, ascii_code, i;
    bool needShift = false;

    if (fd) 
    {
        if (event->getObjectType() == Trellis::TypeManager<Trellis::Application::IWindow::KeyEvent>::getInstance().getName())
        {
            const Trellis::Application::IWindow::KeyEvent& keyEvent = dynamic_cast<const Trellis::Application::IWindow::KeyEvent&>(*event);
            ie.type = EV_KEY;
            ie.code = trellisAndroidKeymap[keyEvent.getKeyCode()];
            ie.value = keyEvent.getPressed();

            // Remap the "Last" button as "Escape" (will
            // allow transitioning out of an active app)
            if (ie.code == KEY_LAST)
                ie.code = KEY_BACK;

            LOGD("%s: Detected KeyEvent: code = %d, value = %d", __PRETTY_FUNCTION__, ie.code, ie.value);
            n = write(fd, (char *) &ie, sizeof(ie));

            ie.type = EV_SYN;
            ie.code = 0;
            ie.value = 0;
            n = write(fd, (char *) &ie, sizeof(ie));
        }

        // Below code forwards the keyboard and mouse events from Trellis. Since Android handles
        // keyboard and mouse devices directly, we don't need this. Kept disabled for future reference
#if 0
        else if (event->getObjectType() == Trellis::TypeManager<Trellis::IWindow::AsciiEvent>::getInstance().getName())
        {
            LOGD("%s: Detected AsciiEvent", __PRETTY_FUNCTION__);

            const Trellis::IWindow::AsciiEvent& asciiEvent = dynamic_cast<const Trellis::IWindow::AsciiEvent&>(*event);

            ie.type = EV_MSC;
            ie.code = MSC_SCAN;
            ie.value = 458756;
            n = write(fd, (char *) &ie, sizeof(ie));

            /* Check if this character needs SHIFT key */
            ascii_code = asciiEvent.getAsciiCode();
            if (ascii_code >= 65 && ascii_code <=90) needShift = true;

            for (i = 0; i < sizeof(asciiWithShift)/sizeof(int); i++)
                if (asciiWithShift[i]==ascii_code) needShift = true;

            if (needShift) {
                ie.type = EV_KEY;
                ie.code = KEY_LEFTSHIFT;
                ie.value = asciiEvent.getPressed();
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            
            ie.type = EV_KEY;
            ie.code = asciiToKeycode[asciiEvent.getAsciiCode()];
            ie.value = asciiEvent.getPressed();
            n = write(fd, (char *) &ie, sizeof(ie));

            ie.type = EV_SYN;
            ie.code = 0;
            ie.value = 0;
            n = write(fd, (char *) &ie, sizeof(ie));
        }

        else if (event->getObjectType() == Trellis::TypeManager<Trellis::IWindow::MouseEvent>::getInstance().getName())
        {
            LOGD("%s: Detected MouseEvent", __PRETTY_FUNCTION__);

            const Trellis::IWindow::MouseEvent& mouseEvent = dynamic_cast<const Trellis::IWindow::MouseEvent&>(*event);
            rel_x = (int)(mouseEvent.getMovement().getDeltaX());
            rel_y = (int)(mouseEvent.getMovement().getDeltaY());
            if(rel_x)
            {
                ie.type = EV_REL;
                ie.code = REL_X;
                ie.value = rel_x;
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            if(rel_y)
            {
                ie.type = EV_REL;
                ie.code = REL_Y;
                ie.value = rel_y;
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            if (mouseEvent.getButtons().getHasLeftChanged()) {
                ie.type = EV_MSC;
                ie.code = MSC_SCAN;
                ie.value = 589825;
                n = write(fd, (char *) &ie, sizeof(ie));

                ie.type = EV_KEY;
                ie.code = BTN_LEFT;
                ie.value = mouseEvent.getButtons().getIsLeftPressed();
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            if (mouseEvent.getButtons().getHasMiddleChanged()) {
                ie.type = EV_MSC;
                ie.code = MSC_SCAN;
                ie.value = 589825;
                n = write(fd, (char *) &ie, sizeof(ie));

                ie.type = EV_KEY;
                ie.code = BTN_MIDDLE;
                ie.value = mouseEvent.getButtons().getIsMiddlePressed();
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            if (mouseEvent.getButtons().getHasRightChanged()) {
                ie.type = EV_MSC;
                ie.code = MSC_SCAN;
                ie.value = 589825;
                n = write(fd, (char *) &ie, sizeof(ie));

                ie.type = EV_KEY;
                ie.code = BTN_RIGHT;
                ie.value = mouseEvent.getButtons().getIsRightPressed();
                n = write(fd, (char *) &ie, sizeof(ie));
            }
            
            ie.type = EV_SYN;
            ie.code = 0;
            ie.value = 0;
            n = write(fd, (char *) &ie, sizeof(ie));
        }
#endif /* if 0 */        
    }
#endif

    return false;
}



/* ----------------------------------------------------------------------------------------
                            BCMAppManager Related Stuff
-------------------------------------------------------------------------------------------*/

void NexusIPCClient::TimerHandler(sigval_t s_val)
{
    NexusIPCClient *pIPC = (NexusIPCClient *)s_val.sival_ptr;
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);

    if (!bSockConnected)
    {
        int iReady = access("/tmp/boot_complete.cfg", F_OK);

        LOGE("NexusIPCClient::TimerHandler, iReady = %d", iReady);

        // Make sure BcmAppMgr is ready before we connect to the server socket
        if (iReady == 0)
        {
            LOGE("NexusIPCClient::TimerHandler, Calling ConnectToServerSocket");
            pIPC->ConnectToServerSocket();

            if (bSockConnected)
            {
                LOGE("NexusIPCClient::TimerHandler, Deleting the timer");
                timer_delete(timerid);
             }
        }
    }

    if (bSockConnected)
    {
        if (iPos != std::string::npos)
        {
            if (pIPC->CheckAB() == 0)
            {
                LOGD("NexusIPCClient::TimerHandler (launching AngryBirds...)");
                pIPC->StartAB();
                bAngryBirdsActive = true;
            }

            else
            {
                LOGD("NexusIPCClient::TimerHandler (Cannot launch AngryBirds (not installed)...)");
                pIPC->DesktopShowNoApp();
            }
        }else{
            LOGE("NexusIPCClient::TimerHandler (Unmuting desktop...)");
            pIPC->DesktopShow();
        }
    }
}


void NexusIPCClient::ConnectToServerSocket()
{
    LOGD("%s : %d Enter", __PRETTY_FUNCTION__, __LINE__);

    int err, i;
    struct sockaddr_un addr;
    socklen_t iLen;
    char szSockBuffer[255];

    addr.sun_family = AF_LOCAL;
    addr.sun_path[0] = '\0';
 
    strcpy(&addr.sun_path[1], SOCKET_NAME);
    iLen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(&addr.sun_path[1]);

    LOGE("ConnectToServerSocket: Before creating socket");
    hSockHandle = socket(PF_LOCAL, SOCK_STREAM, 0);

    if (hSockHandle < 0) 
    {
        err = errno;
        LOGE("%s: Cannot open socket: %s (%d)\n", __PRETTY_FUNCTION__, strerror(err), err);
        errno = err;
        return;
    }

    LOGE("ConnectToServerSocket: Before connecting to Java LocalSocketServer");
    if (connect(hSockHandle, (struct sockaddr *) &addr, iLen) < 0)
    {
        err = errno;
        LOGE("%s: connect() failed: %s (%d)\n", __PRETTY_FUNCTION__, strerror(err), err);
        close(hSockHandle);
        errno = err;
        return;
    }
    LOGE("ConnectToServerSocket: Connecting to Java LocalSocketServer succeeded");

    bSockConnected = true;
    LOGE("ConnectToServerSocket: Exit");
}

int NexusIPCClient::CheckAB()
{
    DIR           *d;
    struct dirent *dir;
    char lib_path[255], *pAB, *pPkg;
    bool bDirFound = false;
    int iRet = 1;

    // First up, try looking for the AngryBirds directory
    d = opendir("/data/app-lib/");

    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            LOGE("CheckAB: dir = %s\n", dir->d_name);

            pAB = strstr(dir->d_name, "com.rovio.angrybirds");
            if (pAB != NULL)
            {
                LOGE("CheckAB: AB tree detected, will look for the library...");

                strcpy(lib_path, "/data/app-lib/");
                strcat(lib_path, dir->d_name);

                // Save the package name (discard everything after '-')
                strcpy(pkg_name, dir->d_name);
                pPkg = strchr(pkg_name, '-');

                if (pPkg)
                {
                    pkg_name[pPkg - pkg_name] = '\0';
                    LOGE("CheckAB: pkg_name = %s", pkg_name);
                }

                bDirFound = true;
                break;
            }
        }

        closedir(d);

        if (bDirFound)
        {
            // Now, look for the library name
            d = opendir(lib_path);

            while ((dir = readdir(d)) != NULL)
            {
                pAB = strstr(dir->d_name, "lib");

                if (pAB != NULL)
                {
                    strcat(lib_path, "/");
                    strcat(lib_path, dir->d_name);
                    iRet = 0;

                    LOGE("CheckAB: Found the library %s", lib_path);
                    break;
                }
            }

            closedir(d);
        }
    }

    LOGE("CheckAB: iRet = %d", iRet);
    return iRet;
}

void NexusIPCClient::StartAB()
{
    int result;
    socklen_t iLen;
    char szSockBuffer[255];
    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);

    strcpy(szSockBuffer, CMD_START_ANGRYBIRDS);

    // Add the package name as well
    strcat(szSockBuffer, pkg_name);

    iLen = strlen(szSockBuffer);
    szSockBuffer[iLen] = '\0';
    LOGE("StartAB: szSockBuffer = %s, iLen = %d", szSockBuffer, iLen);
     
    result = write(hSockHandle, szSockBuffer, iLen);
    LOGE("StartAB: result = %d", result);
}

void NexusIPCClient::StopAB()
{
    int result;
    socklen_t iLen;
    char szSockBuffer[255];

    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    szSockBuffer[0] = '\0';
    strcpy(szSockBuffer, CMD_STOP_ANGRYBIRDS);
    iLen = strlen(szSockBuffer);
    szSockBuffer[iLen] = '\0';
    LOGE("StopAB: szSockBuffer = %s, iLen = %d", szSockBuffer, iLen);

    result = write(hSockHandle, szSockBuffer, iLen);
    LOGE("StopAB: result = %d", result);
}

void NexusIPCClient::DesktopShow()
{
    int result;
    socklen_t iLen;
    char szSockBuffer[255];

    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    szSockBuffer[0] = '\0';
    strcpy(szSockBuffer, CMD_DESKTOP_SHOW);
    iLen = strlen(szSockBuffer);
    szSockBuffer[iLen] = '\0';
    LOGE("DesktopShow: szSockBuffer = %s, iLen = %d", szSockBuffer, iLen);

    result = write(hSockHandle, szSockBuffer, iLen);
    LOGE("DesktopShow: result = %d", result);
}

void NexusIPCClient::DesktopShowNoApp()
{
    int result;
    socklen_t iLen;
    char szSockBuffer[255];

    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    szSockBuffer[0] = '\0';
    strcpy(szSockBuffer, CMD_DESKTOP_SHOW_NO_APP);
    iLen = strlen(szSockBuffer);
    szSockBuffer[iLen] = '\0';
    LOGE("DesktopShowNoApp: szSockBuffer = %s, iLen = %d", szSockBuffer, iLen);

    result = write(hSockHandle, szSockBuffer, iLen);
    LOGE("DesktopShowNoApp: result = %d", result);
}

void NexusIPCClient::DesktopHide()
{
    int result;
    socklen_t iLen;
    char szSockBuffer[255];

    LOGD("%s : %d ", __PRETTY_FUNCTION__, __LINE__);
    szSockBuffer[0] = '\0';
    strcpy(szSockBuffer, CMD_DESKTOP_HIDE);
    iLen = strlen(szSockBuffer);
    szSockBuffer[iLen] = '\0';
    LOGE("DesktopHide: szSockBuffer = %s, iLen = %d", szSockBuffer, iLen);

    result = write(hSockHandle, szSockBuffer, iLen);
    LOGE("DesktopHide: result = %d", result);
}

#endif
