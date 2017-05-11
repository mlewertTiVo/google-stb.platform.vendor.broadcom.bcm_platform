/******************************************************************************
 * (c) 2011-2017 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
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
#include "cutils/properties.h"
#include "nexus_hdmi_cec.h"
#include <nxwrap.h>
#include <nxcec.h>

using namespace android;

/******************************************************************************
  LinuxUInputRef methods
******************************************************************************/
sp<NexusHdmiCecDevice::LinuxUInputRef> NexusHdmiCecDevice::LinuxUInputRef::instantiate()
{
    sp<NexusHdmiCecDevice::LinuxUInputRef> uInput = NULL;

    static const struct input_id id =
    {
        BUS_HOST, /* bustype */
        0, /* vendor */
        0, /* product */
        1  /* version */
    };

    uInput = new NexusHdmiCecDevice::LinuxUInputRef();

    if (uInput != NULL)
    {
        if (!uInput->open() ||
            !uInput->register_event(EV_KEY) ||
            !uInput->register_key(KEY_WAKEUP) ||
            !uInput->start("NexusHdmiCecDevice", id))
        {
            ALOGE("Failed to initialise LinuxUInputRef");
            uInput = NULL;
        }
    }
    return uInput;
}

NexusHdmiCecDevice::LinuxUInputRef::LinuxUInputRef()
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);
}

NexusHdmiCecDevice::LinuxUInputRef::~LinuxUInputRef()
{
    ALOGV("%s: Stopping and closing UInput...", __PRETTY_FUNCTION__);
    stop();
    close();
}

/******************************************************************************
  HdmiCecRxMessageHandler methods
******************************************************************************/
NexusHdmiCecDevice::HdmiCecRxMessageHandler::HdmiCecRxMessageHandler(sp<NexusHdmiCecDevice> device) : AHandler(), mNexusHdmiCecDevice(device)
{
}

NexusHdmiCecDevice::HdmiCecRxMessageHandler::~HdmiCecRxMessageHandler()
{
}

