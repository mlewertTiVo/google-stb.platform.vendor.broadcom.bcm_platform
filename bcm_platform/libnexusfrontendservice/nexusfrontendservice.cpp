/******************************************************************************
 *    (c)2010-2011 Broadcom Corporation
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
 * $brcm_Workfile: nexusservice.cpp $
 * $brcm_Revision: 3 $
 * $brcm_Date: 9/19/11 5:23p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/thirdparty/google/honeycomb/src/broadcom/honeycomb/vendor/broadcom/bcm_platform/libnexusservice/nexusservice.cpp $
 * 
 * 3   9/19/11 5:23p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 2   8/25/11 7:30p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 
 *****************************************************************************/

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include "nexus_frontend.h"
#include "nexus_platform.h"
#include "nexus_parser_band.h"
#include "nexus_message.h"
#include "nexus_pid_channel.h"
#include "bkni.h"
#include "nexus_frontendinterface.h"
#include "nexusfrontendservice.h"

#define LOG_TAG "NexusFrontendService"

#define MAXMSGBUFSIZE 4096

const android::String16 INexusFrontendService::descriptor(NEXUS_FRONTEND_INTERFACE_NAME);

android::String16 INexusFrontendService::getInterfaceDescriptor() {
    return INexusFrontendService::descriptor;
}


void NexusFrontendService::instantiate() {

    LOGI("NexusFrontendService add");
    android::defaultServiceManager()->addService(
            INexusFrontendService::descriptor, new NexusFrontendService());
}

NexusFrontendService::NexusFrontendService()
{
    int returnvalue;
    LOGI("NexusFrontendService Created");

    service_totalnum=0;
    service_list = NULL;
    frontendparam.freq = 379;
    frontendparam.symbolrate = 6875;
    frontendparam.qammode = Qam64;

    memset(&fullbandfrontend,0,sizeof(ServiceFrontend)*3);

    fullbandfrontend[0].inputband=0xFF;
    fullbandfrontend[1].inputband=0xFF;
    fullbandfrontend[2].inputband=0xFF;

    fullbandfrontend[0].nexusfrontendnum=0xFF;
    fullbandfrontend[1].nexusfrontendnum=0xFF;
    fullbandfrontend[2].nexusfrontendnum=0xFF;         
}

NexusFrontendService::~NexusFrontendService()
{
    LOGI("NexusFrontendService Destroyed");

    if(service_list!=NULL)
    {
        services_free(service_list);

    }
    service_totalnum=0;    	   	    
}

