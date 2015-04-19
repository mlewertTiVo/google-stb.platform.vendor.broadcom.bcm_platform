/******************************************************************************
 *    (c)2011-2015 Broadcom Corporation
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

#define LOG_TAG "Brcmstb-HDMI-CEC"

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

bool NexusHdmiCecDevice::HdmiCecMessageEventListener::isValidWakeupCecMessage(android::INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    // Check for valid wake-up opcodes which are:
    // 1) <Set Stream Path> [physical address of STB]
    // 2) <User Control Pressed> ("Power", "Power On" or "Power Toggle")
    bool wakeup = false;
    uint8_t opcode = message->body[0];

    switch (opcode) {
        case CEC_MESSAGE_SET_STREAM_PATH: {
            uint16_t physicalAddr = message->body[1] * 256 + message->body[2];
            if (mNexusHdmiCecDevice->mCecPhysicalAddr == physicalAddr) {
                ALOGV("%s: Waking up on <Set Stream Path> opcode...", __PRETTY_FUNCTION__);
                wakeup = true;
            }
        } break;

        case CEC_MESSAGE_USER_CONTROL_PRESSED: {
            uint8_t uiCommand = message->body[1];
            if (uiCommand == CEC_UI_COMMAND_POWER || uiCommand == CEC_UI_COMMAND_POWER_ON || uiCommand == CEC_UI_COMMAND_POWER_TOGGLE) {
                ALOGV("%s: Waking up on <User Control Pressed>[0x%02x] opcode...", __PRETTY_FUNCTION__, uiCommand);
                wakeup = true;
            }
        } break;

        default:
            wakeup = false;
    }
    return wakeup;
}

status_t NexusHdmiCecDevice::HdmiCecMessageEventListener::onHdmiCecMessageReceived(int32_t portId, android::INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    bool forwardCecMessage = mNexusHdmiCecDevice->mCecEnable && mNexusHdmiCecDevice->mCecSystemControlEnable;
    bool sendHotplugWakeUpEvent = false;

    // If we receive CEC commands whilst we are in standby, then in order
    // to be able to send CEC commands, we need Nexus to be out of standby.
    mNexusHdmiCecDevice->standbyLock();
    if (mNexusHdmiCecDevice->mStandby) {
        mNexusHdmiCecDevice->standbyUnlock();
        b_powerState powerState = mNexusHdmiCecDevice->pIpcClient->getPowerState();

        // If we are in S1, then we need to check the validity of the wake-up message before
        // decoding whether to pass it up to Android.  For the other standby modes, we would
        // only reach here if we were already woken up, so we can pass the message on to
        // Android and also send a hotplug "connected" event to wake it up.
        if ((powerState == ePowerState_S1 && isValidWakeupCecMessage(message)) ||
            (powerState != ePowerState_S0 && powerState != ePowerState_S1)) {
            forwardCecMessage = true;
            sendHotplugWakeUpEvent = true;

            mNexusHdmiCecDevice->standbyLock();
            mNexusHdmiCecDevice->mStandby = false;
            mNexusHdmiCecDevice->standbyUnlock();
        }
        else {
            // Don't forward the CEC message if in S1 and it was an invalid wake-up opcode or
            // we are in S0 (i.e. not fully in standby yet).
            forwardCecMessage = false;
        }
    }
    else {
        mNexusHdmiCecDevice->standbyUnlock();
    }

    hdmi_event_t hdmiCecEvent;

    hdmiCecEvent.type = HDMI_EVENT_CEC_MESSAGE;
    hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
    hdmiCecEvent.cec.initiator = (cec_logical_address_t)message->initiator;
    hdmiCecEvent.cec.destination = (cec_logical_address_t)message->destination;
    hdmiCecEvent.cec.length = message->length;
    memcpy(hdmiCecEvent.cec.body, message->body, message->length);

    ALOGV("%s: portId=%d, initiator=%d, destination=%d, length=%d, opcode=0x%02x", __PRETTY_FUNCTION__, portId,
            hdmiCecEvent.cec.initiator, hdmiCecEvent.cec.destination,hdmiCecEvent.cec.length, hdmiCecEvent.cec.body[0]);

    if (mNexusHdmiCecDevice->mCallback != NULL) {
        if (forwardCecMessage == true) {
            ALOGV("%s: About to call port %d CEC MESSAGE callback function %p...", __PRETTY_FUNCTION__, portId, mNexusHdmiCecDevice->mCallback);
            (*mNexusHdmiCecDevice->mCallback)(&hdmiCecEvent, mNexusHdmiCecDevice->mCallbackArg);
        }

        // Send a hotplug "connected" event to the Android HDMI service to wake-up from sleep...
        if (sendHotplugWakeUpEvent == true) {
            ALOGV("%s: About to call port %d HDMI HOTPLUG callback function %p to wake the device up...", __PRETTY_FUNCTION__,
                   portId, mNexusHdmiCecDevice->mCallback);
            mNexusHdmiCecDevice->mHotPlugConnected = true;
            hdmiCecEvent.type = HDMI_EVENT_HOT_PLUG;
            hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
            hdmiCecEvent.hotplug.port_id = portId;
            hdmiCecEvent.hotplug.connected = true;
            (*mNexusHdmiCecDevice->mCallback)(&hdmiCecEvent, mNexusHdmiCecDevice->mCallbackArg);
        }
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

    if (mNexusHdmiCecDevice->mHotPlugConnected != connected) {
        hdmiCecEvent.type = HDMI_EVENT_HOT_PLUG;
        hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
        hdmiCecEvent.hotplug.port_id = portId;
        hdmiCecEvent.hotplug.connected = connected;
        mNexusHdmiCecDevice->mHotPlugConnected = connected;

        if (mNexusHdmiCecDevice->mCallback != NULL) {
            ALOGV("%s: About to call port %d HDMI HOTPLUG callback function %p...", __PRETTY_FUNCTION__, portId, mNexusHdmiCecDevice->mCallback);
            (*mNexusHdmiCecDevice->mCallback)(&hdmiCecEvent, mNexusHdmiCecDevice->mCallbackArg);
        }
    }
    return NO_ERROR;
}

bool NexusHdmiCecDevice::standbyMonitor(void *ctx)
{
    NexusHdmiCecDevice *dev = reinterpret_cast<NexusHdmiCecDevice *>(ctx);

    Mutex::Autolock autoLock(dev->mStandbyLock);

    ALOGV("%s: Entering standby", __PRETTY_FUNCTION__);
    dev->mStandby = true;
    return true;
}

NexusHdmiCecDevice::NexusHdmiCecDevice(uint32_t cecId) : mCecId(cecId), mCecLogicalAddr(UNDEFINED_LOGICAL_ADDRESS),
                                                         mCecPhysicalAddr(UNDEFINED_PHYSICAL_ADDRESS), mCecEnable(true),
                                                         mCecSystemControlEnable(true), mCecViewOnCmdPending(false), mStandby(false),
                                                         mHotPlugConnected(false), pIpcClient(NULL), pNexusClientContext(NULL), mCallback(NULL),
                                                         mHdmiCecDevice(NULL), mHdmiCecMessageEventListener(NULL), mHdmiHotplugEventListener(NULL)
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
        b_refsw_client_client_configuration clientConfig;

        memset(&clientConfig, 0, sizeof(clientConfig));
        clientConfig.standbyMonitorCallback = standbyMonitor;
        clientConfig.standbyMonitorContext  = this;

        pNexusClientContext = pIpcClient->createClientContext(&clientConfig);
        if (pNexusClientContext == NULL) {
            ALOGE("%s: Could not create Nexus Client Context!!!", __PRETTY_FUNCTION__);
            delete pIpcClient;
            pIpcClient = NULL;
        }
        else {
            mHdmiHotplugEventListener = new HdmiHotplugEventListener(this);

            if (mHdmiHotplugEventListener == NULL) {
                ALOGE("%s: cannot create HDMI hotplug event listener!!!", __PRETTY_FUNCTION__);
                pIpcClient->destroyClientContext(pNexusClientContext);
                pNexusClientContext = NULL;
                delete pIpcClient;
                pIpcClient = NULL;
            }
            else {
                // Attempt to register the HDMI Hotplug Event Listener with NexusService
                ret = pIpcClient->addHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                if (ret != NO_ERROR) {
                    ALOGE("%s: could not register HDMI hotplug event listener (rc=%d)!!!", __PRETTY_FUNCTION__, ret);
                    mHdmiHotplugEventListener = NULL;
                    pIpcClient->destroyClientContext(pNexusClientContext);
                    pNexusClientContext = NULL;
                    delete pIpcClient;
                    pIpcClient = NULL;
                }
                else {
                    if (pIpcClient->isCecEnabled(mCecId) == true) {
                        if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
                            ALOGE("%s: cannot get CEC status!!!", __PRETTY_FUNCTION__);
                            pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                            mHdmiHotplugEventListener = NULL;
                            pIpcClient->destroyClientContext(pNexusClientContext);
                            pNexusClientContext = NULL;
                            delete pIpcClient;
                            pIpcClient = NULL;
                            ret = UNKNOWN_ERROR;
                        }
                        else if (cecStatus.ready) {
                            mHdmiCecMessageEventListener = new HdmiCecMessageEventListener(this);

                            if (mHdmiCecMessageEventListener == NULL) {
                                ALOGE("%s: cannot create HDMI CEC message event listener!!!", __PRETTY_FUNCTION__);
                                pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                                mHdmiHotplugEventListener = NULL;
                                pIpcClient->destroyClientContext(pNexusClientContext);
                                pNexusClientContext = NULL;
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
                                    pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                                    mHdmiHotplugEventListener = NULL;
                                    pIpcClient->destroyClientContext(pNexusClientContext);
                                    pNexusClientContext = NULL;
                                    delete pIpcClient;
                                    pIpcClient = NULL;
                                }
                            }
                        }
                        else {
                            ALOGE("%s: CEC%d not ready!!!", __PRETTY_FUNCTION__, mCecId);
                            pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                            mHdmiHotplugEventListener = NULL;
                            pIpcClient->destroyClientContext(pNexusClientContext);
                            pNexusClientContext = NULL;
                            delete pIpcClient;
                            pIpcClient = NULL;
                            ret = UNKNOWN_ERROR;
                        }
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
            ret = pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
            mHdmiHotplugEventListener = NULL;
        }

        if (mHdmiCecMessageEventListener != NULL) {
            ret = pIpcClient->setHdmiCecMessageEventListener(mCecId, NULL);
            mHdmiCecMessageEventListener = NULL;
        }
        if (pNexusClientContext != NULL) {
            pIpcClient->destroyClientContext(pNexusClientContext);
            pNexusClientContext = NULL;
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

    if (mCecEnable && mCecSystemControlEnable) {
        if (pIpcClient->setCecLogicalAddress(mCecId, addr) == false) {
            ALOGE("%s: cannot add CEC%d logical address 0x%02x!!!", __PRETTY_FUNCTION__, mCecId, addr);
            return UNKNOWN_ERROR;
        }
    }

    mCecLogicalAddr = addr;

    // If the View On Cec command is pending and we have a valid logical address,
    // then we can issue the command to wakeup the connected TV...
    if (mCecLogicalAddr != UNDEFINED_LOGICAL_ADDRESS && mCecViewOnCmdPending) {
        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        }
        else if (pIpcClient->setCecPowerState(mCecId, ePowerState_S0) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __PRETTY_FUNCTION__, mCecId, ePowerState_S0);
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

    if (property_get(PROPERTY_HDMI_TX_STANDBY_CEC, value, DEFAULT_PROPERTY_HDMI_TX_STANDBY_CEC)) {
        tx = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return tx;
}

bool NexusHdmiCecDevice::getCecTransmitViewOn()
{
    char value[PROPERTY_VALUE_MAX];
    bool tx=false;

    if (property_get(PROPERTY_HDMI_TX_VIEW_ON_CEC, value, DEFAULT_PROPERTY_HDMI_TX_VIEW_ON_CEC)) {
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

    mCecSystemControlEnable = enable;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
    }
    else {
        bool tx;
        b_powerState state;

        if (enable) {
            state = ePowerState_S0;
            standbyLock();
            mStandby = false;
            standbyUnlock();
            tx = getCecTransmitViewOn();

            // If the logical address of the STB has not been setup, then we must delay
            // sending the View On Cec command...
            if (tx && mCecLogicalAddr == UNDEFINED_LOGICAL_ADDRESS) {
                mCecViewOnCmdPending = true;
                tx = false;
            }
        }
        else {
            standbyLock();
            mStandby = true;
            standbyUnlock();
            mCecViewOnCmdPending = false;
            state = ePowerState_S3;
            tx = getCecTransmitStandby();
        }

        if (tx && pIpcClient->setCecPowerState(mCecId, state) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __PRETTY_FUNCTION__, mCecId, state);
        }
    }
}

status_t NexusHdmiCecDevice::getConnectedState(int *pIsConnected)
{
    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    b_hdmiOutputStatus hdmiOutputStatus;

    if (pIpcClient->getHdmiOutputStatus(mCecId, &hdmiOutputStatus) != true) {
        *pIsConnected = HDMI_NOT_CONNECTED;
        ALOGE("%s: cannot get HDMI%d output status!!!", __PRETTY_FUNCTION__, mCecId);
        return UNKNOWN_ERROR;
    }
    *pIsConnected = hdmiOutputStatus.connected ? HDMI_CONNECTED : HDMI_NOT_CONNECTED;
    return NO_ERROR;
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

    if (mCecPhysicalAddr == UNDEFINED_PHYSICAL_ADDRESS) {
        b_cecStatus cecStatus;

        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
            return NO_INIT;
        }

        if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
            ALOGE("%s: cannot get CEC status!!!", __PRETTY_FUNCTION__);
            return UNKNOWN_ERROR;
        }
        mCecPhysicalAddr = (cecStatus.physicalAddress[0] * 256) + cecStatus.physicalAddress[1];
    }
    *addr = mCecPhysicalAddr;
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::getCecVersion(int* version)
{
    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        return NO_INIT;
    }

    b_cecStatus cecStatus;

    if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
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
    status_t ret;
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
    ret = getCecPhysicalAddress(&mPortInfo[0].physical_address);

    list[0] = mPortInfo;
    *total = 1; // TODO use NEXUS_NUM_CEC
    return ret;
}

