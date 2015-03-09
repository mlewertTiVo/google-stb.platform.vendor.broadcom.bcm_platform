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

    if (property_get(PROPERTY_HDMI_ENABLE_CEC, value, DEFAULT_PROPERTY_HDMI_ENABLE_CEC) && (strcmp(value,"1")==0 || strcmp(value, "true")==0)) {
        enabled = true;
    }
#endif
    return enabled;
}

bool NexusIPCClient::setCecAutoWakeupEnabled(uint32_t cecId __unused, bool enabled)
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

    if (property_get(PROPERTY_HDMI_AUTO_WAKEUP_CEC, value, DEFAULT_PROPERTY_HDMI_AUTO_WAKEUP_CEC) && (strcmp(value,"1")==0 || strcmp(value, "true")==0)) {
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
    LOGE("%s: use derived class instead! returning NULL handle", __FUNCTION__);
    return NULL;
}

void NexusIPCClient::releaseVideoDecoderHandle(NEXUS_SimpleVideoDecoderHandle handle __unused)
{
    LOGE("%s: use derived class instead! no-op", __FUNCTION__);
}

NEXUS_SimpleAudioDecoderHandle NexusIPCClient::acquireAudioDecoderHandle()
{
    LOGE("%s: use derived class instead! returning NULL handle", __FUNCTION__);
    return NULL;
}

void NexusIPCClient::releaseAudioDecoderHandle(NEXUS_SimpleAudioDecoderHandle handle __unused)
{
    LOGE("%s: use derived class instead! no-op", __FUNCTION__);
}

NEXUS_SimpleAudioPlaybackHandle NexusIPCClient::acquireAudioPlaybackHandle()
{
    LOGE("%s: use derived class instead! returning NULL handle", __FUNCTION__);
    return NULL;
}

void NexusIPCClient::releaseAudioPlaybackHandle(NEXUS_SimpleAudioPlaybackHandle handle __unused)
{
    LOGE("%s: use derived class instead! no-op", __FUNCTION__);
}

NEXUS_SimpleEncoderHandle NexusIPCClient::acquireSimpleEncoderHandle()
{
    LOGE("%s: use derived class instead! returning NULL handle", __FUNCTION__);
    return NULL;
}

void NexusIPCClient::releaseSimpleEncoderHandle(NEXUS_SimpleEncoderHandle handle __unused)
{
    LOGE("%s: use derived class instead! no-op", __FUNCTION__);
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