#ifdef CHECK_INTERFACE
#undef CHECK_INTERFACE
#endif
#ifndef CHECK_INTERFACE
#define CHECK_INTERFACE(interface, data, reply) \
    do { if (!data.enforceInterface(interface::getInterfaceDescriptor())) { \
        LOGW("Call incorrectly routed to " #interface); \
        return android::PERMISSION_DENIED; \
    } } while (0) 

#endif
android::status_t NexusFrontendService::onTransact(uint32_t code,
        const android::Parcel &data,
        android::Parcel *reply,
        uint32_t flags)
{
    LOGI("OnTransact(%u,%u)", code, flags);
    CHECK_INTERFACE(INexusFrontendService, data, reply);

    switch(code) {
        case NEXUS_FRONTEND_TUNE_PARAMETER_SET: {
                                                    unsigned char qam;
                                                    LOGI("NexusFrontendService OnTransact(%u)", code);        	
                                                    frontendparam.freq       = data.readInt32();
                                                    frontendparam.symbolrate = data.readInt32();
                                                    data.read((void*)&qam,sizeof(unsigned char));

                                                    frontendparam.qammode    =(FrontendQam) qam;

                                                    return android::NO_ERROR;
                                                } break;

        case NEXUS_FRONTEND_TUNE_PARAMETER_GET: {
                                                    LOGI("NexusFrontendService OnTransact(%u)", code);
                                                    reply->writeInt32(frontendparam.freq);
                                                    reply->writeInt32(frontendparam.symbolrate);
                                                    reply->write(&frontendparam.qammode,sizeof(unsigned char) );

                                                    return android::NO_ERROR;
                                                } break;

        case NEXUS_FRONTEND_TUNE_TUNE: {

                                           ServiceFrontend frontend;
                                           int ret = -1;

                                           LOGI("NexusFrontendService OnTransact(%u)", code);   
                                           LOGI("NexusFrontendService Tune Tune with param freq=%d,symbol=%d, qam=%d", frontendparam.freq,frontendparam.symbolrate,(int)frontendparam.qammode);     	
                                           ret=tune_signal( 0, frontendparam.freq,frontendparam.symbolrate,(int)frontendparam.qammode,&frontend );

                                           if(ret==0)
                                           {
                                               reply->write(&frontend,sizeof(ServiceFrontend));
                                           }

                                           return android::NO_ERROR;
                                       } break;

        case NEXUS_FRONTEND_TUNE_STATUS_GET: {
                                                 Frontend_status Status;

                                                 int32_t* pfrontend;
                                                 LOGI("NexusFrontendService OnTransact(%u)", code);			
                                                 LOGI("nexusfrontend get status");

                                                 pfrontend=(int32_t*) data.readIntPtr();
                                                 if(pfrontend!=NULL)
                                                 {
                                                     LOGI("call tune_status[%x]",pfrontend);
                                                     tune_status(pfrontend,&Status);

                                                     reply->write(&(Status.receiverLock),sizeof(unsigned char));
                                                     reply->write(&(Status.fecLock),sizeof(unsigned char));
                                                     reply->writeInt32(Status.berEstimate);
                                                     reply->writeInt32(Status.snrEstimate);
                                                     reply->writeInt32(Status.bitErrCorrected);
                                                     reply->writeInt32(Status.dsChannelPower);
                                                 }

                                                 return android::NO_ERROR;
                                             } break;

        case  NEXUS_DVB_SERVICE_PLAY:
                                             {
                                                 unsigned short EMMPid,ECMPid,VPid,APid;
                                                 unsigned int vhandle,ahandle;
                                                 ECMPid = data.readInt32();
                                                 EMMPid = data.readInt32();

                                                 VPid  = data.readInt32();
                                                 APid = data.readInt32();
                                                 vhandle=data.readInt32();
                                                 ahandle=data.readInt32();

                                                 LOGI("dvbca_play with ecm [%x] emm [%x], video [%x],audio [%x]",ECMPid,EMMPid,VPid,APid);
                                                 dvbca_play(ECMPid,EMMPid,VPid,APid,(NEXUS_PidChannelHandle)vhandle,(NEXUS_PidChannelHandle)ahandle);

                                                 return android::NO_ERROR;
                                             }
                                             break;

        case NEXUS_DVB_SERVICE_SEARCH_SERVICE:{
                                                  unsigned int inputband;
                                                  LOGI("NexusFrontendService OnTransact(%u)", code);
                                                  inputband = data.readInt32();
                                                  service_list=get_service_list(inputband);
                                              } break;

        case NEXUS_DVB_SERVICE_GET_TOTALNUM: {

                                                 LOGI("NexusFrontendService OnTransact(%u)", code);      

                                                 reply->writeInt32(get_totalservicenum());
                                                 return android::NO_ERROR;
                                             } break;

        case NEXUS_DVB_SERVICE_GET_BYNUM: {
                                              int servicenum;
                                              Service_info* pService=NULL;

                                              LOGI("NexusFrontendService OnTransact(%u)", code);			

                                              servicenum=data.readInt32();
                                              pService=	get_one_service_bynum(servicenum);

                                              if(pService!=NULL)
                                              {
                                                  LOGI("audio =%x,video=%x,pcr=%x",pService->info.a, pService->info.v,pService->info.p);
                                                  reply->write((void*)pService,sizeof(Service_info)); 
                                              }

                                              /*  LOGI("GET_HANDLE_NEXUS_WINDOW: 0x%x\n", hNexusVideoWindow[1]);		*/		
                                              return android::NO_ERROR;
                                          } break;

        case NEXUS_DVB_SERVICE_RESET: {               
                                          if(service_list!=NULL)
                                          {
                                              services_free(service_list);
                                              service_list = NULL;
                                          }
                                          memset(&fullbandfrontend,0,sizeof(ServiceFrontend)*3);

                                          fullbandfrontend[0].inputband=0xFF;
                                          fullbandfrontend[1].inputband=0xFF;
                                          fullbandfrontend[2].inputband=0xFF;

                                          fullbandfrontend[0].nexusfrontendnum=0xFF;
                                          fullbandfrontend[1].nexusfrontendnum=0xFF;
                                          fullbandfrontend[2].nexusfrontendnum=0xFF;   
                                          LOGI("NEXUS_DVB_SERVICE_RESET: \n");

                                          return android::NO_ERROR;
                                      } break;

        case NEXUS_DVB_MESSAGE_OPEN: {			 
                                         Msg_info_handle* pHandle;
                                         int inputband;
                                         unsigned short pid;
                                         pid_t p;
                                         unsigned char value[16];
                                         unsigned char mask[16];
                                         unsigned char excl[16];
                                         DvbmessageFilter Filter;
                                         LOGI("NexusFrontendService OnTransact(%u)", code);			 
                                         pHandle=NULL;

                                         memset(value,0xff,sizeof(unsigned char)*16);
                                         memset(mask,0xff,sizeof(unsigned char)*16);
                                         memset(excl,0xff,sizeof(unsigned char)*16);

                                         pid=data.readInt32();
                                         inputband=data.readInt32();	
                                         data.read((void*)&p,sizeof(pid_t));
                                         data.read((void*)&Filter,sizeof(DvbmessageFilter));	

                                         memcpy(value,Filter.value,sizeof(unsigned char)*16);
                                         memcpy(mask,Filter.mask,sizeof(unsigned char)*16);
                                         memcpy(excl,Filter.excl,sizeof(unsigned char)*16);

                                         LOGI("NexusFrontendService NEXUS_DVB_MESSAGE_OPEN pid=%x,value[%x %x %x %x %x %x],process id=%x", pid,value[0],value[1],value[2],value[3],value[4],value[5],p);

                                         pHandle=dvbmessage_open(inputband,pid,mask,value,excl,16,p);
                                         reply->writeIntPtr((intptr_t)pHandle); 

                                         return android::NO_ERROR;
                                     } break;

        case NEXUS_DVB_MESSAGE_GET_MESSAGE_DATA: {		
                                                     Msg_info_handle* pHandle;
                                                     unsigned char* pData=NULL;
                                                     int readLength,retLength;
                                                     LOGI("NexusFrontendService  NEXUS_DVB_MESSAGE_GET_MESSAGE_DATA OnTransact(%u)", code);	  	

                                                     pHandle=(Msg_info_handle*)data.readIntPtr();
                                                     readLength = data.readInt32();
                                                     pData =(unsigned char*) malloc( readLength*sizeof(unsigned char));
                                                     if(pData!=NULL)
                                                     {
                                                         LOGI("NexusFrontendService NEXUS_DVB_MESSAGE_GET_MESSAGE_DATA messagehandle=%x,databuff=%x readLength=%d", pHandle,pData,readLength);

                                                         retLength=dvbmessge_get(pHandle,pData,readLength);	 
                                                         reply->writeInt32(retLength);
                                                         reply->write((void*)pData,sizeof(unsigned char)*retLength);  
                                                         free(pData);
                                                     }
                                                     return android::NO_ERROR;
                                                 } break;

        case NEXUS_DVB_MESSAGE_CLOSE:{
                                         Msg_info_handle* pHandle;

                                         LOGI("NexusFrontendService OnTransact(%u)", code);
                                         pHandle=(Msg_info_handle*)data.readIntPtr();
                                         dvbmessage_close(pHandle);

                                         return android::NO_ERROR;
                                     }
                                     break;
        default:
                                     LOGE("ERROR! No such transaction(%d) in nexus service", code);
                                     return BBinder::onTransact(code, data, reply, flags);
    }

    return android::NO_ERROR;
}


/* private function*/
void NexusFrontendService::services_free(Service_info *p)
{
    Service_info *t;
    if (!p) return;

    t = p;
    while(p)
    {
        p=p->next;
        free(t);
        t = p;
    }
    service_totalnum=0;
    return;
}


int NexusFrontendService::tune_signal(int type,int freq,int s_rate,int mode,ServiceFrontend* retfrontend)
{
    NEXUS_FrontendUserParameters userParams;
    NEXUS_FrontendHandle frontend=NULL;
    NEXUS_FrontendQamSettings qamSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_FrontendQamMode qamMode = NEXUS_FrontendQamMode_e64;
    NEXUS_FrontendQamStatus qamStatus;
    NEXUS_Error rc = NEXUS_SUCCESS;

    int i,j;

    if(retfrontend==NULL)
    {
        LOGI("paramer error\n");
        return -1;
    }

    /* NEXUS_Platform_Join(); */
    NEXUS_Platform_GetConfiguration(&platformConfig);

    memset((void*)&qamStatus,0x00,sizeof(NEXUS_FrontendQamStatus));
    int k = 0;
    for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
    {
        frontend = platformConfig.frontend[i];
        LOGI("try to get frontend[%d] 0x%x \n ", i, frontend);
        NEXUS_FrontendCapabilities capabilities;
        if (frontend) {
            LOGI("frontend was %d \n ", i);
            NEXUS_Frontend_GetCapabilities(frontend, &capabilities);
            /* Does this frontend support qam? */
            LOGI("frontend:%d capabilities.qam is:%d\n",i,capabilities.qam);
            if ( capabilities.qam )
            {
                for(;k<3;k++)
                {
                    LOGI("fullbandfrontend[%d].nexusfrontendnum: %d\n", k, fullbandfrontend[k].nexusfrontendnum);   
                    if(fullbandfrontend[k].nexusfrontendnum !=i)
                    {
                        LOGI("fullbandfrontend[%d].nexusfrontendnum: %d\n", k, fullbandfrontend[k].nexusfrontendnum);               	    
                        goto mycheck;
                    }
                    else
                    {   
                        LOGI("frontend[%d] in use", i);
                        break;
                    }
                }
            }
        }
    }

mycheck: 
    LOGI("break for the %d frontend\n", i); 
    if (NULL == frontend )
    {
        LOGI("Unable to find QAM-capable frontend\n");
        return -1 ;
    }
    else
    {
        NEXUS_Frontend_GetUserParameters(frontend, &userParams);	

        LOGI("from the NEXUS_Frontend_GetUserParameters get the inputband [%d] \n", userParams.param1);
        for(j=0;j<3;j++)
        {
            if(fullbandfrontend[j].frontend==NULL)
            {
                fullbandfrontend[j].frontend=(int32_t*)frontend;
                fullbandfrontend[j].nexusfrontendnum=i;
                fullbandfrontend[j].inputband= userParams.param1;
                fullbandfrontend[j].freq= freq;

                retfrontend->frontend = fullbandfrontend[j].frontend;
                retfrontend->nexusfrontendnum=i;
                retfrontend->inputband=userParams.param1;
                retfrontend->freq = freq;

                break;
            }
        }
        if(j==3)
        {
            fullbandfrontend[0].frontend=(int32_t*)frontend;
            fullbandfrontend[0].nexusfrontendnum=i;
            fullbandfrontend[0].inputband= userParams.param1;
            fullbandfrontend[0].freq = freq;
            retfrontend->frontend=fullbandfrontend[j].frontend;
            retfrontend->nexusfrontendnum=i;
            retfrontend->inputband=userParams.param1;
            retfrontend->freq= freq;
        }
    }

    switch (mode) {
        case Qam64: qamMode = NEXUS_FrontendQamMode_e64; break;
        case Qam256: qamMode = NEXUS_FrontendQamMode_e256; break;
        case Qam1024: qamMode = NEXUS_FrontendQamMode_e1024; break;
        default: 
                      LOGI("unknown qam mode %d\n", mode); 
                      return -1;
    }

    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = freq * 1000000;
    qamSettings.mode = qamMode;
#if 0
    switch (qamMode) {
        default:
        case NEXUS_FrontendQamMode_e64: qamSettings.symbolRate = 6875000; break;
        case NEXUS_FrontendQamMode_e256: qamSettings.symbolRate = 5360537; break;
        case NEXUS_FrontendQamMode_e1024: qamSettings.symbolRate = 0; /* TODO */ break; 
    }
#else
    qamSettings.symbolRate  = s_rate * 1000;
#endif
    qamSettings.annex = NEXUS_FrontendQamAnnex_eA;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e8Mhz;
    /*qamSettings.lockCallback.callback = lock_callback;*/
    /*qamSettings.lockCallback.context = frontend;*/

    LOGI("tune tune:frontend  %x, freq %d, symbol %d qam %d\n ",frontend, qamSettings.frequency, qamSettings.symbolRate,qamSettings.mode);

    NEXUS_Frontend_TuneQam(frontend, &qamSettings);
    i = 0;
    while(i++ < 40)
    {
        BKNI_Sleep(50);
        NEXUS_Frontend_GetQamStatus(frontend, &qamStatus);
        if (qamStatus.fecLock &&  qamStatus.receiverLock){
            LOGI("QAM frontend lock %d,%d \n", qamStatus.fecLock,qamStatus.receiverLock);
            break;}
    }
    /* if (i >= 40) 
       {

       LOGI("Unable to finished frontend lock\n");
       return -1;

       }*/

    return 0;
}


int NexusFrontendService::tune_status(int32_t* frontend,  Frontend_status* status)
{
    NEXUS_FrontendQamStatus qamStatus;
    NEXUS_FrontendHandle frontendhandle=NULL;
    Frontend_status  locfrontendstatus;

    memset((void*)&qamStatus,0x00,sizeof(NEXUS_FrontendQamStatus));
    if(frontend!=NULL)
    {
        LOGI("tune_status frontend=%x",frontend);
        frontendhandle = (NEXUS_FrontendHandle) frontend;
    }
    else
    {
        return -1;
    }	

    NEXUS_Frontend_GetQamStatus(frontendhandle,&qamStatus);
    status->receiverLock = qamStatus.receiverLock;
    status->fecLock = qamStatus.fecLock;
    status->berEstimate = qamStatus.berEstimate;
    status->snrEstimate = qamStatus.snrEstimate;
    status->fecCorrected = qamStatus.fecCorrected;
    status->fecUncorrected = qamStatus.fecUncorrected;
    status->fecClean = qamStatus.fecClean;
    status->bitErrCorrected = qamStatus.bitErrCorrected;
    LOGI("tune_status receiverlock %d,feclock %d,snr %d,ber %d",status->receiverLock,status->fecLock,status->snrEstimate,status->bitErrCorrected);
    return 0;
}


int NexusFrontendService::get_section(NEXUS_MessageHandle msg, NEXUS_PidChannelHandle pid,
        unsigned char *mask,unsigned char* value,unsigned char* excl,int len,unsigned char* sec)
{
    NEXUS_MessageStartSettings startSettings;
    int count = 0;
    const uint8_t *buffer;
    size_t size;

    if (!msg || !pid || !mask || !value || !value || len > 8 || !sec) return -1;

    NEXUS_Message_GetDefaultStartSettings(msg, &startSettings);
    startSettings.pidChannel = pid;
    memcpy(startSettings.filter.mask,mask,len);
    memcpy(startSettings.filter.coefficient,value,len);
    memcpy(startSettings.filter.exclusion,excl,len);
    NEXUS_Message_Start(msg, &startSettings);

    while(count++ < 100)
    {
        NEXUS_Message_GetBuffer(msg, (const void **)&buffer, &size);
        if (!buffer || size == 0)
        {
            BKNI_Sleep(100);
            continue;
        }
#define TS_READ_16( BUF ) ((uint16_t)((BUF)[0]<<8|(BUF)[1]))
#define TS_PSI_GET_SECTION_LENGTH( BUF )    (uint16_t)(TS_READ_16( &(BUF)[1] ) & 0x0FFF)

        memcpy(sec,buffer,TS_PSI_GET_SECTION_LENGTH(buffer)+3);
        NEXUS_Message_ReadComplete(msg, size);
        break;
    }
    NEXUS_Message_Stop(msg);
    /*NEXUS_PidChannel_Close(pid);*/

    return (count >= 100)?-1:0;
}

#define DESCR_CA                             0x09
#define DESCR_GEN_LEN 2
typedef struct descr_gen_struct {
    u_char descriptor_tag                         :8;
    u_char descriptor_length                      :8;
} descr_gen_t;
#define CastGenericDescriptor(x) ((descr_gen_t *)(x))
#define GetDescriptorTag(x) (((descr_gen_t *) x)->descriptor_tag)
#define GetDescriptorLength(x) (((descr_gen_t *) x)->descriptor_length+DESCR_GEN_LEN)

#define TCAT  0x01
#define TPMT  0x02 

static int parserdescriptor(Service_info* p,unsigned char* buf,int table)
{
    unsigned short pid;
    LOGI("descriptor [%x]-[%x]-[%x]-[%x]-[%x]-[%x]-[%x]-[%x]-[%x]-[%x]",*buf,*(buf+1),*(buf+2),*(buf+3),*(buf+4),*(buf+5),*(buf+6),*(buf+7),*(buf+8),*(buf+9));      

    switch (GetDescriptorTag(buf))
    {
        case DESCR_CA:
            if(table=TPMT)
            {
                pid = ((buf[4] & 0x1F) << 8) + buf[5];
                /*  p->info.EcmPid = ((buf[4] & 0x1F) << 8) + buf[5];*/
                LOGI("get the ca pid =0x[%x]",pid);
                if(pid!=0x211)
                {
                    p->info.EcmPid = pid;
                    LOGI("info 0x[%x],ECMPid=%x",p,p->info.EcmPid);
                }
            }
            else if(table=TCAT)/* parse*/
            {
                p->info.EmmPid = ((buf[4] & 0x1F) << 8) + buf[5];
                LOGI("info 0x[%x],EMMPid=%x",p,p->info.EmmPid);
            }
            else
            {

                LOGI("error in parse CA");
            }
            break;

        default:
            break;
    }
    return GetDescriptorLength(buf);
}


static void  get_service_caemm(Service_info* p,unsigned char* buf)
{
    int len;
    if(!buf||!p)
    {
        LOGI("return for no cat table data");
        return;
    }
    len = (buf[1]&0x0f)<<8 | buf[2];
    len += 3;
    buf += 8;
    parserdescriptor(p,buf,TCAT);
    return ;
}

Service_info * NexusFrontendService::get_one_service(unsigned char* buf)
{
    Service_info *p;
    /*char * t;*/

    int len,p_info_len,remains,i,es_len,pid,p_info_len_left,p_info_len_use;
    unsigned char stream_type;

    if (!buf) return NULL;

    p = (Service_info*)malloc(sizeof(Service_info));
    if (!p) return NULL;

    memset(p,0x00,sizeof(Service_info));

    len = (buf[1]&0x0f)<<8 | buf[2];
    len += 3;
    buf += 8;
    p->info.p = (buf[0]<<8 | buf[1]) & 0x1fff;

    buf += 2;
    p_info_len = (buf[0]&0x0f)<<8 | buf[1];

    buf += 2;
    /* ecm pid could be found here.*/
    if(p_info_len)
    {
        p_info_len_left = 0;

        while(p_info_len_left<p_info_len)
        {
            LOGI("info_len=%d,left=%d",p_info_len,p_info_len_left);
            p_info_len_use=parserdescriptor(p,buf,TPMT);
            buf += p_info_len_use;
            p_info_len_left +=p_info_len_use;
            LOGI("finished info_len=%d,left=%d",p_info_len,p_info_len_left);
        }
    }

    /* buf +=  p_info_len;*/
    remains = len - p_info_len - 12;

    p->info.v = p->info.a = 0x1fff;
    while(remains > 0)
    {
        stream_type = buf[0];
        pid = (buf[1]<<8 | buf[2]) & 0x1fff;
        es_len = (buf[3]&0x0f)<<8 | buf[4];
        switch(stream_type)
        {
            case 0x01:
                if (p->info.v == 0x1fff)
                {
                    p->info.v = pid;
                    p->info.v_codec = ANDROID_NEXUS_VideoCodec_eMpeg1;
                }
                break;
            case 0x02:
                if (p->info.v == 0x1fff)
                {
                    p->info.v = pid;
                    p->info.v_codec = ANDROID_NEXUS_VideoCodec_eMpeg2;
                }
                break;
            case 0x80:
                if (p->info.v == 0x1fff)
                {
                    p->info.v = pid;
                    p->info.v_codec = ANDROID_NEXUS_VideoCodec_eMpeg2;
                }
                break;
            case 0x10:
                if (p->info.v == 0x1fff)
                {
                    p->info.v = pid;
                    p->info.v_codec = ANDROID_NEXUS_VideoCodec_eMpeg4Part2;
                }
                break;
            case 0x1b:
                if (p->info.v == 0x1fff)
                {
                    p->info.v = pid;
                    p->info.v_codec = ANDROID_NEXUS_VideoCodec_eH264;
                }
                break;

            case 0x03:
            case 0x04:
                if (p->info.a == 0x1fff)
                {
                    p->info.a = pid;
                    p->info.a_codec = ANDROID_NEXUS_AudioCodec_eMpeg;
                }
                break;
            case 0x81:
                if (p->info.a == 0x1fff)
                {
                    p->info.a = pid;
                    p->info.a_codec = ANDROID_NEXUS_AudioCodec_eAc3;
                    LOGE("get audio codec:%d\n",NEXUS_AudioCodec_eAc3);
                }
                break;
            case 0x0f:
                if (p->info.a == 0x1fff)
                {
                    p->info.a = pid;
                    p->info.a_codec = ANDROID_NEXUS_AudioCodec_eAac;
                }
                break;
            case 0x11:
                if (p->info.a == 0x1fff)
                {
                    p->info.a = pid;
                    p->info.a_codec = ANDROID_NEXUS_AudioCodec_eAacPlus;
                }
                break;
            case 0x87:
                if (p->info.a == 0x1fff)
                {
                    p->info.a = pid;
                    p->info.a_codec = ANDROID_NEXUS_AudioCodec_eAc3Plus;
                }
                break;
            default:
#if JNI_NEXUS
                LOGI("not support so far, stream_type: %d\n",stream_type);
#endif
                break;

        }
        buf += es_len + 5;
        remains -= es_len + 5;
    }

    return p;
}


static void message_callback1(void *context, int param)
{
    BSTD_UNUSED(param);
    LOGE("Enter message_callback1");
}


Service_info* NexusFrontendService::get_service_list(int inputband)
{
    NEXUS_ParserBand parserBand = NEXUS_ParserBand_e0;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_MessageHandle msg;
    NEXUS_MessageSettings settings;
    NEXUS_PidChannelHandle pidChannel;
    Service_info * list = NULL; 

    unsigned char mask[8],value[8],excl[8];
    unsigned char* sec = NULL;
    unsigned char* pat = NULL;

    int len,rc;

    sec = (unsigned char*)malloc(1024);
    if (!sec) return NULL;

    if(inputband==0xFF)
    {
        LOGE("tune need lock when get service list");
        goto fail_msg;
    }
    parserBand = (NEXUS_ParserBand)((int)(inputband - NEXUS_InputBand_e0) + NEXUS_ParserBand_e0);

    LOGD("get_service_list, inputband: %d, parserBand: %d\n", inputband, parserBand);

    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputband;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);

    NEXUS_Message_GetDefaultSettings(&settings);

    settings.dataReady.callback = message_callback1;
    settings.maxContiguousMessageSize = 4096;
    msg = NEXUS_Message_Open(&settings);

    if (msg == NULL) goto fail_msg;

    memset(mask,0xff,8);
    memset(excl,0xff,8);

    /* get PAT firstly */
    pidChannel = NEXUS_PidChannel_Open(parserBand, 0x0, NULL);
    if (!pidChannel) goto fail_msg;

    rc = get_section(msg,pidChannel,mask,value,excl,8,sec);

    NEXUS_PidChannel_Close(pidChannel);

    if (rc) goto fail_msg;

    LOGE("get PAT sections\n");

    len = (sec[1]&0x0f)<<8 | sec[2];
    len += 3;

    pat = (unsigned char*)malloc(len);
    if (!pat) 
    {
        LOGE("no memory ,just return\n");
        goto fail_msg;
    }

    /*copy a pat here*/
    memcpy(pat,sec,len);
    /* get each PMT*/
    {
        int p_num,pid,j;
        unsigned char* p = pat + 8;
        int loop = (len -12)/4;
        Service_info * t=NULL, *n=NULL; 

        mask[0]  = 0x00;
        mask[2]  = 0x00;
        mask[3]  = 0x00;
        for(j=0;j<loop;j++)
        {
            p_num = p[0]<<8 | p[1];

            if (p_num == 0)
            {
                p += 4;
                continue;
            }
            pid = (p[2] << 8 | p[3]) & 0x1fff;

            pidChannel = NEXUS_PidChannel_Open(parserBand, pid, NULL);
            if (!pidChannel) goto fail_msg;

            value[0] = 0x02;
            value[2] = p[0];
            value[3] = p[1];
            p += 4;
            LOGE("prepare to get PMT with PID:%x,program number: %x\n",pid,p_num);

            rc = get_section(msg,pidChannel,mask,value,excl,8,sec);

            NEXUS_PidChannel_Close(pidChannel);

            if (rc) 
            {
                continue;
                /*goto fail_msg;*/
            }
            LOGE("get PMT section\n");

            /* parsing received PMT here*/
            t = get_one_service(sec);

            /* parse CAT table and get the EMM pid*/
            memset(mask,0xff,8);
            memset(excl,0xff,8);

            value[0]=0x01;

            /* get PAT firstly */
            pidChannel = NEXUS_PidChannel_Open(parserBand, 0x01, NULL);
            if (!pidChannel) goto fail_msg;

            rc = get_section(msg,pidChannel,mask,value,excl,8,sec);

            get_service_caemm(t,sec);

            NEXUS_PidChannel_Close(pidChannel);            

            if (t)
            {
                if (!list) list = t;
                else
                {
                    n = list;
                    while(n->next) n = n->next;
                    n->next = t;
                }
            }
        }
    }

    free(sec);
    return list;

fail_msg:
    free(sec);
    if(msg!=NULL)
    {
        NEXUS_Message_Close(msg);
    }
    return NULL;
}


