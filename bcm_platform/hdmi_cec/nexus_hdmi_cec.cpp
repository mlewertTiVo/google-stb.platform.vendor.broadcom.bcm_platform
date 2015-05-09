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

/******************************************************************************
  HdmiCecRxMessageHandler methods
******************************************************************************/
NexusHdmiCecDevice::HdmiCecRxMessageHandler::HdmiCecRxMessageHandler(sp<NexusHdmiCecDevice> device) : AHandler(), mNexusHdmiCecDevice(device)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::HdmiCecRxMessageHandler::~HdmiCecRxMessageHandler()
{
    HDMI_CEC_TRACE_ENTER;
}

bool NexusHdmiCecDevice::HdmiCecRxMessageHandler::isValidWakeupCecMessage(cec_message_t *message)
{
    // Check for valid wake-up opcodes which are:
    // 1) <Set Stream Path> [physical address of STB]
    // 2) <User Control Pressed> ("Power", "Power On" or "Power Toggle")
    bool wakeup = false;
    unsigned char opcode = message->body[0];
    int portId = mNexusHdmiCecDevice->mCecId;

    HDMI_CEC_TRACE_ENTER;

    switch (opcode) {
        case CEC_MESSAGE_SET_STREAM_PATH: {
            uint16_t physicalAddr = message->body[1] * 256 + message->body[2];
            uint16_t currentPhysicalAddr;

            if (message->destination == CEC_ADDR_BROADCAST) {
                if (mNexusHdmiCecDevice->getCecPhysicalAddress(&currentPhysicalAddr) == NO_ERROR) {
                    if (currentPhysicalAddr == physicalAddr) {
                        ALOGV("%s: Waking up on <Set Stream Path> opcode on CEC port %d...", __PRETTY_FUNCTION__, portId);
                        wakeup = true;
                    }
                }
            }
        } break;

        case CEC_MESSAGE_USER_CONTROL_PRESSED: {
            uint8_t uiCommand = message->body[1];

            if (mNexusHdmiCecDevice->mCecLogicalAddr == message->destination) {
                if (uiCommand == CEC_UI_COMMAND_POWER || uiCommand == CEC_UI_COMMAND_POWER_ON || uiCommand == CEC_UI_COMMAND_POWER_TOGGLE) {
                    ALOGV("%s: Waking up on <User Control Pressed>[0x%02x] opcode on CEC port %d...", __PRETTY_FUNCTION__, uiCommand, portId);
                    wakeup = true;
                }
            }
        } break;

        default:
            wakeup = false;
    }
    return wakeup;
}

