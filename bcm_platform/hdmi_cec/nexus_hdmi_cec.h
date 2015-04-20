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
 * $brcm_Workfile: nexus_hdmi_cec.h
 * Module Description:
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

#include "nexus_ipc_client_factory.h"

using namespace android;

enum cec_ui_command_type {
    CEC_UI_COMMAND_POWER = 0x40,
    CEC_UI_COMMAND_POWER_TOGGLE = 0x6B,
    CEC_UI_COMMAND_POWER_ON = 0x6D
};

// ----------------------------------------------------------------------------

class NexusHdmiCecDevice : public RefBase
{
    public:
        static sp<NexusHdmiCecDevice> instantiate(int cecId=0) { return new NexusHdmiCecDevice(cecId); }
        ~NexusHdmiCecDevice();

        static cec_logical_address_t toCecLogicalAddressT(uint8_t addr);
        static bool standbyMonitor(void *ctx);

        void registerEventCallback(const hdmi_cec_device_t* dev, event_callback_t callback, void *arg);
        void fireEventCallback(hdmi_event_t *pHdmiCecEvent);
        void setEnableState(bool enable);
        void setAutoWakeupState(bool enable);
        void setControlState(bool enable);
        bool getCecTransmitStandby();
        bool getCecTransmitViewOn();

        status_t initialise();
        status_t uninitialise();
        status_t getConnectedState(int *pIsConnected);
        status_t setCecLogicalAddress(uint8_t addr);
        status_t getCecPhysicalAddress(uint16_t* addr);
        status_t getCecVersion(int* version);
        status_t getCecVendorId(uint32_t* vendor_id);
        status_t sendCecMessage(const cec_message_t*);
        status_t getCecPortInfo(struct hdmi_port_info* list[], int* total);

        inline void standbyLock() { mStandbyLock.lock(); }
        inline void standbyUnlock() { mStandbyLock.unlock(); }

    protected:
        // Protected constructor prevents a client from creating an instance of this
        // class directly, but allows a sub-class to call it through inheritence.
        NexusHdmiCecDevice(int cecId);

        class HdmiCecMessageEventListener : public BnNexusHdmiCecMessageEventListener {
            public:
                static sp<HdmiCecMessageEventListener> instantiate(sp<NexusHdmiCecDevice> device) { return new HdmiCecMessageEventListener(device); }
                ~HdmiCecMessageEventListener();
                virtual status_t onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message);
            protected:
                // Protected constructor prevents a client from creating an instance of this
                // class directly, but allows a sub-class to call it through inheritence.
                HdmiCecMessageEventListener(sp<NexusHdmiCecDevice> device);
            private:
                sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;

                /* Disallow copy constructor and copy operator... */
                HdmiCecMessageEventListener(const HdmiCecMessageEventListener &);
                HdmiCecMessageEventListener &operator=(const HdmiCecMessageEventListener &);
        };

        class HdmiHotplugEventListener : public BnNexusHdmiHotplugEventListener {
            public:
                static sp<HdmiHotplugEventListener> instantiate(sp<NexusHdmiCecDevice> device) { return new HdmiHotplugEventListener(device); }
                ~HdmiHotplugEventListener();
                virtual status_t onHdmiHotplugEventReceived(int32_t portId, bool connected);
            protected:
                // Protected constructor prevents a client from creating an instance of this
                // class directly, but allows a sub-class to call it through inheritence.
                HdmiHotplugEventListener(sp<NexusHdmiCecDevice> device);
            private:
                sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;

                /* Disallow copy constructor and copy operator... */
                HdmiHotplugEventListener(const HdmiHotplugEventListener &);
                HdmiHotplugEventListener &operator=(const HdmiHotplugEventListener &);
        };

        class HdmiCecRxMessageHandler : public AHandler {
            public:
                static sp<HdmiCecRxMessageHandler> instantiate(sp<NexusHdmiCecDevice> device) { return new HdmiCecRxMessageHandler(device); }
                ~HdmiCecRxMessageHandler();

                enum {
                    kWhatHandleMsg =  0x01,
                };
            protected:
                // Protected constructor prevents a client from creating an instance of this
                // class directly, but allows a sub-class to call it through inheritence.
                HdmiCecRxMessageHandler(sp<NexusHdmiCecDevice> device);
            private:
                sp<NexusHdmiCecDevice> mNexusHdmiCecDevice;

                virtual void onMessageReceived(const sp<AMessage> &msg);
                virtual bool isValidWakeupCecMessage(cec_message_t *message);
                virtual status_t handleCecMessage(const sp<AMessage> &msg);

                /* Disallow copy constructor and copy operator... */
                HdmiCecRxMessageHandler(const HdmiCecRxMessageHandler &);
                HdmiCecRxMessageHandler &operator=(const HdmiCecRxMessageHandler &);
        };

    private:
        int                             mCecId;
        uint8_t                         mCecLogicalAddr;
        uint16_t                        mCecPhysicalAddr;
        bool                            mCecEnable;
        bool                            mCecSystemControlEnable;
        bool                            mCecViewOnCmdPending;
        bool                            mStandby;
        bool                            mHotPlugConnected;
        Mutex                           mStandbyLock;
        NexusIPCClientBase*             pIpcClient;
        NexusClientContext*             pNexusClientContext;
        hdmi_port_info                  mPortInfo[NEXUS_NUM_CEC];
        event_callback_t                mCallback;
        void*                           mCallbackArg;
        hdmi_cec_device_t*              mHdmiCecDevice;
        sp<HdmiCecMessageEventListener> mHdmiCecMessageEventListener;
        sp<HdmiHotplugEventListener>    mHdmiHotplugEventListener;
        sp<HdmiCecRxMessageHandler>     mHdmiCecRxMessageHandler;
        sp<ALooper>                     mHdmiCecRxMessageLooper;

        static const uint32_t UNDEFINED_PHYSICAL_ADDRESS = 0xFFFF;
        static const uint32_t UNDEFINED_LOGICAL_ADDRESS = 0xFF;
        static const uint8_t  MAX_LOGICAL_ADDRESS = 0x0F;

        /* Disallow copy constructor and copy operator... */
        NexusHdmiCecDevice(const NexusHdmiCecDevice &);
        NexusHdmiCecDevice &operator=(const NexusHdmiCecDevice &);
};

#endif  // _NEXUS_HDMI_CEC_H_

