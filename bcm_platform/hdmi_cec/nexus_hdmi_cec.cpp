/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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
 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: nexus_hdmi_cec.cpp
 * 
 * Module Description:
 * This file contains functions to make Nexus IPC calls to the Nexus service
 * in order to setup HDMI CEC and retrieve HDMI CEC messages.
 * 
 *****************************************************************************/
#ifdef LOG_TAG
#undef LOG_TAG
#endif

// Verbose messages removed
//#define LOG_NDEBUG 0

#define LOG_TAG "nexus_hdmi_cec"

#include <errno.h>
#include <utils/Log.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utils/Errors.h>
#include <hardware/hdmi_cec.h>

#include "nexus_ipc_client_factory.h"
#include "nexus_hdmi_cec.h"

#define HDMI_CEC_TRACE_ENTER ALOGV("%s: Enter\n", __PRETTY_FUNCTION__)

using namespace android;

NexusHdmiCecDevice::HdmiCecMessageEventListener::HdmiCecMessageEventListener(NexusHdmiCecDevice *device) : mNexusHdmiCecDevice(device)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::HdmiCecMessageEventListener::~HdmiCecMessageEventListener()
{
    HDMI_CEC_TRACE_ENTER;
}

status_t NexusHdmiCecDevice::HdmiCecMessageEventListener::onHdmiCecMessageReceived(int32_t portId, android::INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    hdmi_event_t hdmiCecEvent;

    hdmiCecEvent.type = HDMI_EVENT_CEC_MESSAGE;
    hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
    hdmiCecEvent.cec.initiator = (cec_logical_address_t)message->initiator;
    hdmiCecEvent.cec.destination = (cec_logical_address_t)message->destination;
    hdmiCecEvent.cec.length = message->length;
    memcpy(hdmiCecEvent.cec.body, message->body, message->length);

    ALOGV("%s: portId=%d, initiator=%d, destination=%d, length=%d, opcode=0x%02x", __PRETTY_FUNCTION__, portId,
            hdmiCecEvent.cec.initiator, hdmiCecEvent.cec.destination,hdmiCecEvent.cec.length, hdmiCecEvent.cec.body[0]);

    if (mNexusHdmiCecDevice->mCallback != NULL && mNexusHdmiCecDevice->mCecEnable == true) {
        ALOGV("%s: About to call port %d CEC MESSAGE callback function %p...", __PRETTY_FUNCTION__, portId, mNexusHdmiCecDevice->mCallback);
        (*mNexusHdmiCecDevice->mCallback)(&hdmiCecEvent, mNexusHdmiCecDevice->mCallbackArg);
    }

    return NO_ERROR;
}

NexusHdmiCecDevice::HdmiHotplugEventListener::HdmiHotplugEventListener(NexusHdmiCecDevice *device) : mNexusHdmiCecDevice(device)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::HdmiHotplugEventListener::~HdmiHotplugEventListener()
{
    HDMI_CEC_TRACE_ENTER;
}

status_t NexusHdmiCecDevice::HdmiHotplugEventListener::onHdmiHotplugEventReceived(int32_t portId, bool connected)
{
    hdmi_event_t hdmiCecEvent;

    HDMI_CEC_TRACE_ENTER;

    hdmiCecEvent.type = HDMI_EVENT_HOT_PLUG;
    hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
    hdmiCecEvent.hotplug.port_id = portId;
    hdmiCecEvent.hotplug.connected = connected;

    if (mNexusHdmiCecDevice->mCallback != NULL) {
        ALOGV("%s: About to call port %d HDMI HOTPLUG callback function %p...", __PRETTY_FUNCTION__, portId, mNexusHdmiCecDevice->mCallback);
        (*mNexusHdmiCecDevice->mCallback)(&hdmiCecEvent, mNexusHdmiCecDevice->mCallbackArg);
    }
    return NO_ERROR;
}

NexusHdmiCecDevice::NexusHdmiCecDevice(uint32_t cecId) : mCecId(cecId), mCecLogicalAddr(0xFF), mCecEnable(true), mCecViewOnCmdPending(false),
                                                         pIpcClient(NULL), mCallback(NULL), mHdmiCecDevice(NULL),
                                                         mHdmiCecMessageEventListener(NULL), mHdmiHotplugEventListener(NULL)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::~NexusHdmiCecDevice()
{
    HDMI_CEC_TRACE_ENTER;
}