status_t NexusHdmiCecDevice::HdmiCecRxMessageHandler::processCecMessage(cec_message_t *message)
{
    status_t ret = NO_ERROR;
    bool sendMessage = false;
    int portId = mNexusHdmiCecDevice->mCecId;
    uint8_t destAddr = message->destination;
    unsigned char opcode = message->body[0];
    cec_message_t txMessage;

    HDMI_CEC_TRACE_ENTER;

    switch (opcode) {
        case CEC_MESSAGE_USER_CONTROL_PRESSED:
        case CEC_MESSAGE_USER_CONTROL_RELEASED:
        case CEC_MESSAGE_INACTIVE_SOURCE:
        {
                ALOGV("%s: Ignoring opcode 0x%02x CEC port %d...", __PRETTY_FUNCTION__, opcode, portId);
        } break;

        case CEC_MESSAGE_GIVE_DEVICE_POWER_STATUS: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                Mutex::Autolock autoLock(mNexusHdmiCecDevice->mStandbyLock);
                txMessage.initiator = message->destination;
                txMessage.destination = message->initiator;
                txMessage.length = 2;
                txMessage.body[0] = CEC_MESSAGE_REPORT_POWER_STATUS;
                txMessage.body[1] = mNexusHdmiCecDevice->mStandby ? 0x01 : 0x00;
                ALOGV("%s: Sending <Report Power Status> on CEC port %d...", __PRETTY_FUNCTION__, portId);
                sendMessage = true;
            }
        } break;

        case CEC_MESSAGE_GIVE_DEVICE_VENDOR_ID: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                uint32_t vendorId;
                if (mNexusHdmiCecDevice->getCecVendorId(&vendorId) == NO_ERROR) {
                    txMessage.initiator = message->destination;
                    txMessage.destination = CEC_ADDR_BROADCAST;
                    txMessage.length = 4;
                    txMessage.body[0] = CEC_MESSAGE_DEVICE_VENDOR_ID;
                    txMessage.body[1] = (vendorId & 0xFF0000) >> 16;
                    txMessage.body[2] = (vendorId & 0x00FF00) >> 8;
                    txMessage.body[3] = (vendorId & 0x0000FF) >> 0;
                    ALOGV("%s: Sending <Device Vendor ID> on CEC port %d...", __PRETTY_FUNCTION__, portId);
                    sendMessage = true;
                }
            }
        } break;

        case CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                uint16_t physicalAddr;
                if (mNexusHdmiCecDevice->getCecPhysicalAddress(&physicalAddr) == NO_ERROR) {
                    b_cecDeviceType type = mNexusHdmiCecDevice->pIpcClient->getCecDeviceType(mNexusHdmiCecDevice->mCecId);
                    if (type >= 0 && type < eCecDeviceType_eInvalid) {
                        txMessage.initiator = message->destination;
                        txMessage.destination = CEC_ADDR_BROADCAST;
                        txMessage.length = 4;
                        txMessage.body[0] = CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS;
                        txMessage.body[1] = (physicalAddr & 0xFF00) >> 8;
                        txMessage.body[2] = (physicalAddr & 0x00FF) >> 0;
                        txMessage.body[3] = type;
                        ALOGV("%s: Sending <Report Physical Address> on CEC port %d...", __PRETTY_FUNCTION__, portId);
                        sendMessage = true;
                    }
                }
            }
        } break;

        case CEC_MESSAGE_GET_CEC_VERSION: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                int version;
                if (mNexusHdmiCecDevice->getCecVersion(&version) == NO_ERROR) {
                    txMessage.initiator = message->destination;
                    txMessage.destination = message->initiator;
                    txMessage.length = 2;
                    txMessage.body[0] = CEC_MESSAGE_CEC_VERSION;
                    txMessage.body[1] = version & 0xff;
                    ALOGV("%s: Sending <CEC Version> on CEC port %d...", __PRETTY_FUNCTION__, portId);
                    sendMessage = true;
                }
            }
        } break;

        default: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                txMessage.initiator = message->destination;
                txMessage.destination = message->initiator;
                txMessage.length = 3;
                txMessage.body[0] = CEC_MESSAGE_FEATURE_ABORT;
                txMessage.body[1] = opcode;
                txMessage.body[2] = 0x01;   // Abort Reason = "Not in correct mode to respond"
                ALOGV("%s: Sending <Feature Abort> on CEC port %d...", __PRETTY_FUNCTION__, portId);
                sendMessage = true;
            }
        }
    }

    if (sendMessage) {
        ret = mNexusHdmiCecDevice->sendCecMessage(&txMessage, NexusIPCCommon::DEFAULT_MAX_CEC_RETRIES);

        if (ret != NO_ERROR) {
            ALOGE("%s: Could not send CEC message opcode=0x%02x on CEC port %d!!!", __PRETTY_FUNCTION__, txMessage.body[0], portId);
        }
    }
    return ret;
}

