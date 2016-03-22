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
#undef LOG_TAG
#define LOG_TAG "INexusHdmiCecMessageEventListener"
#include <utils/Log.h>

#include "INexusHdmiCecMessageEventListener.h"

using namespace android;

enum {
    ON_HDMI_CEC_MESSAGE_RECEIVED = IBinder::FIRST_CALL_TRANSACTION,
};

class BpNexusHdmiCecMessageEventListener : public BpInterface<INexusHdmiCecMessageEventListener>
{
public:
    BpNexusHdmiCecMessageEventListener(const sp<IBinder>& impl) : BpInterface<INexusHdmiCecMessageEventListener>(impl) { }

    /* HDMI related events... */
    virtual status_t onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message);
};

/* The process that receives HDMI CEC events from Nexus should be responsible for calling this function
   so that other "client" process(es) can receive this event. */
status_t BpNexusHdmiCecMessageEventListener::onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    Parcel data, reply;

    data.writeInterfaceToken(INexusHdmiCecMessageEventListener::getInterfaceDescriptor());
    data.writeInt32(portId);
    data.write(message, sizeof(*message));
    return remote()->transact(ON_HDMI_CEC_MESSAGE_RECEIVED, data, &reply);
}

IMPLEMENT_META_INTERFACE(NexusHdmiCecMessageEventListener, "com.broadcom.bse.nexusHdmiCecMessageEventListenerInterface");

status_t BnNexusHdmiCecMessageEventListener::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    status_t ret;

    switch (code) {
        case ON_HDMI_CEC_MESSAGE_RECEIVED: {
            int32_t portId;
            INexusHdmiCecMessageEventListener::hdmiCecMessage_t message;
            CHECK_INTERFACE(INexusHdmiCecMessageEventListener, data, reply);
            portId = data.readInt32();
            ret = data.read(&message, sizeof(message));
            if (ret == OK) {
                ret = onHdmiCecMessageReceived(portId, &message);
            }
        } break;

        default:
            ret = BBinder::onTransact(code, data, reply, flags);
    }
    return ret;
}
