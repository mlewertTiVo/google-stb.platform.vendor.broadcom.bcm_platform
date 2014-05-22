/* nexusfrontend_base.cpp*/
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include <utils/String16.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/threads.h>
#include <string.h>
#include "nexus_frontend.h"
#include "nexus_platform.h"
#include "nexus_parser_band.h"
#include "nexus_message.h"
#include "nexus_pid_channel.h"
#include "bkni.h"
#include "nexus_frontendinterface.h"
#include "nexusfrontendservice.h"
#define LOG_TAG "NexusFrontendService_proxy"

android_IMPLEMENT_META_FRONTENDINTERFACE(NexusFrontendClient, NEXUS_FRONTEND_INTERFACE_NAME)

static void frontend_signalhandler (int sig)
{
    LOGI("Get signal from NexusFrontendService \n");	
}

void BpNexusFrontendClient::NexusFrontendParamterSet(void* pParam)
{
    android::Parcel data,reply;
    FrontendParam* pFrontendParam;

    pFrontendParam = (FrontendParam*)pParam;
    if(pFrontendParam !=NULL)
    {
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
        data.writeInt32(pFrontendParam->freq);	
        data.writeInt32(pFrontendParam->symbolrate);
        data.write((void*)&(pFrontendParam->qammode),sizeof(unsigned char));
        remote()->transact(NEXUS_FRONTEND_TUNE_PARAMETER_SET,data,&reply);
    }
}


void BpNexusFrontendClient::NexusFrontendParamterGet(void* pParam)
{
    android::Parcel data,reply;
    FrontendParam* pFrontendParam;
    unsigned char qam;

    pFrontendParam = (FrontendParam*)pParam;

    if(pFrontendParam !=NULL)
    {
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
        remote()->transact(NEXUS_FRONTEND_TUNE_PARAMETER_GET,data,&reply);
        pFrontendParam->freq=reply.readInt32();	
        pFrontendParam->symbolrate=reply.readInt32();
        reply.read((void*)&qam,sizeof(unsigned char));
        pFrontendParam->qammode=(FrontendQam)qam;
    }
}


int BpNexusFrontendClient::NexusFrontendTune(ServiceFrontend* frontend ) 
{
    android::Parcel data,reply;

    LOGI("Call the NexusFrontendTune");	
    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    remote()->transact(NEXUS_FRONTEND_TUNE_TUNE,data,&reply);
    if(frontend)
    {
        reply.read((void*)frontend,sizeof(ServiceFrontend));
        return 0;
    }
    return 1;
}


void BpNexusFrontendClient::NexusFrontendStatusGet(int32_t* frontend ,void* pStatus)
{
    android::Parcel data,reply;
    Frontend_status* pFrontendStatus;
    pFrontendStatus=(Frontend_status*)pStatus;
    unsigned char receiver, fec;

    if(pFrontendStatus!=NULL)
    {	
        LOGI("Call the NexusFrontendStatusGet");
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
        data.writeIntPtr((intptr_t)frontend);	
        remote()->transact(NEXUS_FRONTEND_TUNE_STATUS_GET,data,&reply);
        reply.read((void*)&receiver,sizeof(unsigned char));
        pFrontendStatus->receiverLock=receiver;
        reply.read((void*)&fec,sizeof(unsigned char));
        pFrontendStatus->fecLock=fec;
        pFrontendStatus->berEstimate=reply.readInt32();
        pFrontendStatus->snrEstimate=reply.readInt32();
        pFrontendStatus->bitErrCorrected=reply.readInt32();
        pFrontendStatus->dsChannelPower=reply.readInt32();
        LOGI("NexusFrontendStatusGet :receiverlock :%d fecLock:%d,ber:%d,snr:%d",pFrontendStatus->receiverLock,pFrontendStatus->fecLock,pFrontendStatus->berEstimate,pFrontendStatus->snrEstimate);
    }
}

void BpNexusFrontendClient::NexusDVBServiceSearch( int inputband)
{
    android::Parcel data,reply;

    LOGI("Call the NexusDVBServiceSearch");	
    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    data.writeInt32(inputband);	
    remote()->transact(NEXUS_DVB_SERVICE_SEARCH_SERVICE,data,&reply);
}


int  BpNexusFrontendClient::NexusDVBServiceGetTotalNum( ) 
{
    android::Parcel data,reply;

    LOGI("Call the NexusDVBServiceGetTotalNum");	
    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    remote()->transact(NEXUS_DVB_SERVICE_GET_TOTALNUM,data,&reply);

    return reply.readInt32();
}