status_t NexusHdmiCecDevice::HdmiCecRxMessageHandler::handleCecMessage(const sp<AMessage> &msg)
{
    status_t ret = NO_ERROR;
    uint8_t portId;
    int32_t initiator;
    int32_t destination;
    size_t  length;

    HDMI_CEC_TRACE_ENTER;

    portId = mNexusHdmiCecDevice->mCecId;

    msg->findInt32("srcAddr",  &initiator);
    msg->findInt32("destAddr", &destination);
    msg->findSize("length", &length);

    if (initiator > NexusHdmiCecDevice::MAX_LOGICAL_ADDRESS) {
        ALOGE("%s: Invalid initiator logical address of %d on CEC port %d!!!", __PRETTY_FUNCTION__, initiator, portId);
        ret = BAD_VALUE;
    }
    else if (destination > NexusHdmiCecDevice::MAX_LOGICAL_ADDRESS) {
        ALOGE("%s: Invalid destination logical address of %d on CEC port %d!!!", __PRETTY_FUNCTION__, destination, portId);
        ret = BAD_VALUE;
    }
    else if (length > CEC_MESSAGE_BODY_MAX_LENGTH) {
        ALOGE("%s: Message length %d out of range on CEC port %d!!!", __PRETTY_FUNCTION__, length, portId);
        ret = BAD_VALUE;
    }
    else {
        cec_message_t cecMessage;
        bool forwardCecMessage = mNexusHdmiCecDevice->mCecEnable && mNexusHdmiCecDevice->mCecSystemControlEnable;
        bool sendHotplugWakeUpEvent = false;

        memset(&cecMessage, 0, sizeof(cecMessage));
        cecMessage.initiator = NexusHdmiCecDevice::toCecLogicalAddressT(initiator);
        cecMessage.destination = NexusHdmiCecDevice::toCecLogicalAddressT(destination);
        cecMessage.length = length;

        if (length > 0) {
            sp<ABuffer> body;
            msg->findBuffer("body", &body);
            if (body != NULL) {
                memcpy(cecMessage.body, body->base(), length);
            }
        }

        ALOGV("%s: CEC portId=%d, initiator=%d, destination=%d, length=%d, opcode=0x%02x", __PRETTY_FUNCTION__, portId,
                initiator, destination, length, cecMessage.body[0]);

        // If we receive CEC commands whilst we are in standby, then in order
        // to be able to send CEC commands, we need Nexus to be out of standby.
        mNexusHdmiCecDevice->standbyLock();
        if (mNexusHdmiCecDevice->mStandby) {
            b_powerState powerState;

            mNexusHdmiCecDevice->standbyUnlock();

            powerState = mNexusHdmiCecDevice->pIpcClient->getPowerState();

            // If we are in S0.5 (effectively S0) or S1, then we need to check the validity of the wake-up message before
            // deciding whether to pass it up to Android.  For the other standby modes, we would
            // only reach here if we were already woken up, so we can pass the message on to
            // Android and also send a hotplug "connected" event to wake it up.
            if (((powerState == ePowerState_S0 || powerState == ePowerState_S1) && isValidWakeupCecMessage(&cecMessage)) ||
                (powerState != ePowerState_S0 && powerState != ePowerState_S1)) {
                Mutex::Autolock autoLock(mNexusHdmiCecDevice->mStandbyLock);
                forwardCecMessage = true;
                sendHotplugWakeUpEvent = true;
                mNexusHdmiCecDevice->mStandby = false;
            }
            else {
                // Don't forward the CEC message if in S0.5 or S1 and it was an invalid wake-up opcode or
                // we are in S0 (i.e. not fully in standby yet) or S1.
                forwardCecMessage = false;

                // If we are in S0.5 (effectively S0), then the AutoCEC h/w is disabled as the drivers are fully running.
                // However, we still need to respond to some received CEC messages in order to ensure the TV's state machine
                // can continue to run to allow a valid wake-up CEC message to be sent.
                if (powerState == ePowerState_S0) {
                    processCecMessage(&cecMessage);
                }
            }
        }
        else {
            mNexusHdmiCecDevice->standbyUnlock();
        }

        hdmi_event_t hdmiCecEvent;

        // Forward the CEC message to the Android HDMI service...
        if (forwardCecMessage == true) {
            hdmiCecEvent.type = HDMI_EVENT_CEC_MESSAGE;
            hdmiCecEvent.dev = mNexusHdmiCecDevice->mHdmiCecDevice;
            memcpy(&hdmiCecEvent.cec, &cecMessage, sizeof(hdmiCecEvent.cec));

            ALOGV("%s: About to call CEC port %d CEC MESSAGE callback function...", __PRETTY_FUNCTION__, portId);
            mNexusHdmiCecDevice->fireEventCallback(&hdmiCecEvent);
        }

        // Send a fake hotplug "connected" event to the Android HDMI service to wake-up from sleep...
        if (sendHotplugWakeUpEvent == true) {
            mNexusHdmiCecDevice->hotplugLock();
            // Reset the CEC physical address when we wake-up, as the HDMI port may have
            // changed whilst we were in standby...
            mNexusHdmiCecDevice->mCecPhysicalAddr = NexusHdmiCecDevice::UNDEFINED_PHYSICAL_ADDRESS;
            mNexusHdmiCecDevice->hotplugUnlock();
            ALOGV("%s: About to call port %d HDMI HOTPLUG callback function to wake the device up...", __PRETTY_FUNCTION__, portId);
            mNexusHdmiCecDevice->fireHotplugCallback(true);
        }
    }
    return ret;
}

