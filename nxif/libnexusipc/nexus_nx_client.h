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
 * Module Description:
 * This file contains the class that implements the NxClient bipc communication
 * with the nxserver server process and Binder IPC communication with the
 * Nexus Service.  The Nexus Service will then communicate using NxClient calls
 * to the nxserver process. 
 * A client process should NOT instantiate an object of this type directly.
 * Instead, they should use the "getClient()" method of the NexusIPCClientFactory
 * abstract factory class.
 * On the client side, the definition of these API functions simply encapsulate
 * the API into either a "command + param" format and sends the command over binder
 * to the Nexus Service for actual execution OR via bipc to the nxserver for
 * direct NxClient API's calls.
 * 
 *****************************************************************************/
#ifndef _NEXUS_NX_CLIENT_H_
#define _NEXUS_NX_CLIENT_H_

#ifdef __cplusplus
#include <utils/threads.h>
#include <utils/Errors.h>
#include "nexus_ipc_client.h"
#include "nxclient.h"

class NexusIPCClientFactory;

class NexusNxClient : public NexusIPCClient
{
public: 
    friend class NexusIPCClientFactory;
    virtual      ~NexusNxClient();

    struct StandbyMonitorThread : public android::Thread {

        enum ThreadState {
            STATE_UNKNOWN,
            STATE_STOPPED,
            STATE_RUNNING
        };

        StandbyMonitorThread(b_refsw_client_standby_monitor_callback callback, b_refsw_client_standby_monitor_context context);
        virtual ~StandbyMonitorThread();

        virtual android::status_t run( const char* name = 0,
                                       int32_t priority = android::PRIORITY_DEFAULT,
                                       size_t stack = 0);

        virtual void stop() { state = STATE_STOPPED; }

        bool isRunning() { return (state == STATE_RUNNING); }

        const char *getName() { return name; }

    private:
        ThreadState state;
        b_refsw_client_standby_monitor_callback mCallback;
        b_refsw_client_standby_monitor_context  mContext;
        char *name;
        unsigned mStandbyId;
        bool threadLoop();

        /* Disallow copy constructor and copy operator... */
        StandbyMonitorThread(const StandbyMonitorThread &);
        StandbyMonitorThread &operator=(const StandbyMonitorThread &);
    };

    /* These API's require a Nexus Client Context as they handle per client resources... */
    virtual uint64_t createClientContext(const b_refsw_client_client_configuration *config = NULL);
    virtual void destroyClientContext(uint64_t client);

    virtual bool setPowerState(b_powerState pmState);
    virtual bool getPowerStatus(b_powerStatus *pPowerStatus);
    virtual bool getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus);

protected:
    // Protected constructor prevents a client from creating an instance of this
    // class directly, but allows a sub-class to call it through inheritence.
                 NexusNxClient(const char *clientName = NULL);
    // This method is protected and can only be called by the NexusIPCClientFactory::getClient() abstract factory class
    static NexusIPCClientBase *getClient(const char *clientName=NULL) { return new NexusNxClient(clientName); }
    virtual NEXUS_Error clientJoin(const b_refsw_client_client_configuration *config);
    virtual NEXUS_Error clientUninit();

private:
    NEXUS_Error standbyCheck(NEXUS_PlatformStandbyMode mode);

    android::sp<NexusNxClient::StandbyMonitorThread> mStandbyMonitorThread;
    b_refsw_client_client_info info;
};


#endif /* __cplusplus */
#endif /* _NEXUS_NX_CLIENT_H_ */
