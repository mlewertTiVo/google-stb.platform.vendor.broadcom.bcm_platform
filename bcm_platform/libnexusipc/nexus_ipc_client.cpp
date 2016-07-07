/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
    bindNexusService();
}

NexusIPCClient::~NexusIPCClient()
{
    unbindNexusService();
}

void NexusIPCClient::bindNexusService()
{
    android::Mutex::Autolock autoLock(mLock);

    if (mBindRefCount == 0) {
        android::sp<android::IServiceManager> sm = android::defaultServiceManager();
        android::sp<android::IBinder> binder;
        do {
            binder = sm->getService(android::String16(NEXUS_INTERFACE_NAME));
            if (binder != 0) {
                break;
            }
            ALOGW("%s: client \"%s\" waiting for NexusService...", __PRETTY_FUNCTION__, getClientName());
            usleep(500000);
        } while(true);

        iNC = android::interface_cast<INexusClient>(binder);
    }
    mBindRefCount++;
}

void NexusIPCClient::unbindNexusService()
{
    android::Mutex::Autolock autoLock(mLock);

    if (mBindRefCount > 0) {
        mBindRefCount--;
    }
}

NEXUS_Error NexusIPCClient::clientJoin(const b_refsw_client_client_configuration *config __unused)
{
    NEXUS_Error rc = NEXUS_SUCCESS;

    android::Mutex::Autolock autoLock(mLock);

    if (mJoinRefCount == 0) {
        api_data cmd;

        BKNI_Memset(&cmd, 0, sizeof(cmd));
        cmd.api = api_clientJoin;
        BKNI_Snprintf(&cmd.param.clientJoin.in.clientName.string[0], CLIENT_MAX_NAME, "%s", getClientName());
        iNC->api_over_binder(&cmd);

        clientHandle = cmd.param.clientJoin.out.clientHandle;

        if (clientHandle != NULL) {
            NEXUS_ClientAuthenticationSettings *pAuthentication = &cmd.param.clientJoin.in.clientAuthenticationSettings;

            do {
                rc = NEXUS_ABSTRACTED_JOIN(pAuthentication);
                if(rc != NEXUS_SUCCESS)
                {
                    ALOGE("%s: NEXUS_Platform_AuthenticatedJoin failed for client \"%s\"!!!\n", __PRETTY_FUNCTION__, getClientName());
                    sleep(1);
                }
            } while (rc != NEXUS_SUCCESS);
        }
        else {
            ALOGE("%s: FATAL: could not obtain join client \"%s\" with server!!!", __PRETTY_FUNCTION__, getClientName());
            rc = NEXUS_NOT_INITIALIZED;
        }
    }

    if (rc == NEXUS_SUCCESS) {
        mJoinRefCount++;
    }
    return rc;
}

NEXUS_Error NexusIPCClient::clientUninit()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    android::Mutex::Autolock autoLock(mLock);

    if (mJoinRefCount > 0) {
        mJoinRefCount--;

        if (mJoinRefCount == 0) {
            api_data cmd;

            BKNI_Memset(&cmd, 0, sizeof(cmd));
            cmd.api = api_clientUninit;
            cmd.param.clientUninit.in.clientHandle = clientHandle;
            iNC->api_over_binder(&cmd);

            rc = cmd.param.clientUninit.out.status;
            if (rc == NEXUS_SUCCESS) {
                NEXUS_Platform_Uninit();
            }
        }
    } else {
        rc = NEXUS_NOT_INITIALIZED;
    }
    return rc;
}

/* Client side implementation of the APIs that are transferred to the server process over binder */
NexusClientContext * NexusIPCClient::createClientContext(const b_refsw_client_client_configuration *config)
{
    NEXUS_Error rc;
    NexusClientContext *client = NULL;
    api_data cmd;

    rc = clientJoin(config);

    if (rc == NEXUS_SUCCESS) {
        BKNI_Memset(&cmd, 0, sizeof(cmd));
        cmd.api = api_createClientContext;
        strncpy(cmd.param.createClientContext.in.clientName.string, getClientName(), CLIENT_MAX_NAME);
        cmd.param.createClientContext.in.clientPid = getClientPid();
        iNC->api_over_binder(&cmd);

        client = cmd.param.createClientContext.out.client;
    }

    if (client == NULL) {
        ALOGE("%s: Could not create client \"%s\" context!!!", __PRETTY_FUNCTION__, getClientName());
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

bool NexusIPCClient::isCecEnabled(uint32_t cecId __unused)
{
    bool enabled = false;
    char value[PROPERTY_VALUE_MAX];

    if (NEXUS_NUM_CEC > 0 && property_get(PROPERTY_HDMI_ENABLE_CEC, value, DEFAULT_PROPERTY_HDMI_ENABLE_CEC) && (strcmp(value,"1")==0 || strcmp(value, "true")==0)) {
        enabled = true;
    }
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

b_cecDeviceType NexusIPCClient::toCecDeviceType(char *string)
{
    b_cecDeviceType deviceType;
    int type = atoi(string);

    switch (type) {
        case -1:
            deviceType = eCecDeviceType_eInactive;
            break;
        case 0:
            deviceType = eCecDeviceType_eTv;
            break;
        case 1:
            deviceType = eCecDeviceType_eRecordingDevice;
            break;
        case 2:
            deviceType = eCecDeviceType_eReserved;
            break;
        case 3:
            deviceType = eCecDeviceType_eTuner;
            break;
        case 4:
            deviceType = eCecDeviceType_ePlaybackDevice;
            break;
        case 5:
            deviceType = eCecDeviceType_eAudioSystem;
            break;
        case 6:
            deviceType = eCecDeviceType_ePureCecSwitch;
            break;
        case 7:
            deviceType = eCecDeviceType_eVideoProcessor;
            break;
        default:
            deviceType = eCecDeviceType_eInvalid;
    }
    return deviceType;
}

/* Android-L sets up a property to define the device type and hence
   the logical address of the device.  */
b_cecDeviceType NexusIPCClient::getCecDeviceType(uint32_t cecId __unused)
{
    char value[PROPERTY_VALUE_MAX];
    b_cecDeviceType type = eCecDeviceType_eInvalid;

    if (property_get("ro.hdmi.device_type", value, NULL)) {
        type = NexusIPCClient::toCecDeviceType(value);
    }
    return type;
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

bool NexusIPCClient::sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage, uint8_t maxRetries)
{
    api_data cmd;
    BKNI_Memset(&cmd, 0, sizeof(cmd));
    cmd.api = api_sendCecMessage;
    cmd.param.sendCecMessage.in.cecId = cecId;
    cmd.param.sendCecMessage.in.srcAddr = srcAddr;
    cmd.param.sendCecMessage.in.destAddr = destAddr;
    cmd.param.sendCecMessage.in.length = length;
    memcpy(cmd.param.sendCecMessage.in.message, pMessage, MIN(length, NEXUS_CEC_MESSAGE_DATA_SIZE));
    cmd.param.sendCecMessage.in.maxRetries = maxRetries;
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