void NexusHdmiCecDevice::HdmiCecRxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    uint8_t portId = mNexusHdmiCecDevice->mCecId;

    switch (msg->what())
    {
        case kWhatHandleMsg:
            ALOGV("%s: handling CEC port %d message...", __PRETTY_FUNCTION__, portId);
            handleCecMessage(msg);
            break;
        default:
            ALOGE("%s: Invalid message received on CEC port %d - ignoring!", __PRETTY_FUNCTION__, portId);
    }
}

/******************************************************************************
  HdmiCecMessageEventListener methods
******************************************************************************/
NexusHdmiCecDevice::HdmiCecMessageEventListener::HdmiCecMessageEventListener(sp<NexusHdmiCecDevice> device) : mNexusHdmiCecDevice(device)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::HdmiCecMessageEventListener::~HdmiCecMessageEventListener()
{
    HDMI_CEC_TRACE_ENTER;
}

status_t NexusHdmiCecDevice::HdmiCecMessageEventListener::onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    ALOGV("%s: CEC portId=%d, from %d, to %d, length %d, opcode=0x%02x", __PRETTY_FUNCTION__, portId,
            message->initiator, message->destination, message->length, message->body[0]);

    sp<AMessage> msg = new AMessage;
    sp<ABuffer> buf = new ABuffer(message->length);

    msg->setInt32("srcAddr", message->initiator);
    msg->setInt32("destAddr", message->destination);
    msg->setSize("length",  message->length);

    if (message->length > 0) {
        buf->setRange(0, 0);
        memcpy(buf->data(), &message->body[0], message->length);
        buf->setRange(0, message->length);
        msg->setBuffer("body", buf);
    }
    msg->setWhat(NexusHdmiCecDevice::HdmiCecRxMessageHandler::kWhatHandleMsg);
    msg->setTarget(mNexusHdmiCecDevice->mHdmiCecRxMessageHandler->id());
    msg->post();

    return NO_ERROR;
}

/******************************************************************************
  HdmiHotplugEventListener methods
******************************************************************************/
NexusHdmiCecDevice::HdmiHotplugEventListener::HdmiHotplugEventListener(sp<NexusHdmiCecDevice> device) : mNexusHdmiCecDevice(device)
{
    HDMI_CEC_TRACE_ENTER;
}

NexusHdmiCecDevice::HdmiHotplugEventListener::~HdmiHotplugEventListener()
{
    HDMI_CEC_TRACE_ENTER;
}

