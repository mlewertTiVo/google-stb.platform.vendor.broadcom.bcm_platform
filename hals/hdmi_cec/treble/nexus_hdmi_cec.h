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
#ifndef _NEXUS_HDMI_CEC_H_
#define _NEXUS_HDMI_CEC_H_

#include <stdint.h>
#include <sys/types.h>

#include "bstd.h"
#include "berr.h"
#include "nexus_platform.h"
#include "nexus_types.h"
#include "nexus_platform.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

#include <hardware/hdmi_cec.h>
#include <utils/SortedVector.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include <nexus_cec.h>
#include "linuxuinput.h"
#include <nxcec.h>
#include <nxwrap.h>

using namespace android;

#define DEFAULT_MAX_CEC_RETRIES 5

enum cec_ui_command_type {
    CEC_UI_COMMAND_POWER = 0x40,
    CEC_UI_COMMAND_POWER_TOGGLE = 0x6B,
    CEC_UI_COMMAND_POWER_ON = 0x6D
};

// ----------------------------------------------------------------------------

class NexusHdmiCecDevice : public RefBase
{
    public:
        static sp<NexusHdmiCecDevice> instantiate() { return new NexusHdmiCecDevice(); }
        ~NexusHdmiCecDevice();

        void registerEventCallback(const hdmi_cec_device_t* dev, event_callback_t callback, void *arg);
        void setEnableState(bool enable);
        void setAutoWakeupState(bool enable);
        void setControlState(bool enable);

        status_t initialise();
        status_t uninitialise();
        status_t getConnectedState(int *pIsConnected);
        status_t setCecLogicalAddress(uint8_t addr);
        status_t getCecPhysicalAddress(uint16_t* addr);
        status_t getCecVersion(int* version);
        status_t getCecVendorId(uint32_t* vendor_id);
        status_t sendCecMessage(const cec_message_t*, uint8_t maxRetries=NexusHdmiCecDevice::DEFAULT_CEC_RETRIES);
        status_t getCecPortInfo(struct hdmi_port_info* list[], int* total);
        status_t viewOn();
        status_t standBy();
        void     onHpd(bool connected);

    protected:
        NexusHdmiCecDevice();

        class HdmiCecRxMessageHandler : public AHandler {
            public:
                static sp<HdmiCecRxMessageHandler> instantiate(sp<NexusHdmiCecDevice> device) { return new HdmiCecRxMessageHandler(device); }
                ~HdmiCecRxMessageHandler();

                enum {
                    kWhatHandleMsg =  0x01,
                };
            protected:
                HdmiCecRxMessageHandler(sp<NexusHdmiCecDevice> device);
            private:
                sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;

                virtual void onMessageReceived(const sp<AMessage> &msg);
                virtual bool isValidWakeupCecMessage(cec_message_t *message);
                virtual status_t handleCecMessage(const sp<AMessage> &msg);
                virtual status_t processCecMessage(cec_message_t *message);

                HdmiCecRxMessageHandler(const HdmiCecRxMessageHandler &);
                HdmiCecRxMessageHandler &operator=(const HdmiCecRxMessageHandler &);
        };

        class LinuxUInputRef : public LinuxUInput, public android::RefBase {
            public:
                static sp<LinuxUInputRef> instantiate();
                ~LinuxUInputRef();

            private:
                LinuxUInputRef();
        };

        class HdmiCecTxMessageHandler : public AHandler {
            public:
                static sp<HdmiCecTxMessageHandler> instantiate(sp<NexusHdmiCecDevice> device) { return new HdmiCecTxMessageHandler(device); }
                ~HdmiCecTxMessageHandler();

                enum {
                    kWhatSend =  0x01,
                };
            protected:
                HdmiCecTxMessageHandler(sp<NexusHdmiCecDevice> device);
            private:
                sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;

                virtual void onMessageReceived(const sp<AMessage> &msg);
                virtual status_t outputCecMessage(const sp<AMessage> &msg);

                HdmiCecTxMessageHandler(const HdmiCecTxMessageHandler &);
                HdmiCecTxMessageHandler &operator=(const HdmiCecTxMessageHandler &);
        };

    private:
        uint8_t                         mCecLogicalAddr;
        uint16_t                        mCecPhysicalAddr;
        uint32_t                        mCecVendorId;
        bool                            mCecEnable;
        bool                            mCecSystemControlEnable;
        bool                            mCecViewOnCmdPending;
        bool                            mStandby;
        int                             mHotplugConnected;
        Mutex                           mStandbyLock;
        NxWrap                          *mNxWrap;
        hdmi_port_info                  mPortInfo;
        event_callback_t                mCallback;
        void*                           mCallbackArg;
        hdmi_cec_device_t*              mHdmiCecDevice;
        sp<HdmiCecRxMessageHandler>     mHdmiCecRxMessageHandler;
        sp<ALooper>                     mHdmiCecRxMessageLooper;
        sp<LinuxUInputRef>              mUInput;
        sp<HdmiCecTxMessageHandler>     mHdmiCecTxMessageHandler;
        sp<ALooper>                     mHdmiCecTxMessageLooper;
        NEXUS_CecHandle                 mCecHandle;
        NEXUS_HdmiOutputHandle          mHdmiHandle;
        uint8_t                         mLogicalAddress;
        Mutex                           mCecMessageTransmittedLock;
        Condition                       mCecMessageTransmittedCondition;
        Mutex                           mCecDeviceReadyLock;
        Condition                       mCecDeviceReadyCondition;

        static const uint32_t UNDEFINED_PHYSICAL_ADDRESS = 0xFFFF;
        static const uint32_t UNDEFINED_LOGICAL_ADDRESS = 0xFF;
        static const uint8_t  MAX_LOGICAL_ADDRESS = 0x0F;
        static const uint8_t  DEFAULT_CEC_RETRIES = 0;
        static const uint32_t UNKNOWN_VENDOR_ID = 0x0;

        static cec_logical_address_t toCecLogicalAddressT(uint8_t addr);
        static bool standbyMonitor(void *ctx);

        void fireEventCallback(hdmi_event_t *pHdmiCecEvent);
        void fireHotplugCallback(int connected);
        bool getHdmiHotplugWakeup();
        bool setPowerState(nxwrap_pwr_state state);

        inline void standbyLock() { mStandbyLock.lock(); }
        inline void standbyUnlock() { mStandbyLock.unlock(); }

        static void cecMsgReceivedCb(void *context, int param);
        static void cecMsgTransmittedCb(void *context, int param);
        static void cecDeviceReadyCb(void *context, int param);

        NexusHdmiCecDevice(const NexusHdmiCecDevice &);
        NexusHdmiCecDevice &operator=(const NexusHdmiCecDevice &);
};

#endif  // _NEXUS_HDMI_CEC_H_