status_t NexusHdmiCecDevice::initialise()
{
    status_t ret = NO_INIT;
    b_cecStatus cecStatus;

    HDMI_CEC_TRACE_ENTER;

    pIpcClient = NexusIPCClientFactory::getClient("Android-HDMI-CEC");

    if (pIpcClient == NULL) {
        ALOGE("%s: cannot create NexusIPCClient!!!", __PRETTY_FUNCTION__);
    }
    else {
        mHdmiHotplugEventListener = new HdmiHotplugEventListener(this);

        if (mHdmiHotplugEventListener == NULL) {
            ALOGE("%s: cannot create HDMI hotplug event listener!!!", __PRETTY_FUNCTION__);
            delete pIpcClient;
            pIpcClient = NULL;
        }
        else {
            // Attempt to register the HDMI Hotplug Event Listener with NexusService
            ret = pIpcClient->setHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
            if (ret != NO_ERROR) {
                ALOGE("%s: could not register HDMI hotplug event listener (rc=%d)!!!", __PRETTY_FUNCTION__, ret);
                mHdmiHotplugEventListener = NULL;
                delete pIpcClient;
                pIpcClient = NULL;
            }
            else {
                if (pIpcClient->isCecEnabled(mCecId) == true) {
                    if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
                        ALOGE("%s: cannot get CEC status!!!", __PRETTY_FUNCTION__);
                        mHdmiHotplugEventListener = NULL;
                        delete pIpcClient;
                        pIpcClient = NULL;
                        ret = UNKNOWN_ERROR;
                    }
                    else if (cecStatus.ready) {
                        mHdmiCecMessageEventListener = new HdmiCecMessageEventListener(this);

                        if (mHdmiCecMessageEventListener == NULL) {
                            ALOGE("%s: cannot create HDMI CEC message event listener!!!", __PRETTY_FUNCTION__);
                            mHdmiHotplugEventListener = NULL;
                            delete pIpcClient;
                            pIpcClient = NULL;
                            ret = NO_INIT;
                        }
                        else {
                            // Attempt to register the HDMI CEC Message Event Listener with NexusService
                            ret = pIpcClient->setHdmiCecMessageEventListener(mCecId, mHdmiCecMessageEventListener);
                            if (ret != NO_ERROR) {
                                ALOGE("%s: could not register HDMI CEC message event listener (rc=%d)!!!", __PRETTY_FUNCTION__, ret);
                                mHdmiCecMessageEventListener = NULL;
                                mHdmiHotplugEventListener = NULL;
                                delete pIpcClient;
                                pIpcClient = NULL;
                            }
                        }
                    }
                    else {
                        ALOGE("%s: CEC%d not ready!!!", __PRETTY_FUNCTION__, mCecId);
                        mHdmiHotplugEventListener = NULL;
                        delete pIpcClient;
                        pIpcClient = NULL;
                        ret = UNKNOWN_ERROR;
                    }
                }
            }
        }
    }
    return ret;
}

status_t NexusHdmiCecDevice::uninitialise()
{
    status_t ret = NO_ERROR;

    if (pIpcClient != NULL) {
        if (mHdmiHotplugEventListener != NULL) {
            ret = pIpcClient->setHdmiHotplugEventListener(mCecId, NULL);
            mHdmiHotplugEventListener = NULL;
        }

        if (mHdmiCecMessageEventListener != NULL) {
            ret = pIpcClient->setHdmiCecMessageEventListener(mCecId, NULL);
            mHdmiCecMessageEventListener = NULL;
        }
        delete pIpcClient;
    }
    return ret;
}

status_t NexusHdmiCecDevice::setCecLogicalAddress(uint8_t addr)
{
    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    if (pIpcClient->setCecLogicalAddress(mCecId, addr) == false) {
        ALOGE("%s: cannot add CEC%d logical address 0x%02x!!!", __PRETTY_FUNCTION__, mCecId, addr);
        return UNKNOWN_ERROR;
    }

    mCecLogicalAddr = addr;

    // If the View On Cec command is pending and we have a valid logical address,
    // then we can issue the command to wakeup the connected TV...
    if (mCecLogicalAddr != 0xFF && mCecViewOnCmdPending) {
        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        }
        else if (pIpcClient->setCecPowerState(mCecId, ePowerState_S0) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, mCecId, ePowerState_S0);
        }
        else {
            mCecViewOnCmdPending = false;
        }
    }

    return NO_ERROR;
}

bool NexusHdmiCecDevice::getCecTransmitStandby()
{
    char value[PROPERTY_VALUE_MAX];
    bool tx=false;

    if (property_get(PROPERTY_HDMI_TX_STANDBY_CEC, value, NULL)) {
        tx = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return tx;
}

bool NexusHdmiCecDevice::getCecTransmitViewOn()
{
    char value[PROPERTY_VALUE_MAX];
    bool tx=false;

    if (property_get(PROPERTY_HDMI_TX_VIEW_ON_CEC, value, NULL)) {
        tx = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return tx;
}

void NexusHdmiCecDevice::setEnableState(bool enable)
{
    ALOGV("%s: enable=%d", __PRETTY_FUNCTION__, enable);

    mCecEnable = enable;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
    }
    else {
        pIpcClient->setCecEnabled(mCecId, enable);
    }
}

void NexusHdmiCecDevice::setAutoWakeupState(bool enable)
{
    ALOGV("%s: enable=%d", __PRETTY_FUNCTION__, enable);

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
    }
    else {
        pIpcClient->setCecAutoWakeupEnabled(mCecId, enable);
    }
}