status_t NexusHdmiCecDevice::HdmiHotplugEventListener::onHdmiHotplugEventReceived(int32_t portId, bool connected)
{
    HDMI_CEC_TRACE_ENTER;

    ALOGV("%s: HDMI%d %s", __PRETTY_FUNCTION__, portId, connected ? "connected" : "disconnected");

    if (connected)
    {
        // Reset the CEC physical address when hotplug connected event occurs...
        Mutex::Autolock autoLock(mNexusHdmiCecDevice->mHotplugLock);
        mNexusHdmiCecDevice->mCecPhysicalAddr = NexusHdmiCecDevice::UNDEFINED_PHYSICAL_ADDRESS;
    }

    if (mNexusHdmiCecDevice->mHotplugConnected != connected) {
        uint16_t addr;

        // If not in standby or the STB is configured to allow waking up the box on reception of
        // an HDMI hotplug event, then allow the event to propogate to Android if there has
        // been a change in the "connected" state.
        mNexusHdmiCecDevice->standbyLock();
        if (!mNexusHdmiCecDevice->mStandby || (mNexusHdmiCecDevice->mStandby && mNexusHdmiCecDevice->getHdmiHotplugWakeup())) {
            mNexusHdmiCecDevice->standbyUnlock();
            ALOGV("%s: About to call CEC port %d HDMI HOTPLUG callback...", __PRETTY_FUNCTION__, portId);
            mNexusHdmiCecDevice->fireHotplugCallback(connected);
        }
        else {
            mNexusHdmiCecDevice->standbyUnlock();
        }
    }
    return NO_ERROR;
}

/******************************************************************************
  NexusHdmiCecDevice methods
******************************************************************************/
cec_logical_address_t NexusHdmiCecDevice::toCecLogicalAddressT(uint8_t addr)
{
    cec_logical_address_t logicalAddr = CEC_ADDR_UNREGISTERED;

    switch (addr) {
        case 0:
            logicalAddr = CEC_ADDR_TV;
            break;
        case 1:
            logicalAddr = CEC_ADDR_RECORDER_1;
            break;
        case 2:
            logicalAddr = CEC_ADDR_RECORDER_2;
            break;
        case 3:
            logicalAddr = CEC_ADDR_TUNER_1;
            break;
        case 4:
            logicalAddr = CEC_ADDR_PLAYBACK_1;
            break;
        case 5:
            logicalAddr = CEC_ADDR_AUDIO_SYSTEM;
            break;
        case 6:
            logicalAddr = CEC_ADDR_TUNER_2;
            break;
        case 7:
            logicalAddr = CEC_ADDR_TUNER_3;
            break;
        case 8:
            logicalAddr = CEC_ADDR_PLAYBACK_2;
            break;
        case 9:
            logicalAddr = CEC_ADDR_RECORDER_3;
            break;
        case 10:
            logicalAddr = CEC_ADDR_TUNER_4;
            break;
        case 11:
            logicalAddr = CEC_ADDR_PLAYBACK_3;
            break;
        case 12:
            logicalAddr = CEC_ADDR_RESERVED_1;
            break;
        case 13:
            logicalAddr = CEC_ADDR_RESERVED_2;
            break;
        case 14:
            logicalAddr = CEC_ADDR_FREE_USE;
            break;
        case 15:
            logicalAddr = CEC_ADDR_BROADCAST;
            break;
    }
    return logicalAddr;
}

bool NexusHdmiCecDevice::standbyMonitor(void *ctx)
{
    NexusHdmiCecDevice *dev = reinterpret_cast<NexusHdmiCecDevice *>(ctx);

    Mutex::Autolock autoLock(dev->mStandbyLock);

    ALOGV("%s: Entering standby", __PRETTY_FUNCTION__);
    dev->mStandby = true;
    return true;
}

