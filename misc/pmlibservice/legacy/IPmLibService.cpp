/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
#define LOG_TAG "IPmLibService"
#include <utils/Log.h>

#include "legacy/IPmLibService.h"

using namespace android;

enum {
    GET_STATUS = IBinder::FIRST_CALL_TRANSACTION,
    SET_STATUS
};

class BpPmLibService : public BpInterface<IPmLibService>
{
public:
    BpPmLibService(const sp<IBinder>& impl) : BpInterface<IPmLibService>(impl) { }

    virtual status_t setState(pmlib_state_t *state);
    virtual status_t getState(pmlib_state_t *state);
};

status_t BpPmLibService::setState(pmlib_state_t *state)
{
    Parcel data, reply;

    data.writeInterfaceToken(IPmLibService::getInterfaceDescriptor());
    data.write(state, sizeof(*state));
    return remote()->transact(SET_STATUS, data, &reply);
}

status_t BpPmLibService::getState(pmlib_state_t *state)
{
    Parcel data, reply;

    data.writeInterfaceToken(IPmLibService::getInterfaceDescriptor());
    remote()->transact(GET_STATUS, data, &reply);
    return reply.read(state, sizeof(*state));
}

IMPLEMENT_META_INTERFACE(PmLibService, "android.media.IPmLibService");

status_t BnPmLibService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    status_t ret;

    switch (code) {
        case GET_STATUS: {
            pmlib_state_t state;
            CHECK_INTERFACE(IPmLibService, data, reply);
            ret = getState(&state);
            if (ret == OK) {
                ret = reply->write(&state, sizeof(state));
            }
        } break;

        case SET_STATUS: {
            pmlib_state_t state;
            CHECK_INTERFACE(IPmLibService, data, reply);
            ret = data.read(&state, sizeof(state));
            if (ret == OK) {
                ret = setState(&state);
            }
        } break;

        default:
            ret = BBinder::onTransact(code, data, reply, flags);
    }
    return ret;
}
