/*#include "com_android_broadcom_tsplayer_native_frontend.h"*/

#include <jni.h>
#include <cutils/memory.h>
#include <utils/Log.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "nexus_frontendinterface.h"
#include "nexusfrontendservice.h"
#include "nexus_interface.h"
#include "nexusservice.h"

class BpNexusClient: public android::BpInterface<INexusClient>
{
    public:
        BpNexusClient(const android::sp<android::IBinder>& impl)
            : android::BpInterface<INexusClient>(impl)
        {
        }

        void NexusHandles(NEXUS_TRANSACT_ID eTransactId, int32_t *pHandle)
        {
            android::Parcel data, reply;
            data.writeInterfaceToken(INexusService::getInterfaceDescriptor());
            remote()->transact(eTransactId, data, &reply);
            *pHandle = reply.readInt32();
        }
};
android_IMPLEMENT_META_INTERFACE(NexusClient, NEXUS_INTERFACE_NAME)

using namespace android;

#define JNI_NEXUS 1

static jdouble JNICALL Java_com_android_tsplayer_native_1frontend_lock_1signal(JNIEnv *env, jobject obj, jint t, jint f, jint s, jint m);
static jint JNICALL Java_com_android_tsplayer_native_1frontend_services_1get(JNIEnv *env, jobject obj,jint inputband);  
static jint JNICALL Java_com_android_tsplayer_native_1frontend_ca_playset(JNIEnv *env,jobject obj,jint v,jint a,jint ecmpid,jint emmpid);

#if !JNI_NEXUS
int main(void)
{
    Service_info *p;
    Service_info *prog = get_service_list(tune_signal(0,716,0,64));

    p = prog;
    while(p)
    {
        printf("------------------------------------\n");
        printf("audio pid       :%d\n",p->a);
        printf("video pid       :%d\n",p->v);
        printf("pcr pid         :%d\n",p->p);
        printf("audio codec     :%d\n",p->a_codec);
        printf("video codec     :%d\n",p->v_codec);
        printf("------------------------------------\n");

        p = p->next;
    }
    services_free(prog);

    return 0;
}
#else

#if 1
static JNINativeMethod gMethods[] = {

    {"lock_signal",   "(IIII)D", (void *)Java_com_android_tsplayer_native_1frontend_lock_1signal},
    {"services_get",   "(I)I", (void *)Java_com_android_tsplayer_native_1frontend_services_1get},
    {"dvbca_playset",   "(IIII)I",(void*)Java_com_android_tsplayer_native_1frontend_ca_playset},
};

static int registerNativeMethods(JNIEnv* env, const char* className,
        JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        LOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        LOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int register_tuner_jni(JNIEnv *env){

    return registerNativeMethods(env,"com/android/tsplayer/native_frontend", gMethods,  sizeof(gMethods) / sizeof(gMethods[0]));
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("ERROR: GetEnv failed\n");
        return result;
    }
    assert(env != NULL);

    if (register_tuner_jni(env) < 0)
    {
        LOGE("ERROR: register tuner interface error failed\n");
        return result;
    }

    result = JNI_VERSION_1_4;

    return result;
}
#endif
/*
 * Class:     com_android_broadcom_tsplayer_native_frontend
 * Method:    lock_signal
 * Signature: (IIII)I
 */
    JNIEXPORT jdouble JNICALL Java_com_android_tsplayer_native_1frontend_lock_1signal
(JNIEnv *env, jobject obj, jint t, jint f, jint s, jint m)
{
    // set up the thread-pool
    ServiceFrontend frontend;
    double retd;

    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<android::IBinder> binder;
    android::sp<INexusFrontendClient> client;

    LOGE("call the function Java_com_android_tsplayer_native_1frontend_lock_1signal...");

    FrontendParam param,param1;
    Frontend_status status;

    int32_t frontendhandle ;
    int inputband;

    LOGE("begin testfrontend");

    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    binder = sm->getService(android::String16(NEXUS_FRONTEND_INTERFACE_NAME));
    if (binder == 0){
        LOGE("Nexusfrontendservice is not ready, abort...");
        return  0x0;
    }

    LOGI("Get handles from nexusfrontendservice...");

    // create a client to nexusfrontendservice
    client = android::interface_cast<INexusFrontendClient>(binder); 	

    client->NexusDVBServiceReset();

    param.freq=f;
    param.symbolrate = s;

    switch(m)
    {
        case 0:
            param.qammode = Qam8;
            break;
        case 1:
            param.qammode = Qam32;
            break;	
        case 2:
            param.qammode = Qam64;
            break;
        case 3:
            param.qammode = Qam128;
            break;
        case 4:
            param.qammode = Qam256;
            break;
        case 5:	
            param.qammode = Qam1024;
            break;
        default:
            param.qammode = Qam64;
            break;  				
    }

    /* param.qammode =(FrontendQam) m;*/

    client->NexusFrontendParamterSet(&param);
    client->NexusFrontendParamterGet(&param1);

    LOGE("get freq=%d,symbolrate=%d,qam=%d\n",param1.freq,param1.symbolrate,param1.qammode);

    if(client->NexusFrontendTune(&frontend)==0)
    {
        client->NexusFrontendStatusGet((int32_t*)frontend.frontend,&status);

        LOGE("get frontend [%x]lock=%d\n",frontend.frontend,status.fecLock);
    }  

    frontendhandle =(unsigned int)( frontend.frontend);
    inputband = frontend.inputband;

    LOGE("get frontend [%x] inputband[%d]\n",frontend.frontend,frontend.inputband);
    /*android::IPCThreadState::self()->joinThreadPool();*/
    LOGE("return inputband[%d]\n",inputband);

    return (int)((unsigned int)inputband);
}

