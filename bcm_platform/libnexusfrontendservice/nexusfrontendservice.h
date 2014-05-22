/******************************************************************************
 *    (c)2011 Broadcom Corporation
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
 * $brcm_Workfile: nexusservice.h $
 * $brcm_Revision: 3 $
 * $brcm_Date: 9/19/11 5:23p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/thirdparty/google/honeycomb/src/broadcom/honeycomb/vendor/broadcom/bcm_platform/libnexusservice/nexusservice.h $
 * 
 * 3   9/19/11 5:23p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 2   8/25/11 7:30p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 *****************************************************************************/
#ifndef _NEXUSFRONTENDSERVICE_H_
#define _NEXUSFRONTENDSERVICE_H_

#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/String16.h>
#include <utils/threads.h>
#include <string.h>
#include <signal.h>
#include <utils/Log.h>
#include "nexus_message.h"
#include "nexus_frontendinterface.h"

typedef struct Service_info
{
    Nexus_Channel_Service_info info;
    Service_info* next;
} Service_info;

typedef struct Msg_info_handle
{
    unsigned int msgid;
    unsigned int pidchannelid;
    pid_t     processid;/*for signal notify,get from the client*/
}Msg_info_handle;

class INexusFrontendService: public android::IInterface {
    public:
        android_DECLARE_META_FRONTENDINTERFACE(NexusFrontendService)
            void hellothere(const char *str);
};

class BnNexusFrontendService : public android::BnInterface<INexusFrontendService>
{

};

class NexusFrontendService : public BnNexusFrontendService
{
    public:
        static  void                instantiate();

        NexusFrontendService();
        virtual                 ~NexusFrontendService();

        android::status_t onTransact(uint32_t code,
                const android::Parcel &data,
                android::Parcel *reply,
                uint32_t flags);

        mutable     android::Mutex                       mLock;
        //android::SortedVector< android::wp<Client> >     mClients;

    private:
        Service_info* service_list;
        int           service_totalnum;
        Frontend_status  frontendstatus;
        FrontendParam    frontendparam;
        ServiceFrontend  fullbandfrontend[3];

        int tune_signal(int type,int freq,int s_rate,int mode,ServiceFrontend* retfrontend);
        int tune_status(int32_t* frontend , Frontend_status* status);

        int get_section(NEXUS_MessageHandle msg, NEXUS_PidChannelHandle pid,unsigned char *mask,
                unsigned char* value,unsigned char* excl,int len,unsigned char* sec);
        Service_info* get_service_list(int inputband);
        Service_info* get_one_service(unsigned char* buf);
        void services_free(Service_info *p);
        Service_info* get_one_service_bynum(int num);
        int get_totalservicenum();
        Msg_info_handle* dvbmessage_open(int inputband,unsigned short pid ,unsigned char* mask, unsigned char* value, unsigned char* excl, int length,pid_t p);
        int dvbmessge_get( Msg_info_handle* msghandle, unsigned char* msgbuff, unsigned int bufsize);
        void dvbmessage_close(Msg_info_handle* msghandle);
        void dvbca_play(unsigned short ecmpid,unsigned short emmpid,unsigned short vpid, unsigned short apid,NEXUS_PidChannelHandle vpidChannel,NEXUS_PidChannelHandle apidChannel);
};

class BpNexusFrontendClient: public android::BpInterface<INexusFrontendClient>
{
    public:
        BpNexusFrontendClient(const android::sp<android::IBinder>& impl)
            : android::BpInterface<INexusFrontendClient>(impl)
        {
        }

        void NexusFrontendParamterSet(void* pParam);
        void NexusFrontendParamterGet(void* pParam) ;
        int  NexusFrontendTune( ServiceFrontend* frontend );
        void NexusFrontendStatusGet(int32_t* frontend ,void* pStatus);
        void NexusDVBServiceSearch( int inputband);
        int NexusDVBServiceGetTotalNum( ) ;
        void NexusDVBServiceGetByNum(int num,void* pService ) ; 
        void NexusDVBServiceReset( ) ; 
        void NexusDVBServicePlay(unsigned int ecmpid,unsigned int emmpid,unsigned int videopid,unsigned int audiopid,unsigned int vpidchannel,unsigned int apidchannel) ;
        int32_t* NexusDVBMessageOpen(int inputband,void* pMessageParam) ;
        int NexusDVBMessageGetMessageData(int32_t* pMessageHandle,unsigned char *pData,int32_t iDataLength) ;
        void NexusDVBMessageClose(int32_t* pMessageHandle) ;
};

#endif
