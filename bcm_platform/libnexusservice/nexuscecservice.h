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
#ifndef _NEXUSCECSERVICE_H_
#define _NEXUSCECSERVICE_H_

#include "nexusservice.h"

#include <utils/RefBase.h>

using namespace android;

struct NexusService::CecServiceManager : public RefBase
{
public:
    friend class NexusService;
    virtual      ~CecServiceManager();
    virtual status_t platformInit();
    virtual void platformUninit();
    virtual bool isPlatformInitialised();
    virtual bool getHdmiStatus(NEXUS_HdmiOutputStatus *pStatus);
    virtual status_t sendCecMessage(uint8_t destAddr, size_t length, uint8_t *pBuffer);
    virtual bool setPowerState(b_powerState pmState);
    virtual bool getPowerStatus(uint8_t *pPowerStatus);

protected:
    struct CecRxMessageHandler;
    struct CecTxMessageHandler;

    uint32_t                            cecId;
    NEXUS_CecHandle                     cecHandle;

    // Protected constructor prevents a client from creating an instance of this
    // class directly, but allows a sub-class to call it through inheritence.
    CecServiceManager(uint32_t cecId = 0) : cecId(cecId), cecHandle(NULL), mCecRxMessageHandler(NULL), mCecTxMessageHandler(NULL) {
        ALOGV("%s: called for CEC%d", __PRETTY_FUNCTION__, cecId);
    }

    static sp<CecServiceManager> instantiate(uint32_t cecId) { return new CecServiceManager(cecId); }

private:
    uint8_t                             cecPowerStatus;
    Mutex                               mCecDeviceReadyLock;
    Condition                           mCecDeviceReadyCondition;
    Mutex                               mCecGetPowerStatusLock;
    Condition                           mCecGetPowerStatusCondition;
    sp<ALooper>                         mCecRxMessageLooper;
    sp<CecRxMessageHandler>             mCecRxMessageHandler;
    sp<ALooper>                         mCecTxMessageLooper;
    sp<CecTxMessageHandler>             mCecTxMessageHandler;

    static void deviceReady_callback(void *context, int param);
    static void msgReceived_callback(void *context, int param);
    static void msgTransmitted_callback(void *context, int param);

    /* Disallow copy constructor and copy operator... */
    CecServiceManager(const CecServiceManager &);
    CecServiceManager &operator=(const CecServiceManager &);
};

struct NexusService::CecServiceManager::CecRxMessageHandler : public AHandler
{
public:
    friend class NexusService::CecServiceManager;

    virtual      ~CecRxMessageHandler();

protected:
    uint32_t cecId;

    // Protected constructor prevents a client from creating an instance of this
    // class directly, but allows a sub-class to call it through inheritence.
    CecRxMessageHandler(sp<NexusService::CecServiceManager> sm, uint32_t cecId) : AHandler(), cecId(cecId), mCecServiceManager(sm) {
        ALOGV("%s: called for CEC%d", __PRETTY_FUNCTION__, cecId);
    }
    
    virtual void getPhysicalAddress( unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void getDeviceType(unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void getCecVersion(unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void getOsdName(unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void getDeviceVendorID(unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void enterStandby(unsigned inLength, uint8_t *content, unsigned *outLength );
    virtual void reportPowerStatus(unsigned inLength, uint8_t *content, unsigned *outLength );
    static sp<CecRxMessageHandler> instantiate(sp<NexusService::CecServiceManager> sm, uint32_t cecId) { return new CecRxMessageHandler(sm, cecId); }

private:
    enum {
        kWhatParse =  0x01,
    };

    static const unsigned numMaxParameters = 3;

    struct opCodeCommandList
    {
        uint8_t opCodeCommand;
        uint8_t opCodeResponse;
        void (NexusService::CecServiceManager::CecRxMessageHandler::*getParamFunc[numMaxParameters])(unsigned inLength, uint8_t *content, unsigned *outLength);
    };

    static const struct opCodeCommandList opCodeList[];

    sp<NexusService::CecServiceManager> mCecServiceManager;

    virtual void onMessageReceived(const sp<AMessage> &msg);
    virtual void responseLookUp( const uint8_t opCode, size_t length, uint8_t *pBuffer );
    virtual void parseCecMessage(const sp<AMessage> &msg);

    /* Disallow copy constructor and copy operator... */
    CecRxMessageHandler(const CecRxMessageHandler &);
    CecRxMessageHandler &operator=(const CecRxMessageHandler &);
}; 

struct NexusService::CecServiceManager::CecTxMessageHandler : public AHandler
{
public:
    friend class NexusService::CecServiceManager;

    enum {
        kWhatSend =  0x01,
    };

    virtual      ~CecTxMessageHandler();

    virtual void onMessageReceived(const sp<AMessage> &msg);

protected:
    uint32_t cecId;

    // Protected constructor prevents a client from creating an instance of this
    // class directly, but allows a sub-class to call it through inheritence.
    CecTxMessageHandler(sp<NexusService::CecServiceManager> sm, uint32_t cecId) : AHandler(), cecId(cecId), mCecServiceManager(sm) {
        ALOGV("%s: called for CEC%d", __PRETTY_FUNCTION__, cecId);
    }

    virtual status_t outputCecMessage(const sp<AMessage> &msg);
    static sp<CecTxMessageHandler> instantiate(sp<NexusService::CecServiceManager> sm, uint32_t cecId) { return new CecTxMessageHandler(sm, cecId); }

private:
    sp<NexusService::CecServiceManager> mCecServiceManager;

    /* Disallow copy constructor and copy operator... */
    CecTxMessageHandler(const CecTxMessageHandler &);
    CecTxMessageHandler &operator=(const CecTxMessageHandler &);
}; 

#endif // _NEXUSCECSERVICE_H_