void NexusHdmiCecDevice::setControlState(bool enable)
{
    ALOGV("%s: enable=%d", __PRETTY_FUNCTION__, enable);

    mCecEnable = enable;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
    }
    else {
        bool tx;
        b_powerState state;

        if (enable) {
            state = ePowerState_S0;
            tx = getCecTransmitViewOn();

            // If the logical address of the STB has not been setup, then we must delay
            // sending the View On Cec command...
            if (tx && mCecLogicalAddr == 0xFF) {
                mCecViewOnCmdPending = true;
                tx = false;
            }
        }
        else {
            mCecViewOnCmdPending = false;
            state = ePowerState_S3;
            tx = getCecTransmitStandby();
        }

        if (tx && pIpcClient->setCecPowerState(mCecId, state) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, mCecId, state);
        }
    }
}

bool NexusHdmiCecDevice::getConnectedState()
{
    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    b_hdmiOutputStatus hdmiOutputStatus;

    if (pIpcClient->getHdmiOutputStatus(mCecId, &hdmiOutputStatus) != true) {
        ALOGE("%s: cannot get HDMI%d output status!!!", __PRETTY_FUNCTION__, mCecId);
        return UNKNOWN_ERROR;
    }
    return hdmiOutputStatus.connected;
}

void NexusHdmiCecDevice::registerEventCallback(const struct hdmi_cec_device* dev, event_callback_t callback, void *arg)
{
    HDMI_CEC_TRACE_ENTER;

    mHdmiCecDevice = const_cast<struct hdmi_cec_device*>(dev);
    mCallback = callback;
    mCallbackArg = arg;
}

status_t NexusHdmiCecDevice::getCecPhysicalAddress(uint16_t* addr)
{
    HDMI_CEC_TRACE_ENTER;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    b_hdmiOutputStatus status;

    if (pIpcClient->getHdmiOutputStatus(mCecId, &status) != true) {
        ALOGE("%s: cannot get HDMI%d output status!!!", __PRETTY_FUNCTION__, mCecId);
        return UNKNOWN_ERROR;
    }

    *addr = (status.physicalAddress[0] * 256) + status.physicalAddress[1];
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::getCecVersion(int* version)
{
    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    b_cecStatus cecStatus;
    bool cecEnabled;

    cecEnabled = pIpcClient->isCecEnabled(mCecId) && (mCecEnable == true);
    if (!cecEnabled || pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
        ALOGE("%s: cannot get CEC status!!!", __PRETTY_FUNCTION__);
        return UNKNOWN_ERROR;
    }
    *version = cecStatus.cecVersion;
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::getCecVendorId(uint32_t* vendor_id)
{
    union {
        char id_array[4];
        uint32_t id_u32;
    } vendorId;

    vendorId.id_array[0] = 'B';
    vendorId.id_array[1] = 'R';
    vendorId.id_array[2] = 'C';
    vendorId.id_array[3] = 'M';

    *vendor_id = vendorId.id_u32;
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::sendCecMessage(const cec_message_t *message)
{
    HDMI_CEC_TRACE_ENTER;

    if (mCecEnable == true) {
        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
            return NO_INIT;
        }

        uint8_t srcAddr = (uint8_t)message->initiator;
        uint8_t destAddr = (uint8_t)message->destination;

        if (pIpcClient->sendCecMessage(mCecId, srcAddr, destAddr, message->length, const_cast<uint8_t *>(message->body)) != true) {
            ALOGE("%s: cannot send CEC message!!!", __PRETTY_FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::getCecPortInfo(struct hdmi_port_info* list[], int* total)
{
    bool cecEnabled;
    b_cecStatus cecStatus;

    HDMI_CEC_TRACE_ENTER;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    cecEnabled = pIpcClient->isCecEnabled(mCecId) && (mCecEnable == true);

    mPortInfo[0].type = HDMI_OUTPUT;
    mPortInfo[0].port_id = mCecId + 1;
    mPortInfo[0].cec_supported = cecEnabled;
    mPortInfo[0].arc_supported = false;
    mPortInfo[0].physical_address = 0;

    if (cecEnabled) {
        if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
            ALOGE("%s: cannot get CEC%d status!!!", __PRETTY_FUNCTION__, mCecId);
            return UNKNOWN_ERROR;
        }
        else {
            mPortInfo[0].physical_address = (cecStatus.physicalAddress[0] * 256) + cecStatus.physicalAddress[1];
        }
    }

    list[0] = mPortInfo;
    *total = 1; // TODO use NEXUS_NUM_CEC
    return NO_ERROR;
}

