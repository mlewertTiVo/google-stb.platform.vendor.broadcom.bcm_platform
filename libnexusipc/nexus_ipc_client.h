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
 * $brcm_Workfile: nexus_ipc_client.h $
 * 
 * Module Description:
 * This file contains the class that implements the binder IPC communication
 * with the Nexus Service.  A client process should NOT instantiate an object
 * of this type directly.  Instead, they should use the "getClient()" method
 * of the NexusIPCClientFactory abstract factory class.
 * On the client side, the definition of these API functions simply encapsulate
 * the API into a command + param format and sends the command over binder to
 * the server side for actual execution.
 * 
 *****************************************************************************/
#ifndef _NEXUS_IPC_CLIENT_H_
#define _NEXUS_IPC_CLIENT_H_

#ifdef __cplusplus
#include "nexus_ipc_client_base.h"

class NexusIPCClientFactory;

class NexusIPCClient : public NexusIPCClientBase
{
public:
    friend class NexusIPCClientFactory;
    virtual      ~NexusIPCClient();

    /* These API's require a Nexus Client Context as they handle per client resources... */
    virtual NexusClientContext * createClientContext(const b_refsw_client_client_configuration *config = NULL);
    virtual void destroyClientContext(NexusClientContext * client);

    /* These API's do NOT require a Nexus Client Context as they handle global resources...*/
    virtual status_t setHdmiCecMessageEventListener(uint32_t cecId, const sp<INexusHdmiCecMessageEventListener> &listener);
    virtual status_t addHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);
    virtual status_t removeHdmiHotplugEventListener(uint32_t portId, const sp<INexusHdmiHotplugEventListener> &listener);
    virtual status_t addDisplaySettingsChangedEventListener(uint32_t portId, const sp<INexusDisplaySettingsChangedEventListener> &listener);
    virtual status_t removeDisplaySettingsChangedEventListener(uint32_t portId, const sp<INexusDisplaySettingsChangedEventListener> &listener);

    virtual bool setPowerState(b_powerState pmState);
    virtual bool getPowerStatus(b_powerStatus *pPowerStatus);
    virtual bool setCecPowerState(uint32_t cecId, b_powerState pmState);
    virtual bool getCecPowerStatus(uint32_t cecId, uint8_t *pPowerStatus);
    virtual bool getCecStatus(uint32_t cecId, b_cecStatus *pCecStatus);
    virtual bool sendCecMessage(uint32_t cecId, uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pMessage, uint8_t maxRetries);
    virtual bool isCecEnabled(uint32_t cecId);
    virtual bool setCecAutoWakeupEnabled(uint32_t cecId, bool enabled);
    virtual bool isCecAutoWakeupEnabled(uint32_t cecId);
    virtual b_cecDeviceType getCecDeviceType(uint32_t cecId);
    virtual bool setCecLogicalAddress(uint32_t cecId, uint8_t addr);
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus);

protected:
    // Protected constructor prevents a client from creating an instance of this
    // class directly, but allows a sub-class to call it through inheritence.
                 NexusIPCClient(const char *clientName = NULL);
    // This method is protected and can only be called by the NexusIPCClientFactory::getClient() abstract factory class
    static NexusIPCClientBase *getClient(const char *clientName=NULL) { return new NexusIPCClient(clientName); }
    virtual NEXUS_Error clientJoin(const b_refsw_client_client_configuration *config);
    virtual NEXUS_Error clientUninit();
    virtual void bindNexusService();
    virtual void unbindNexusService();
    static unsigned mBindRefCount;
    //
    // This is the interface to access our internal server.
    //
    static android::sp<INexusClient> iNC;

private:
    static b_cecDeviceType toCecDeviceType(char *string);

    static NEXUS_ClientHandle clientHandle;
};

#endif /* __cplusplus */
#endif /* _NEXUS_IPC_CLIENT_H_ */