/*
 * Class:     com_android_broadcom_tsplayer_native_frontend
 * Method:    services_get
 * Signature: ()I
 */
/*
   JNIEXPORT jint JNICALL Java_com_android_tsplayer_native_1frontend_services_1get
   (JNIEnv *env, jobject obj)
   */  
    JNIEXPORT jint JNICALL Java_com_android_tsplayer_native_1frontend_services_1get
(JNIEnv *env, jobject obj,jint inputband)
{
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<android::IBinder> binder;
    android::sp<INexusFrontendClient> client;

    int              i,servicenum;
    Nexus_Channel_Service_info* pS;

    LOGE("call the function Java_com_android_tsplayer_native_1frontend_services_1get...");

    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    binder = sm->getService(android::String16(NEXUS_FRONTEND_INTERFACE_NAME));
    if (binder == 0){
        LOGE("Nexusfrontendservice is not ready, abort...");
        return  -1;
    }

    client = android::interface_cast<INexusFrontendClient>(binder); 	
    client->NexusDVBServiceReset();
    client->NexusDVBServiceSearch(inputband);

    servicenum=0;
    servicenum=client->NexusDVBServiceGetTotalNum();
    if(servicenum!=0)
    {
        jclass clazz = (env)->GetObjectClass(obj);
        pS=(Nexus_Channel_Service_info*)malloc(sizeof(Nexus_Channel_Service_info)*servicenum);

        LOGE("Get the function set_services...");

        jmethodID set_service =
            (env)->GetMethodID(clazz, "set_services", "(IIIIIIIII)V");

        for(i=0;i<servicenum;i++)
        {
            client->NexusDVBServiceGetByNum(i,(void*)(&(*(pS+i))));

            (env)->CallVoidMethod(obj,set_service,inputband,
                    (Nexus_Channel_Service_info*)(pS+i)->a,(Nexus_Channel_Service_info*)(pS+i)->v,(Nexus_Channel_Service_info*)(pS+i)->p,(Nexus_Channel_Service_info*)(pS+i)->a_codec,(Nexus_Channel_Service_info*)(pS+i)->v_codec,(Nexus_Channel_Service_info*)(pS+i)->EcmPid,(Nexus_Channel_Service_info*)(pS+i)->EmmPid,i);
            LOGE("get the service : %x,%x,%x,%x,%x",(pS+i)->v,(pS+i)->a,(pS+i)->p,(pS+i)->EcmPid,(pS+i)->EmmPid);
        }
    }

    free(pS);

    /*  IPCThreadState::self()->joinThreadPool();*/

    LOGE("finish call the function Java_com_android_tsplayer_native_1frontend_services_1get...");
    return 0;
}


JNIEXPORT jint JNICALL Java_com_android_tsplayer_native_1frontend_ca_playset(JNIEnv *env,jobject obj,jint v,jint a,jint ecmpid,jint emmpid)
{
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    android::sp<android::IBinder> binder;
    android::sp<INexusFrontendClient> client;
    android::sp<INexusClient> handleclient;

    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    unsigned int vhandle, ahandle; 	

    binder = sm->getService(android::String16(NEXUS_INTERFACE_NAME));
    if (binder == 0){
        LOGE("Nexusservice is not ready, abort...");
        return  -1;
    }

    handleclient = android::interface_cast<INexusClient>(binder); 		

    /*  handleclient->NexusHandles(GET_HANDLE_NEXUS_VIDEO_PIDCHANNEL,(int32_t*)&vhandle); 	
        handleclient->NexusHandles(GET_HANDLE_NEXUS_AUDIO_PIDCHANNEL,(int32_t*)&ahandle);*/ 	

    binder = sm->getService(android::String16(NEXUS_FRONTEND_INTERFACE_NAME));
    if (binder == 0){
        LOGE("Nexusfrontendservice is not ready, abort...");
        return  -1;
    }

    client = android::interface_cast<INexusFrontendClient>(binder); 	

    LOGE("in Java_com_android_tsplayer_native_1frontend_ca_playset call NexusDVBServicePlay with ecmpid[%x], emmpid[%x],videopid[%x],audiopid[%x], vhandle[%x] ahandle[%x]",ecmpid,emmpid,v,a,vhandle,ahandle );
    client->NexusDVBServicePlay(ecmpid,emmpid,v, a,vhandle,ahandle)	;

    LOGE("finish call the function Java_com_android_tsplayer_native_1frontend_ca_playset...");
    return 0;
}

#endif