NexusHdmiCecDevice::NexusHdmiCecDevice(int cecId) : mCecId(cecId), mCecLogicalAddr(UNDEFINED_LOGICAL_ADDRESS),
                                                    mCecPhysicalAddr(UNDEFINED_PHYSICAL_ADDRESS), mCecVendorId(UNKNOWN_VENDOR_ID), mCecEnable(true),
                                                    mCecSystemControlEnable(true), mCecViewOnCmdPending(false), mStandby(false),
                                                    mHotplugConnected(false), pIpcClient(NULL), pNexusClientContext(NULL), mCallback(NULL),
                                                    mHdmiCecDevice(NULL), mHdmiCecMessageEventListener(NULL), mHdmiHotplugEventListener(NULL),
                                                    mHdmiCecRxMessageHandler(NULL), mHdmiCecRxMessageLooper(NULL)
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
            goto fail_create_client_context;
        }
        else {
            mHdmiHotplugEventListener = NexusHdmiCecDevice::HdmiHotplugEventListener::instantiate(this);

            if (mHdmiHotplugEventListener == NULL) {
                ALOGE("%s: cannot create HDMI hotplug event listener!!!", __PRETTY_FUNCTION__);
                goto fail_create_hotplug_listener;
            }
            else {
                // Attempt to register the HDMI Hotplug Event Listener with NexusService
                ret = pIpcClient->addHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
                if (ret != NO_ERROR) {
                    ALOGE("%s: could not add HDMI hotplug event listener (rc=%d)!!!", __PRETTY_FUNCTION__, ret);
                    goto fail_add_hotplug_listener;
                }
                else {
                    if (NEXUS_NUM_CEC > 0) {
                        if (pIpcClient->getCecStatus(mCecId, &cecStatus) != true) {
                            ALOGE("%s: cannot get CEC status!!!", __PRETTY_FUNCTION__);
                            ret = UNKNOWN_ERROR;
                            goto fail_get_cec_status;
                        }
                        else if (cecStatus.ready) {
                            // Create looper to receive CEC messages...
                            char looperName[] = "Hdmi Cec  Rx Message Looper";
                            looperName[8] = '0' + mCecId;

                            mHdmiCecRxMessageLooper = new ALooper;
                            if (mHdmiCecRxMessageLooper.get() == NULL) {
                                ALOGE("%s: could not create CEC%d messasge looper!!!", __PRETTY_FUNCTION__, mCecId);
                                ret = NO_INIT;
                                goto fail_create_looper;
                            }
                            else {
                                mHdmiCecRxMessageLooper->setName(looperName);
                                mHdmiCecRxMessageLooper->start();

                                // Create Handler to handle reception of CEC messages...
                                mHdmiCecRxMessageHandler = NexusHdmiCecDevice::HdmiCecRxMessageHandler::instantiate(this);

                                if (mHdmiCecRxMessageHandler.get() == NULL) {
                                    ALOGE("%s: could not create CEC%d message handler!!!", __PRETTY_FUNCTION__, mCecId);
                                    ret = NO_INIT;
                                    goto fail_create_message_handler;
                                }
                                else {
                                    mHdmiCecRxMessageLooper->registerHandler(mHdmiCecRxMessageHandler);

                                    mHdmiCecMessageEventListener = NexusHdmiCecDevice::HdmiCecMessageEventListener::instantiate(this);

                                    if (mHdmiCecMessageEventListener.get() == NULL) {
                                        ALOGE("%s: cannot create HDMI CEC message event listener!!!", __PRETTY_FUNCTION__);
                                        ret = NO_INIT;
                                        goto fail_create_event_listener;
                                    }
                                    else {
                                        // Attempt to register the HDMI CEC Message Event Listener with NexusService
                                        ret = pIpcClient->setHdmiCecMessageEventListener(mCecId, mHdmiCecMessageEventListener);
                                        if (ret != NO_ERROR) {
                                            ALOGE("%s: could not register HDMI CEC message event listener (rc=%d)!!!", __PRETTY_FUNCTION__, ret);
                                            goto fail_set_event_listener;
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            ALOGE("%s: CEC%d not ready!!!", __PRETTY_FUNCTION__, mCecId);
                            ret = UNKNOWN_ERROR;
                            goto fail_get_cec_status;
                        }
                    }
                    else {
                        ALOGE("%s: No CEC ports available on device!!!", __PRETTY_FUNCTION__);
                        ret = NO_INIT;
                    }
                }
            }
        }
    }
    return ret;

fail_set_event_listener:
    mHdmiCecMessageEventListener = NULL;
fail_create_event_listener:
    mHdmiCecRxMessageLooper->unregisterHandler(mHdmiCecRxMessageHandler->id());
fail_create_message_handler:
    mHdmiCecRxMessageLooper->stop();
    mHdmiCecRxMessageHandler = NULL;
    mHdmiCecRxMessageLooper = NULL;
fail_get_cec_status:
fail_create_looper:
    pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
fail_add_hotplug_listener:
    mHdmiHotplugEventListener = NULL;
fail_create_hotplug_listener:
    pIpcClient->destroyClientContext(pNexusClientContext);
    pNexusClientContext = NULL;
fail_create_client_context:
    delete pIpcClient;
    pIpcClient = NULL;

    return ret;
}

status_t NexusHdmiCecDevice::uninitialise()
{
    status_t ret = NO_ERROR;

    if (pIpcClient != NULL) {
        if (mHdmiCecMessageEventListener.get() != NULL) {
            mHdmiCecMessageEventListener = NULL;
        }

        if (mHdmiCecRxMessageHandler.get() != NULL && mHdmiCecRxMessageLooper.get() != NULL) {
            mHdmiCecRxMessageLooper->unregisterHandler(mHdmiCecRxMessageHandler->id());
            mHdmiCecRxMessageLooper->stop();
            mHdmiCecRxMessageHandler = NULL;
            mHdmiCecRxMessageLooper = NULL;
        }

        if (mHdmiHotplugEventListener.get() != NULL) {
            ret = pIpcClient->removeHdmiHotplugEventListener(mCecId, mHdmiHotplugEventListener);
            mHdmiHotplugEventListener = NULL;
        }

        if (mHdmiCecMessageEventListener.get() != NULL) {
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

bool NexusHdmiCecDevice::getHdmiHotplugWakeup()
{
    char value[PROPERTY_VALUE_MAX];
    bool wakeup=false;

    if (property_get(PROPERTY_HDMI_HOTPLUG_WAKEUP, value, DEFAULT_PROPERTY_HDMI_HOTPLUG_WAKEUP)) {
        wakeup = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return wakeup;
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

            hotplugLock();
            // Reset the CEC physical address when we wake-up, as the HDMI port may have
            // changed whilst we were in standby...
            mCecPhysicalAddr = NexusHdmiCecDevice::UNDEFINED_PHYSICAL_ADDRESS;
            hotplugUnlock();

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

status_t NexusHdmiCecDevice::getHdmiOutputStatus(b_hdmiOutputStatus *pHdmiOutputStatus)
{
    status_t ret = NO_ERROR;

    if (pIpcClient == NULL) {
        ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
        ret = NO_INIT;
    }
    else {
        b_hdmiOutputStatus hdmiOutputStatus;

        if (pIpcClient->getHdmiOutputStatus(mCecId, &hdmiOutputStatus) != true) {
            ALOGE("%s: cannot get HDMI%d output status!!!", __PRETTY_FUNCTION__, mCecId);
            ret = UNKNOWN_ERROR;
        }
        else {
            *pHdmiOutputStatus = hdmiOutputStatus;
        }
    }
    return ret;
}

status_t NexusHdmiCecDevice::getConnectedState(int *pIsConnected)
{
    status_t ret;
    b_hdmiOutputStatus hdmiOutputStatus;
    int connected = HDMI_NOT_CONNECTED;

    ret = getHdmiOutputStatus(&hdmiOutputStatus);

    if (ret == NO_ERROR) {
        connected = hdmiOutputStatus.connected ? HDMI_CONNECTED : HDMI_NOT_CONNECTED;
    }
    *pIsConnected = connected;
    return ret;
}

void NexusHdmiCecDevice::registerEventCallback(const hdmi_cec_device_t* dev, event_callback_t callback, void *arg)
{
    HDMI_CEC_TRACE_ENTER;

    mHdmiCecDevice = const_cast<hdmi_cec_device_t*>(dev);
    mCallback = callback;
    mCallbackArg = arg;
}

void NexusHdmiCecDevice::fireEventCallback(hdmi_event_t *pHdmiCecEvent)
{
    HDMI_CEC_TRACE_ENTER;

    if (mCallback != NULL) {
        (*mCallback)(pHdmiCecEvent, mCallbackArg);
    }
}

void NexusHdmiCecDevice::fireHotplugCallback(bool connected)
{
    hdmi_event_t hdmiCecEvent;

    HDMI_CEC_TRACE_ENTER;

    hdmiCecEvent.type = HDMI_EVENT_HOT_PLUG;
    hdmiCecEvent.dev = mHdmiCecDevice;
    hdmiCecEvent.hotplug.port_id = mCecId;
    hdmiCecEvent.hotplug.connected = connected;
    mHotplugConnected = connected;

    fireEventCallback(&hdmiCecEvent);
}

status_t NexusHdmiCecDevice::getCecPhysicalAddress(uint16_t* addr)
{
    status_t ret = NO_ERROR;

    HDMI_CEC_TRACE_ENTER;

    hotplugLock();
    // Is the CEC Physical address cached?
    if (mCecPhysicalAddr == NexusHdmiCecDevice::UNDEFINED_PHYSICAL_ADDRESS) {
        b_hdmiOutputStatus hdmiOutputStatus;

        hotplugUnlock();

        ret = getHdmiOutputStatus(&hdmiOutputStatus);
        if (ret == NO_ERROR) {
            Mutex::Autolock autoLock(mHotplugLock);

            mCecPhysicalAddr = (hdmiOutputStatus.physicalAddress[0] * 256) + hdmiOutputStatus.physicalAddress[1];
            ALOGV("%s: Read CEC Physical Address as %01d.%01d.%01d.%01d", __PRETTY_FUNCTION__,
                    (mCecPhysicalAddr >> 12) & 0x0F,
                    (mCecPhysicalAddr >>  8) & 0x0F,
                    (mCecPhysicalAddr >>  4) & 0x0F,
                    (mCecPhysicalAddr >>  0) & 0x0F);
        }
        else {
            ALOGE("%s: Could not get HDMI%d output status!!!", __PRETTY_FUNCTION__, mCecId);
        }
    }
    else {
        hotplugUnlock();
    }
    *addr = mCecPhysicalAddr;
    return ret;
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
    // Is the value cached?
    if (mCecVendorId == NexusHdmiCecDevice::UNKNOWN_VENDOR_ID) {
        mCecVendorId = property_get_int32(PROPERTY_HDMI_CEC_VENDOR_ID, DEFAULT_PROPERTY_HDMI_CEC_VENDOR_ID);
    }
    *vendor_id = mCecVendorId;
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::sendCecMessage(const cec_message_t *message, uint8_t maxRetries)
{
    HDMI_CEC_TRACE_ENTER;

    if (mCecEnable == true) {
        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
            return NO_INIT;
        }

        uint8_t srcAddr = (uint8_t)message->initiator;
        uint8_t destAddr = (uint8_t)message->destination;

        if (pIpcClient->sendCecMessage(mCecId, srcAddr, destAddr, message->length, const_cast<uint8_t *>(message->body), maxRetries) != true) {
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