int NexusFrontendService::get_totalservicenum()
{
    Service_info * p;
    int totalnum=0;

    p=service_list;

    while(p)
    {
        p=p->next;
        totalnum++;
    }
    service_totalnum = totalnum;
    return totalnum;
}	

Service_info* NexusFrontendService::get_one_service_bynum(int num)
{
    Service_info * p;

    int count;
    int totalnum=0;

    p=service_list;

    totalnum = get_totalservicenum();
    count=0;
    if(num<totalnum)
    {
        while(p)
        {
            if(count==num)
            {
                break;
            }
            p=p->next;
            count++;

        }
        return p;
    }
    else
    {
        return NULL;
    }
}


static void message_callback(void *context, int param)
{
    BSTD_UNUSED(param);

    pid_t p;

    p = getpid();
    LOGI("Call SIGUSER1 in the BpNexusFrontendService value is %d, process id [%x]",SIGUSR1,p);
    kill((pid_t)context,SIGUSR1);
    printf("kill have called\n");
}


Msg_info_handle* NexusFrontendService::dvbmessage_open(int inputband,unsigned short pid ,unsigned char* mask, unsigned char* value, unsigned char* excl, int length,pid_t p)
{
    NEXUS_ParserBand parserBand = NEXUS_ParserBand_e0;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_MessageHandle msg;
    NEXUS_MessageSettings settings;
    NEXUS_PidChannelHandle pidChannel;
    NEXUS_MessageStartSettings startSettings;
    Msg_info_handle*  msghandleid;

    int count = 0;
    size_t size;
    int len,rc;

    LOGE("Enter dvbmessage_open");

    msghandleid = (Msg_info_handle*)malloc(sizeof(Msg_info_handle));

    if(msghandleid==NULL)
    {
        LOGE("no enough memory to allocate!\n ");
        return NULL;
    }

    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputband;

    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);

    NEXUS_Message_GetDefaultSettings(&settings);

    settings.dataReady.callback = message_callback;
    settings.dataReady.context = (void*)p ;

    settings.maxContiguousMessageSize = MAXMSGBUFSIZE;
    msg = NEXUS_Message_Open(&settings);

    if (msg == NULL) 
    {
        LOGE("open message failed, return!!");
        return NULL;
    }

    pidChannel = NEXUS_PidChannel_Open(parserBand, pid, NULL);
    if(!pidChannel) 
    {
        LOGE("open pid channel failed, return!!\n");
        return NULL;
    }

    NEXUS_Message_GetDefaultStartSettings(msg, &startSettings);	
    startSettings.pidChannel = pidChannel;
    memcpy(startSettings.filter.mask,mask,sizeof(unsigned char)*16);
    memcpy(startSettings.filter.coefficient,value,sizeof(unsigned char)*16);
    memcpy(startSettings.filter.exclusion,excl,sizeof(unsigned char)*16);

    if(NEXUS_Message_Start(msg, &startSettings))
    {
        return NULL;
    }
    LOGI("dvbmessge_open nexus msghandle is %x\n",msg);
    msghandleid->msgid = (unsigned int)msg;
    msghandleid->pidchannelid = (unsigned int) pidChannel;
    msghandleid->processid = p;

    LOGE("dvbmessage_open return with msghandleid=%x",msghandleid);
    return  msghandleid;
}