void BpNexusFrontendClient::NexusDVBServiceGetByNum(int num,void* pService ) 
{
    android::Parcel data,reply;
    Nexus_Channel_Service_info* pServiceInfo;
    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    data.writeInt32(num);	
    remote()->transact(NEXUS_DVB_SERVICE_GET_BYNUM,data,&reply);

    if(pService!=NULL)
    {
        reply.read(pService,sizeof(Nexus_Channel_Service_info));
        pServiceInfo =(Nexus_Channel_Service_info*) pService;

        LOGI("get service a=%x,v=%x,p=%x",pServiceInfo->a,pServiceInfo->v,pServiceInfo->p);
    }
}


void BpNexusFrontendClient::NexusDVBServiceReset( ) 
{
    android::Parcel data,reply;
    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    remote()->transact(NEXUS_DVB_SERVICE_RESET,data,&reply);
} 


void BpNexusFrontendClient::NexusDVBServicePlay(unsigned int ecmpid,unsigned int emmpid,unsigned int videopid, unsigned int audiopid,unsigned int vpidchannel,unsigned int apidchannel)
{
    android::Parcel data,reply;

    data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
    data.writeInt32(ecmpid);
    data.writeInt32(emmpid);	
    data.writeInt32(videopid);
    data.writeInt32(audiopid);	
    data.writeInt32(vpidchannel);
    data.writeInt32(apidchannel);
    remote()->transact(NEXUS_DVB_SERVICE_PLAY,data,&reply);
}


int32_t* BpNexusFrontendClient::NexusDVBMessageOpen(int inputband,void* pMessageParam)
{
    android::Parcel data,reply;
    sigset_t newmask;
    sigset_t oldmask;
    sigset_t pendmask;
    DVBMessageParam* pParam;
    pid_t p;

    LOGI("Call signal in the NexusDVBMessageOpen");
    LOGI("Call SIGUSER1 in the BpNexusFrontendClient value is %d",SIGUSR1);
    signal (SIGUSR1, frontend_signalhandler);
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);
    pParam =(DVBMessageParam*) pMessageParam;
    p = getpid();
    LOGI("Call the NexusDVBMessageOpen in BpNexusFrontendClient with process id [%x]",p);

    if(pParam!=NULL)
    {
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());

        data.writeInt32(pParam->pid);	
        data.writeInt32(inputband);
        data.write((void*)&p,sizeof(pid_t));
        data.write((void*)(&(pParam->filter)),sizeof(DvbmessageFilter));	

        remote()->transact(NEXUS_DVB_MESSAGE_OPEN,data,&reply);

        return (int32_t*)reply.readIntPtr();
    }
    return NULL;
}


int  BpNexusFrontendClient::NexusDVBMessageGetMessageData(int32_t* pMessageHandle,unsigned char *pData,int32_t iDataLength)
{
    android::Parcel data,reply;
    int retLength;
    Msg_info_handle* pHandle;
    pHandle =(Msg_info_handle*) pMessageHandle;
    retLength = 0;
    unsigned char* pTempData;

    LOGI("Call the NexusDVBMessageGetMessageData pHandle=%x",pHandle);

    if(pHandle!=NULL)
    {
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
        data.writeIntPtr((intptr_t)pHandle);	
        data.writeInt32(iDataLength);
        /*	LOGI("Call the NexusDVBMessageGetMessageData pData=%x",pData);
                data.write((void*)pData,sizeof(unsigned char)*iDataLength);*/
        remote()->transact(NEXUS_DVB_MESSAGE_GET_MESSAGE_DATA,data,&reply);
        /*	pData=(unsigned char*)reply.ReadIntPtr();*/
        retLength=reply.readInt32();
        pTempData=(unsigned char*) malloc(sizeof(unsigned char)*retLength);
        if(pTempData!=NULL)
        {
            reply.read((void*)pTempData,sizeof(unsigned char)*retLength);
        }

        if(iDataLength<retLength)
        {
            retLength =  iDataLength;
        }

        memcpy(pData,pTempData,sizeof(unsigned char)*retLength);
        free(pTempData);	
    }
    return retLength;
}


void BpNexusFrontendClient::NexusDVBMessageClose(int32_t* pMessageHandle)
{
    android::Parcel data,reply;
    Msg_info_handle* pHandle;
    pHandle =(Msg_info_handle*) pMessageHandle;

    if(pHandle!=NULL)
    {
        data.writeInterfaceToken(INexusFrontendService::getInterfaceDescriptor());
        data.writeIntPtr((intptr_t)pHandle);	
        remote()->transact(NEXUS_DVB_MESSAGE_CLOSE,data,&reply);
    }
}         