bool NexusHdmiCecDevice::HdmiCecRxMessageHandler::isValidWakeupCecMessage(cec_message_t *message)
{
    // Check for valid wake-up opcodes which are:
    // 1) <Set Stream Path> [physical address of STB]
    // 2) <User Control Pressed> ("Power", "Power On" or "Power Toggle")
    bool wakeup = false;
    unsigned char opcode = message->body[0];

    switch (opcode) {
        case CEC_MESSAGE_SET_STREAM_PATH: {
            uint16_t physicalAddr = message->body[1] * 256 + message->body[2];
            uint16_t currentPhysicalAddr;

            if (message->destination == CEC_ADDR_BROADCAST) {
                if (mNexusHdmiCecDevice->getCecPhysicalAddress(&currentPhysicalAddr) == NO_ERROR) {
                    if (currentPhysicalAddr == physicalAddr) {
                        ALOGV("%s: Waking up on <Set Stream Path> opcode on CEC.", __PRETTY_FUNCTION__);
                        wakeup = true;
                    }
                }
            }
        } break;

        case CEC_MESSAGE_USER_CONTROL_PRESSED: {
            uint8_t uiCommand = message->body[1];

            if (mNexusHdmiCecDevice->mCecLogicalAddr == message->destination) {
                if (uiCommand == CEC_UI_COMMAND_POWER || uiCommand == CEC_UI_COMMAND_POWER_ON || uiCommand == CEC_UI_COMMAND_POWER_TOGGLE) {
                    ALOGV("%s: Waking up on <User Control Pressed>[0x%02x] opcode on CEC.", __PRETTY_FUNCTION__, uiCommand);
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
    uint8_t destAddr = message->destination;
    unsigned char opcode = message->body[0];
    cec_message_t txMessage;

    switch (opcode) {
        case CEC_MESSAGE_USER_CONTROL_PRESSED:
        case CEC_MESSAGE_USER_CONTROL_RELEASED:
        case CEC_MESSAGE_INACTIVE_SOURCE:
        {
                ALOGV("%s: Ignoring opcode 0x%02x CEC.", __PRETTY_FUNCTION__, opcode);
        } break;

        case CEC_MESSAGE_GIVE_DEVICE_POWER_STATUS: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                Mutex::Autolock autoLock(mNexusHdmiCecDevice->mStandbyLock);
                txMessage.initiator = message->destination;
                txMessage.destination = message->initiator;
                txMessage.length = 2;
                txMessage.body[0] = CEC_MESSAGE_REPORT_POWER_STATUS;
                txMessage.body[1] = mNexusHdmiCecDevice->mStandby ? 0x01 : 0x00;
                ALOGV("%s: Sending <Report Power Status> on CEC.", __PRETTY_FUNCTION__);
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
                    ALOGV("%s: Sending <Device Vendor ID> on CEC", __PRETTY_FUNCTION__);
                    sendMessage = true;
                }
            }
        } break;

        case CEC_MESSAGE_GIVE_PHYSICAL_ADDRESS: {
            if (mNexusHdmiCecDevice->mCecLogicalAddr == destAddr) {
                uint16_t physicalAddr;
                if (mNexusHdmiCecDevice->getCecPhysicalAddress(&physicalAddr) == NO_ERROR) {
                    nxcec_cec_device_type type = nxcec_get_cec_device_type();
                    if (type >= 0 && type < eCecDeviceType_eInvalid) {
                        txMessage.initiator = message->destination;
                        txMessage.destination = CEC_ADDR_BROADCAST;
                        txMessage.length = 4;
                        txMessage.body[0] = CEC_MESSAGE_REPORT_PHYSICAL_ADDRESS;
                        txMessage.body[1] = (physicalAddr & 0xFF00) >> 8;
                        txMessage.body[2] = (physicalAddr & 0x00FF) >> 0;
                        txMessage.body[3] = type;
                        ALOGV("%s: Sending <Report Physical Address> on CEC.", __PRETTY_FUNCTION__);
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
                    ALOGV("%s: Sending <CEC Version> on CEC.", __PRETTY_FUNCTION__);
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
                ALOGV("%s: Sending <Feature Abort> on CEC.", __PRETTY_FUNCTION__);
                sendMessage = true;
            }
        }
    }

    if (sendMessage) {
        ret = mNexusHdmiCecDevice->sendCecMessage(&txMessage, DEFAULT_MAX_CEC_RETRIES);
        if (ret != NO_ERROR) {
            ALOGE("%s: Could not send CEC message opcode=0x%02x on CEC (err=%d).", __PRETTY_FUNCTION__, txMessage.body[0], ret);
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

    msg->findInt32("srcAddr",  &initiator);
    msg->findInt32("destAddr", &destination);
    msg->findSize("length", &length);

    if (initiator > NexusHdmiCecDevice::MAX_LOGICAL_ADDRESS) {
        ALOGE("%s: Invalid initiator logical address of %d on CEC.", __PRETTY_FUNCTION__, initiator);
        ret = BAD_VALUE;
    }
    else if (destination > NexusHdmiCecDevice::MAX_LOGICAL_ADDRESS) {
        ALOGE("%s: Invalid destination logical address of %d on CEC.", __PRETTY_FUNCTION__, destination);
        ret = BAD_VALUE;
    }
    else if (length > CEC_MESSAGE_BODY_MAX_LENGTH) {
        ALOGE("%s: Message length %zu out of range on CEC.", __PRETTY_FUNCTION__, length);
        ret = BAD_VALUE;
    }
    else {
        cec_message_t cecMessage;
        bool forwardCecMessage = mNexusHdmiCecDevice->mCecEnable && mNexusHdmiCecDevice->mCecSystemControlEnable;
        bool sendWakeUpEvent = false;

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

        ALOGV("%s: CEC initiator=%d, destination=%d, length=%zu, opcode=0x%02x", __PRETTY_FUNCTION__,
                initiator, destination, length, cecMessage.body[0]);

        // If we receive CEC commands whilst we are in standby, then in order
        // to be able to send CEC commands, we need Nexus to be out of standby.
        mNexusHdmiCecDevice->standbyLock();
        if (mNexusHdmiCecDevice->mStandby) {
            mNexusHdmiCecDevice->standbyUnlock();

            // We always need to check the validity of the received wake-up message before deciding
            // whether to pass it up to the Android framework. If it's valid, then we also spoof
            // a WAKEUP key event to force the framework to wake-up.
            if (isValidWakeupCecMessage(&cecMessage)) {
                Mutex::Autolock autoLock(mNexusHdmiCecDevice->mStandbyLock);
                forwardCecMessage = true;
                sendWakeUpEvent = true;
                mNexusHdmiCecDevice->mStandby = false;
            }
            else {
                nxwrap_pwr_state power;
                bool getpower = false;

                // Don't forward the CEC message if in S0.5 or S1 and it was an invalid wake-up opcode or
                // we are in S0 (i.e. not fully in standby yet) or S1.
                forwardCecMessage = false;

                getpower = nxwrap_get_pwr_info(&power, NULL);
                // If we are in S0.5 (effectively S0), then the AutoCEC h/w is disabled as the drivers are fully running.
                // However, we still need to respond to some received CEC messages in order to ensure the TV's state machine
                // can continue to run to allow a valid wake-up CEC message to be sent.
                if (getpower && (power == ePowerState_S0)) {
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

            ALOGV("%s: About to call CEC MESSAGE callback function...", __PRETTY_FUNCTION__);
            mNexusHdmiCecDevice->fireEventCallback(&hdmiCecEvent);
        }

        // Send a WAKEUP key event to wake-up from sleep...
        if (sendWakeUpEvent == true) {
            ALOGV("%s: About to spoof KEY_WAKEUP event to wake the device up...", __PRETTY_FUNCTION__);
            mNexusHdmiCecDevice->mUInput->emit_key_state(KEY_WAKEUP, true);
            mNexusHdmiCecDevice->mUInput->emit_syn();
            mNexusHdmiCecDevice->mUInput->emit_key_state(KEY_WAKEUP, false);
            mNexusHdmiCecDevice->mUInput->emit_syn();
        }
    }
    return ret;
}

void NexusHdmiCecDevice::HdmiCecRxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatHandleMsg:
            ALOGV("%s: handling CEC message...", __PRETTY_FUNCTION__);
            handleCecMessage(msg);
            break;
        default:
            ALOGE("%s: Invalid message received on CEC - ignoring!", __PRETTY_FUNCTION__);
    }
}

/******************************************************************************
  HdmiHotplugEventListener methods
******************************************************************************/
NexusHdmiCecDevice::HdmiHotplugEventListener::HdmiHotplugEventListener(sp<NexusHdmiCecDevice> device) : mNexusHdmiCecDevice(device)
{
}

NexusHdmiCecDevice::HdmiHotplugEventListener::~HdmiHotplugEventListener()
{
}

status_t NexusHdmiCecDevice::HdmiHotplugEventListener::onHpd(bool connected)
{
    int isConnected = connected ? HDMI_CONNECTED : HDMI_NOT_CONNECTED;

    ALOGV("%s: HDMI %s", __PRETTY_FUNCTION__, connected ? "connected" : "disconnected");

    if (mNexusHdmiCecDevice->mHotplugConnected != isConnected) {
        uint16_t addr;

        // If not in standby or the STB is configured to allow waking up the box on reception of
        // an HDMI hotplug event, then allow the event to propogate to Android if there has
        // been a change in the "connected" state.
        mNexusHdmiCecDevice->standbyLock();
        if (!mNexusHdmiCecDevice->mStandby || (mNexusHdmiCecDevice->mStandby && mNexusHdmiCecDevice->getHdmiHotplugWakeup())) {
            mNexusHdmiCecDevice->standbyUnlock();
            ALOGV("%s: About to call CEC HDMI HOTPLUG callback...", __PRETTY_FUNCTION__);
            mNexusHdmiCecDevice->fireHotplugCallback(isConnected);
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

NexusHdmiCecDevice::NexusHdmiCecDevice() : mCecLogicalAddr(UNDEFINED_LOGICAL_ADDRESS),
                                           mCecPhysicalAddr(UNDEFINED_PHYSICAL_ADDRESS), mCecVendorId(UNKNOWN_VENDOR_ID), mCecEnable(false),
                                           mCecSystemControlEnable(true), mCecViewOnCmdPending(false), mStandby(false),
                                           mHotplugConnected(-1), mNxWrap(NULL), mCallback(NULL),
                                           mHdmiCecDevice(NULL), mHdmiHotplugEventListener(NULL),
                                           mHdmiCecRxMessageHandler(NULL), mHdmiCecRxMessageLooper(NULL), mCecHandle(NULL)
{
}

NexusHdmiCecDevice::~NexusHdmiCecDevice()
{
}

void NexusHdmiCecDevice::cecMsgReceivedCb(void *context, int param) {
   NEXUS_CecReceivedMessage receivedMessage;
   NEXUS_Error rc;
   NEXUS_CecStatus status;

   NexusHdmiCecDevice *pCecDevice = reinterpret_cast<NexusHdmiCecDevice *>(context);
   (void)param;

   rc = NEXUS_Cec_GetStatus(pCecDevice->mCecHandle, &status);
   if (!rc && status.messageReceived) {
      rc = NEXUS_Cec_ReceiveMessage(pCecDevice->mCecHandle, &receivedMessage);
      if (!rc && receivedMessage.data.length > 0) {
         size_t len = (receivedMessage.data.length <= HDMI_CEC_MESSAGE_BODY_MAX_LENGTH) ?
                      receivedMessage.data.length : HDMI_CEC_MESSAGE_BODY_MAX_LENGTH;
         sp<AMessage> msg = new AMessage;
         sp<ABuffer> buf = new ABuffer(len);

         msg->setInt32("srcAddr", receivedMessage.data.initiatorAddr);
         msg->setInt32("destAddr", receivedMessage.data.destinationAddr);
         msg->setSize("length", len);
         buf->setRange(0, 0);
         memcpy(buf->data(), receivedMessage.data.buffer, len);
         buf->setRange(0, len);
         msg->setBuffer("body", buf);
         msg->setWhat(HdmiCecRxMessageHandler::kWhatHandleMsg);
         msg->setTarget(pCecDevice->mHdmiCecRxMessageHandler);
         msg->post();
      }
   }
}

void NexusHdmiCecDevice::cecMsgTransmittedCb(void *context, int param) {
   NexusHdmiCecDevice *pCecDevice = reinterpret_cast<NexusHdmiCecDevice *>(context);
   (void)param;
   pCecDevice->mCecMessageTransmittedLock.lock();
   pCecDevice->mCecMessageTransmittedCondition.broadcast();
   pCecDevice->mCecMessageTransmittedLock.unlock();
}

void NexusHdmiCecDevice::cecDeviceReadyCb(void *context, int param) {
   NEXUS_Error rc;
   NEXUS_CecStatus status;
   NexusHdmiCecDevice *pCecDevice = reinterpret_cast<NexusHdmiCecDevice *>(context);
   (void)param;

   rc = NEXUS_Cec_GetStatus(pCecDevice->mCecHandle, &status);
   if (!rc && (status.physicalAddress[0] != 0xFF) && (status.physicalAddress[1] != 0xFF)) {
      pCecDevice->mCecDeviceReadyLock.lock();
      pCecDevice->mCecDeviceReadyCondition.broadcast();
      pCecDevice->mCecDeviceReadyLock.unlock();
   }
}

status_t NexusHdmiCecDevice::initialise()
{
    NEXUS_PlatformConfiguration *pPlatformConfig;
    NEXUS_CecSettings cecSettings;
    NEXUS_Error errCode;
    NEXUS_HdmiOutputStatus hdmiStatus;
    NEXUS_CecStatus cecStatus;
    status_t status;

    mNxWrap = new NxWrap("Android-HDMI-CEC");
    if (mNxWrap == NULL) {
        goto out_error;
    }
    mNxWrap->join(standbyMonitor, (void *)this);

    pPlatformConfig = reinterpret_cast<NEXUS_PlatformConfiguration *>(BKNI_Malloc(sizeof(*pPlatformConfig)));
    if (pPlatformConfig == NULL) {
        goto out_error;
    }
    NEXUS_Platform_GetConfiguration(pPlatformConfig);
    mCecHandle  = pPlatformConfig->outputs.cec[0];
    mHdmiHandle = pPlatformConfig->outputs.hdmi[0];
    BKNI_Free(pPlatformConfig);
    if (mCecHandle == NULL) {
        NEXUS_Cec_GetDefaultSettings(&cecSettings);
        mCecHandle = NEXUS_Cec_Open(0, &cecSettings);
        if (mCecHandle == NULL) {
            goto out_error;
        }
    }

    errCode = NEXUS_HdmiOutput_GetStatus(mHdmiHandle, &hdmiStatus);
    if (!errCode) {
        NEXUS_Cec_GetSettings(mCecHandle, &cecSettings);

        cecSettings.enabled = false;
        cecSettings.messageReceivedCallback.callback = NexusHdmiCecDevice::cecMsgReceivedCb;
        cecSettings.messageReceivedCallback.context  = this;
        cecSettings.messageReceivedCallback.param    = 0;

        cecSettings.messageTransmittedCallback.callback = NexusHdmiCecDevice::cecMsgTransmittedCb;
        cecSettings.messageTransmittedCallback.context  = this;
        cecSettings.messageTransmittedCallback.param    = 0;

        cecSettings.logicalAddressAcquiredCallback.callback = NexusHdmiCecDevice::cecDeviceReadyCb;
        cecSettings.logicalAddressAcquiredCallback.context  = this;
        cecSettings.logicalAddressAcquiredCallback.param    = 0;

        cecSettings.physicalAddress[0] = hdmiStatus.physicalAddressA << 4 | hdmiStatus.physicalAddressB;
        cecSettings.physicalAddress[1] = hdmiStatus.physicalAddressC << 4 | hdmiStatus.physicalAddressD;

        if (nxcec_get_cec_device_type() != eCecDeviceType_eInvalid) {
            cecSettings.disableLogicalAddressPolling = true;
            cecSettings.logicalAddress = 0xff;
        }
        errCode = NEXUS_Cec_SetSettings(mCecHandle, &cecSettings);
        if (errCode) {
           goto out_error;
        }
        NEXUS_Cec_GetSettings(mCecHandle, &cecSettings);
        cecSettings.enabled = true;
        errCode = NEXUS_Cec_SetSettings(mCecHandle, &cecSettings);
        if (errCode) {
           goto out_error;
        }
        if (nxcec_get_cec_device_type() == eCecDeviceType_eInvalid) {
           for (int loops = 0; loops < 5; loops++) {
              mCecDeviceReadyLock.lock();
              status = mCecDeviceReadyCondition.waitRelative(mCecDeviceReadyLock, 1 * 1000 * 1000 * 1000);
              mCecDeviceReadyLock.unlock();
              if (status == OK) {
                 break;
              }
           }
        }
    }

    mUInput = NexusHdmiCecDevice::LinuxUInputRef::instantiate();
    if (mUInput == NULL) {
       goto out_error;
    }

    mHdmiHotplugEventListener = NexusHdmiCecDevice::HdmiHotplugEventListener::instantiate(this);
    if (mHdmiHotplugEventListener == NULL) {
       goto out_error;
    }
    mNxWrap->regHpdEvt(mHdmiHotplugEventListener);

    if (NEXUS_Cec_GetStatus(mCecHandle, &cecStatus) != NEXUS_SUCCESS) {
       goto out_error;
    }
    mLogicalAddress = cecStatus.logicalAddress;
    if (cecStatus.ready) {
       char looperName[] = "Hdmi Cec Rx Message Looper";

       mHdmiCecRxMessageLooper = new ALooper;
       if (mHdmiCecRxMessageLooper.get() == NULL) {
           goto out_error;
       }
       mHdmiCecRxMessageLooper->setName(looperName);
       mHdmiCecRxMessageLooper->start();
       mHdmiCecRxMessageHandler = HdmiCecRxMessageHandler::instantiate(this);

       if (mHdmiCecRxMessageHandler.get() == NULL) {
          goto out_error;
       }
       mHdmiCecRxMessageLooper->registerHandler(mHdmiCecRxMessageHandler);
       mCecEnable = nxcec_is_cec_enabled();
    }
    return NO_ERROR;

out_error:
    if (mCecHandle) {
        NEXUS_Cec_Close(mCecHandle);
        mCecHandle = NULL;
    }
    if (mHdmiCecRxMessageLooper != NULL) {
        mHdmiCecRxMessageLooper->unregisterHandler(mHdmiCecRxMessageHandler->id());
    }
    if (mHdmiCecRxMessageHandler != NULL) {
        mHdmiCecRxMessageLooper->stop();
    }
    mHdmiCecRxMessageHandler = NULL;
    mHdmiCecRxMessageLooper = NULL;
    mHdmiHotplugEventListener = NULL;
    mUInput = NULL;
    if (mNxWrap) {
       mNxWrap->leave();
       delete mNxWrap;
       mNxWrap = NULL;
    }
    return NO_INIT;
}

status_t NexusHdmiCecDevice::uninitialise()
{
    if (mNxWrap != NULL) {
       if (mHdmiCecRxMessageHandler.get() != NULL && mHdmiCecRxMessageLooper.get() != NULL) {
          mHdmiCecRxMessageLooper->unregisterHandler(mHdmiCecRxMessageHandler->id());
          mHdmiCecRxMessageLooper->stop();
          mHdmiCecRxMessageHandler = NULL;
          mHdmiCecRxMessageLooper = NULL;
       }
       if (mHdmiHotplugEventListener.get() != NULL) {
          mNxWrap->unregHpdEvt(mHdmiHotplugEventListener);
          mHdmiHotplugEventListener = NULL;
       }
       if (mUInput.get() != NULL) {
          mUInput = NULL;
       }
       if (mCecHandle) {
          NEXUS_Cec_Close(mCecHandle);
          mCecHandle = NULL;
       }
       mNxWrap->leave();
       delete mNxWrap;
       mNxWrap = NULL;
    }
    return NO_ERROR;
}

status_t NexusHdmiCecDevice::setCecLogicalAddress(uint8_t addr)
{
    NEXUS_CecSettings cecSettings;
    NEXUS_Error rc;

    NEXUS_Cec_GetSettings(mCecHandle, &cecSettings);
    cecSettings.logicalAddress = addr;
    rc = NEXUS_Cec_SetSettings(mCecHandle, &cecSettings);
    if (!rc) {
       mCecLogicalAddr = addr;
       if (mCecLogicalAddr != UNDEFINED_LOGICAL_ADDRESS && mCecViewOnCmdPending) {
          if (setPowerState(ePowerState_S0)) {
             mCecViewOnCmdPending = false;
          }
       }
    }
    return NO_ERROR;
}

bool NexusHdmiCecDevice::setPowerState(nxwrap_pwr_state state)
{
   bool result = false;
   NEXUS_CecStatus cecStatus;
   cec_message_t txMessage;

   if (NEXUS_Cec_GetStatus(mCecHandle, &cecStatus) != NEXUS_SUCCESS) {
      goto out;
   }

   if (ePowerState_S0 == state) {
      txMessage.initiator = (cec_logical_address_t)mLogicalAddress;
      txMessage.destination = (cec_logical_address_t)0;
      txMessage.length = 1;
      txMessage.body[0] = NEXUS_CEC_OpImageViewOn;

      if (sendCecMessage(&txMessage, DEFAULT_MAX_CEC_RETRIES) == OK) {
         result = true;
      }
      if (result) {
         result = false;
         txMessage.initiator = (cec_logical_address_t)mLogicalAddress;
         txMessage.destination = (cec_logical_address_t)0xF;
         txMessage.length = 3;
         txMessage.body[0] = NEXUS_CEC_OpActiveSource;
         txMessage.body[1] = cecStatus.physicalAddress[0];
         txMessage.body[2] = cecStatus.physicalAddress[1];
         if (sendCecMessage(&txMessage, DEFAULT_MAX_CEC_RETRIES) == OK) {
            result = true;
         }
      }
   } else {
      txMessage.initiator = (cec_logical_address_t)mLogicalAddress;
      txMessage.destination = (cec_logical_address_t)0;
      txMessage.length = 1;
      txMessage.body[0] = NEXUS_CEC_OpStandby;
      if (sendCecMessage(&txMessage, DEFAULT_MAX_CEC_RETRIES) == OK) {
         result = true;
      }
   }
out:
   return result;
}

bool NexusHdmiCecDevice::getHdmiHotplugWakeup()
{
    static bool cached=false;
    static bool wakeup = false;

    if (!cached) {
        wakeup = property_get_bool(PROPERTY_HDMI_HOTPLUG_WAKEUP, DEFAULT_PROPERTY_HDMI_HOTPLUG_WAKEUP);
        cached = true;
    }
    return wakeup;
}

void NexusHdmiCecDevice::setEnableState(bool enable)
{
    ALOGV("%s: enable=%d", __PRETTY_FUNCTION__, enable);

    mCecEnable = nxcec_is_cec_enabled() && enable;
}

void NexusHdmiCecDevice::setAutoWakeupState(bool enable)
{
   nxcec_set_cec_autowake_enabled(enable);
}

void NexusHdmiCecDevice::setControlState(bool enable)
{
    ALOGV("%s: enable=%d", __PRETTY_FUNCTION__, enable);

    mCecSystemControlEnable = enable;

    if (mNxWrap != NULL) {
        bool tx;
        nxwrap_pwr_state state;

        if (enable) {
            state = ePowerState_S0;
            standbyLock();
            mStandby = false;
            standbyUnlock();
            tx = nxcec_get_cec_xmit_viewon();

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
            tx = nxcec_get_cec_xmit_stdby();
        }

        if (tx && setPowerState(state) != true) {
            ALOGE("%s: Could not set CEC PowerState %d!", __PRETTY_FUNCTION__, state);
        }
    }
}

status_t NexusHdmiCecDevice::getConnectedState(int *pIsConnected)
{
   NEXUS_HdmiOutputHandle hdmi;
   NEXUS_HdmiOutputStatus hstatus;
   hdmi = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
   NEXUS_HdmiOutput_GetStatus(hdmi, &hstatus);
   if (hstatus.connected) {
      *pIsConnected = HDMI_CONNECTED;
   } else {
      *pIsConnected = HDMI_NOT_CONNECTED;
   }
   return NO_ERROR;
}

void NexusHdmiCecDevice::registerEventCallback(const hdmi_cec_device_t* dev, event_callback_t callback, void *arg)
{
    mHdmiCecDevice = const_cast<hdmi_cec_device_t*>(dev);
    mCallback = callback;
    mCallbackArg = arg;
}

void NexusHdmiCecDevice::fireEventCallback(hdmi_event_t *pHdmiCecEvent)
{
    if (mCallback != NULL) {
        (*mCallback)(pHdmiCecEvent, mCallbackArg);
    }
}

void NexusHdmiCecDevice::fireHotplugCallback(int connected)
{
    hdmi_event_t hdmiCecEvent;

    hdmiCecEvent.type = HDMI_EVENT_HOT_PLUG;
    hdmiCecEvent.dev = mHdmiCecDevice;
    hdmiCecEvent.hotplug.port_id = 0;
    hdmiCecEvent.hotplug.connected = connected;
    mHotplugConnected = connected;

    fireEventCallback(&hdmiCecEvent);
}

status_t NexusHdmiCecDevice::getCecPhysicalAddress(uint16_t* addr)
{
   status_t ret = NO_ERROR;
   NEXUS_CecStatus cecStatus;
   if (NEXUS_Cec_GetStatus(mCecHandle, &cecStatus) != NEXUS_SUCCESS) {
      ret = UNKNOWN_ERROR;
      goto out;
   }
   mCecPhysicalAddr = (cecStatus.physicalAddress[0] * 256) + cecStatus.physicalAddress[1];
   *addr = mCecPhysicalAddr;
out:
   return ret;
}

status_t NexusHdmiCecDevice::getCecVersion(int* version)
{
   status_t ret = NO_ERROR;
   NEXUS_CecStatus cecStatus;
   if (NEXUS_Cec_GetStatus(mCecHandle, &cecStatus) != NEXUS_SUCCESS) {
      ret = UNKNOWN_ERROR;
      goto out;
   }
   *version = cecStatus.cecVersion;
out:
   return ret;
}

status_t NexusHdmiCecDevice::getCecVendorId(uint32_t* vendor_id)
{
   if (mCecVendorId == UNKNOWN_VENDOR_ID) {
       mCecVendorId = property_get_int32(PROPERTY_HDMI_CEC_VENDOR_ID, DEFAULT_PROPERTY_HDMI_CEC_VENDOR_ID);
   }
   *vendor_id = mCecVendorId;
   return NO_ERROR;
}

status_t NexusHdmiCecDevice::sendCecMessage(const cec_message_t *message, uint8_t maxRetries)
{
   status_t err = NO_ERROR;
   (void)message;
   (void)maxRetries;
#if 0
    if (mCecEnable) {
        if (pIpcClient == NULL) {
            ALOGE("%s: NexusIPCClient has been instantiated!!!", __PRETTY_FUNCTION__);
            return NO_INIT;
        }

        uint8_t srcAddr = (uint8_t)message->initiator;
        uint8_t destAddr = (uint8_t)message->destination;

        if (pIpcClient->sendCecMessage(0, srcAddr, destAddr, message->length, const_cast<uint8_t *>(message->body), maxRetries) != true) {
            ALOGE("%s: cannot send CEC message!!!", __PRETTY_FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }
#endif
    return err;
}

status_t NexusHdmiCecDevice::getCecPortInfo(struct hdmi_port_info* list[], int* total)
{
    status_t ret;
    bool cecEnabled;

    cecEnabled = nxcec_is_cec_enabled() && mCecEnable;

    mPortInfo.type = HDMI_OUTPUT;
    mPortInfo.port_id = 1;
    mPortInfo.cec_supported = cecEnabled;
    mPortInfo.arc_supported = false;
    ret = getCecPhysicalAddress(&mPortInfo.physical_address);

    list[0] = &mPortInfo;
    *total = 1;
    return ret;
}