int NexusFrontendService::dvbmessge_get( Msg_info_handle* msghandle, unsigned char* msgbuff, unsigned int bufsize)
{
    NEXUS_MessageHandle msg;
    pid_t p;
    const uint8_t *buffer;
    size_t size;
    int len,count,rc;
    int truebuffsize=0;

    if(msghandle==0)
    {
        LOGE("messageid is invalid one, return!!\n");
        return 0;
    }
    if((!msgbuff)||(bufsize<7))
    {
        LOGE("the paramter is invalid %x,%d\n",msgbuff,bufsize);
    }

    msg=(NEXUS_MessageHandle)msghandle->msgid;
    p= msghandle->processid;

    LOGI("dvbmessge_get nexus msghandle is %x\n",msg);

    while(count++ < 100)
    {
        NEXUS_Message_GetBuffer(msg, (const void **)&buffer, &size);

        if (!buffer || size == 0)
        {
            BKNI_Sleep(100);
            continue;
        }

        if(size<bufsize)
        {

            bufsize = size;

        }
        else
        {
            LOGE("get  msg length is biger than the buffer!\n");
        }
        LOGI("dvbmessge_get nexus copy size is %d \n", size);
        LOGI("dvbmessge_get data is :\n");

        for(len=0;len<size;len++)
        {
            LOGI("0x%x\t",*(buffer+len));
        }
        LOGI("finished dvbmessge_get data print\n");

        memcpy(msgbuff,buffer,bufsize);

        NEXUS_Message_ReadComplete(msg, bufsize); /* if you need recopy it ,*/
        break;
    }

    return size;
}

void NexusFrontendService::dvbmessage_close(Msg_info_handle* msghandle)
{
    NEXUS_MessageHandle msg;
    NEXUS_PidChannelHandle pidChannel;

    if(msghandle==NULL)
    {
        LOGE("msghandle is NULL\n");
        return;
    }
    if(	msghandle->msgid==0)
    {
        LOGE("msghandle's msgid is NULL\n");
        return;
    }
    if(	msghandle->pidchannelid==0)
    {
        LOGE("msghandle's pidchannelid is NULL\n");
        return;
    }

    NEXUS_Message_Stop((NEXUS_MessageHandle)(msghandle->msgid));
    NEXUS_Message_Close((NEXUS_MessageHandle)(msghandle->msgid));
    NEXUS_PidChannel_Close((NEXUS_PidChannelHandle)(msghandle->pidchannelid));
    /*  free(msghandle);*/

    /* msghandle=NULL;*/
    return;
}

void NexusFrontendService::dvbca_play(unsigned short ecmpid,unsigned short emmpid,unsigned short vpid, unsigned short apid,NEXUS_PidChannelHandle vpidChannel,NEXUS_PidChannelHandle apidChannel)
{
    return ;
}


