/******************************************************************************
 *    (c)2014 Broadcom Corporation
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
#include <jni.h>
#include <utils/Log.h>
#include <assert.h>

// This file has fully qualified signatures & names. Just
// use them as is & do not modify this header file
#include "com_broadcom_tvinput_TunerHAL.h"
#include "TunerHAL.h"
#include "TunerInterface.h"
#include "Broadcast.h"

// Enable this for debug prints
#define DEBUG_JNI

#ifdef DEBUG_JNI
#define TV_LOG	ALOGE
#else
#define TV_LOG
#endif

// All globals must be JNI primitives, any other data type 
// will fail to hold its value across different JNI methods
void *j_main;
static Tuner_Data *g_pTD;
static jclass gTunerServiceClass;

// The signature syntax is: Native-method-name, signature & fully-qualified-name
static JNINativeMethod gMethods[] = 
{
    {"initialize",     "(Ljava/lang/Object;)I",     (void *)Java_com_broadcom_tvinput_TunerHAL_initialize},
    {"startBlindScan", "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_startBlindScan},  
    {"stopScan",       "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_stopScan},  
    {"tune",           "(Ljava/lang/String;)I",     (void *)Java_com_broadcom_tvinput_TunerHAL_tune},
    {"getChannelList", "()[Lcom/broadcom/tvinput/ChannelInfo;",     (void *)Java_com_broadcom_tvinput_TunerHAL_getChannelList},
    {"getProgramList", "(Ljava/lang/String;)[Lcom/broadcom/tvinput/ProgramInfo;",     (void *)Java_com_broadcom_tvinput_TunerHAL_getProgramList},
    {"getScanInfo",    "()Lcom/broadcom/tvinput/ScanInfo;",     (void *)Java_com_broadcom_tvinput_TunerHAL_getScanInfo},
    {"getUtcTime",     "()J",      (void *)Java_com_broadcom_tvinput_TunerHAL_getUtcTime},  
    {"stop",           "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_stop},  
    {"release",        "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_release},  
    {"setSurface",     "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_setSurface},  
    {"getVideoTrackInfoList", "()[Lcom/broadcom/tvinput/VideoTrackInfo;", (void *)Java_com_broadcom_tvinput_TunerHAL_getVideoTrackInfoList},  
};

const String16 INexusTunerService::descriptor(TUNER_INTERFACE_NAME);

String16 INexusTunerService::getInterfaceDescriptor() 
{
    return INexusTunerService::descriptor;
}

NexusTunerService::NexusTunerService()
{
    ALOGE("NexusTunerService created");
}

void NexusTunerService::instantiate() 
{
    NexusTunerService *pNTS = new NexusTunerService();

    ALOGE("NexusTunerService::instantiate, creating the service");
    defaultServiceManager()->addService(NexusTunerService::descriptor, pNTS);
}

NexusTunerService::~NexusTunerService()
{
    ALOGE("NexusTunerService destroyed");
}

status_t NexusTunerService::onTransact(uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags)
{
//    CHECK_INTERFACE(INexusTunerService, data, reply);

    ALOGE("NexusTunerService::onTransact, code = 0x%x", code);
    switch (code)
    {
        case GET_TUNER_CONTEXT:
        {
            ALOGE("NexusTunerService::onTransact, sending back nexus_client");
            reply->writeInt32((int32_t)g_pTD->nexus_client);
        }
        break;

        default:
            ALOGE("NexusTunerService::onTransact, Error!! No such transaction(%d)", code);
            return BBinder::onTransact(code, data, reply, flags);
        break;
    }

    return NO_ERROR;
}

static int registerNativeMethods(JNIEnv* env, const char* className, JNINativeMethod* pMethods, int numMethods)
{
    jclass clazz;
	int i;

    clazz = env->FindClass(className);
    if (clazz == NULL) 
    {
        TV_LOG("%s: Native registration unable to find class '%s'", __FUNCTION__, className);
        return JNI_FALSE;
    }

    TV_LOG("libbcmtv_jni: numMethods = %d", numMethods);
	for (i = 0; i < numMethods; i++)
	{
		TV_LOG("%s: pMethods[%d]: %s, %s, %p", __FUNCTION__, i, pMethods[i].name, pMethods[i].signature, pMethods[i].fnPtr);
	}

    if (env->RegisterNatives(clazz, pMethods, numMethods) < 0) 
    {
        TV_LOG("%s: RegisterNatives failed for '%s'", __FUNCTION__, className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) 
    {
        TV_LOG("%s: GetEnv failed", __FUNCTION__);
        return result;
    }
    assert(env != NULL);
        
    TV_LOG("%s: GetEnv succeeded!!", __FUNCTION__);
    if (registerNativeMethods(env, "com/broadcom/tvinput/TunerHAL", gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) != JNI_TRUE)
    {
        TV_LOG("%s: register interface error!!", __FUNCTION__);
        return result;
    }
    TV_LOG("%s: register interface succeeded!!", __FUNCTION__);

    result = JNI_VERSION_1_4;

    // Allocate memory for the global pointer
    g_pTD = (Tuner_Data *) calloc(1, sizeof(Tuner_Data));
    g_pTD->vm = vm;

	// Launch the binder service
    NexusTunerService::instantiate();
    ALOGE("%s: NexusTunerService is now ready", __FUNCTION__);
//    IPCThreadState::self()->joinThreadPool();

    return result;
}

#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

typedef void (* BCMSIDEBAND_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class BcmSidebandBinder : public HwcListener
{
public:

    BcmSidebandBinder() : cb(NULL), cb_data(0) {};
    virtual ~BcmSidebandBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->registerListener(this, HWC_BINDER_SDB);
       else
           ALOGE("%s: failed to associate %p with BcmSidebandBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->unregisterListener(this);
       else
           ALOGE("%s: failed to dissociate %p from BcmSidebandBinder service.", __FUNCTION__, this);
    };

    inline void getvideo(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getVideoSurfaceId(this, index, value);
       }
    };

    inline void getsideband(int index, int &value) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getSidebandSurfaceId(this, index, value);
       }
    };

    inline void getgeometry(int type, int index,
                            struct hwc_position &frame, struct hwc_position &clipped,
                            int &zorder, int &visible) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getGeometry(this, type, index, frame, clipped, zorder, visible);
       }
    };

    void register_notify(BCMSIDEBAND_BINDER_NTFY_CB callback, int data) {
       cb = callback;
       cb_data = data;
    }

private:
    BCMSIDEBAND_BINDER_NTFY_CB cb;
    int cb_data;
};

class BcmSidebandBinder_wrap
{
private:

   sp<BcmSidebandBinder> ihwc;

public:
   BcmSidebandBinder_wrap(void) {
      ALOGV("%s: allocated %p", __FUNCTION__, this);
      ihwc = new BcmSidebandBinder;
      ihwc.get()->listen();
   };

   virtual ~BcmSidebandBinder_wrap(void) {
      ALOGV("%s: cleared %p", __FUNCTION__, this);
      ihwc.get()->hangup();
      ihwc.clear();
   };

   void getvideo(int index, int &value) {
      ihwc.get()->getvideo(index, value);
   }

   void getsideband(int index, int &value) {
      ihwc.get()->getsideband(index, value);
   }

   void getgeometry(int type, int index,
                    struct hwc_position &frame, struct hwc_position &clipped,
                    int &zorder, int &visible) {
      ihwc.get()->getgeometry(type, index, frame, clipped, zorder, visible);
   }

   BcmSidebandBinder *get(void) {
      return ihwc.get();
   }
};

static BcmSidebandBinder_wrap *m_hwcBinder;

void BcmSidebandBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
   ALOGE( "%s: notify received: msg=%u", __FUNCTION__, msg);

   if (cb)
      cb(cb_data, msg, ntfy);
}

#define LOCKHAL() { \
ALOGE("%s: LOCKHAL", __FUNCTION__); \
BKNI_AcquireMutex(g_pTD->mutex); \
}

#define UNLOCKHAL() { \
ALOGE("%s: UNLOCKHAL", __FUNCTION__); \
BKNI_ReleaseMutex(g_pTD->mutex); \
}


static void TunerHALSidebandBinderNotify(int cb_data, int msg, struct hwc_notification_info &ntfy)
{
    struct bcmsideband_ctx *ctx = (struct bcmsideband_ctx *)cb_data;

    switch (msg)
    {
    case HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE:
    {
       BroadcastRect position, clip;
       position.x = ntfy.frame.x;
       position.y = ntfy.frame.y;
       position.w = ntfy.frame.w;
       position.h = ntfy.frame.h;
       clip.x = ntfy.clipped.x;
       clip.y = ntfy.clipped.y;
       clip.w = ntfy.clipped.w;
       clip.h = ntfy.clipped.h;
       ALOGE("%s: frame:{%d,%d,%d,%d}, clipped:{%d,%d,%d,%d}, display:{%d,%d}, zorder:%d", __FUNCTION__,
             position.x, position.y, position.w, position.h,
             clip.x, clip.y, clip.w, clip.h,
             ntfy.display_width, ntfy.display_height, ntfy.zorder);

       if (position.x > 0 && position.y == 0 && position.x + position.w == ntfy.display_width) {
           ALOGE("%s: applying workaround", __FUNCTION__);
           position.w -= position.x;
       }
       if (g_pTD->driver.SetGeometry) {
           LOCKHAL();
           g_pTD->driver.SetGeometry(position, clip, ntfy.display_width, ntfy.display_height, ntfy.zorder, true); 
           UNLOCKHAL();
       }
    }
    break;

    default:
        break;
    }
}

static void
HwcBinderConnect()
{
    // connect to the HWC binder.
    m_hwcBinder = new BcmSidebandBinder_wrap;
    if ( NULL == m_hwcBinder )
    {
        ALOGE("%s: Unable to connect to HwcBinder", __FUNCTION__);
        return;
    }
    m_hwcBinder->get()->register_notify(&TunerHALSidebandBinderNotify, (int)m_hwcBinder);
    ALOGE("%s: Connected to HwcBinder", __FUNCTION__);
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_initialize(JNIEnv *env, jclass /*thiz*/, jobject o)
{
    int rc;

    TV_LOG("%s: Initializing the Tuner stack!!", __FUNCTION__);

    b_refsw_client_client_configuration  config;

    g_pTD->ipcclient = NexusIPCClientFactory::getClient("TunerHAL");

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string), "TunerHAL");

    g_pTD->nexus_client = g_pTD->ipcclient->createClientContext(&config);

    if (g_pTD->nexus_client == NULL) {
        ALOGE("%s: createClientContext failed", __FUNCTION__);
        return -1;
    }

    ALOGE("%s: nexus_client = %p", __FUNCTION__, g_pTD->nexus_client);

    jclass cls = env->FindClass("com/broadcom/tvinput/TunerService");
    TV_LOG("%s: Found class (%p)!!", __FUNCTION__, cls);
    gTunerServiceClass = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
    g_pTD->o = env->NewGlobalRef(o);

    HwcBinderConnect();
    BKNI_CreateMutex(&g_pTD->mutex);
    return Broadcast_Initialize(&g_pTD->driver);
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_tune(JNIEnv *env, jclass /*thiz*/, jstring id)
{
    const char *s8id = env->GetStringUTFChars(id, NULL);
    jint rv = -1;

    TV_LOG("%s: Tuning to a new channel (%s)!!", __FUNCTION__, s8id);

    if (g_pTD->driver.Tune == 0) {
        TV_LOG("%s: Tune call is null", __FUNCTION__);
    }
    else {
        LOCKHAL();
        rv = (*g_pTD->driver.Tune)(String8(s8id)); 
        UNLOCKHAL();
    }

    env->ReleaseStringUTFChars(id, s8id);   
    return rv;
}

#define simpleField(name, sig) \
    jfieldID name##ID = env->GetFieldID(cls, #name, #sig); \
    if(name##ID == 0){ \
        ALOGE("%s: could not get %s ID", __FUNCTION__, #name); \
    }

#define booleanField(name) simpleField(name, Z)
#define shortField(name) simpleField(name, S)
#define floatField(name) simpleField(name, F)
#define stringField(name) simpleField(name, Ljava/lang/String;)

static void
setStringField(JNIEnv *env, jobject o, jfieldID id, const String8 &s8)
{
    jstring js = env->NewStringUTF(s8.string());
    env->SetObjectField(o, id, js);
    env->DeleteLocalRef(js);
}

JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getChannelList(JNIEnv *env, jclass /*thiz*/)
{
    TV_LOG("%s: Fetching channel list!!", __FUNCTION__);

    Vector<BroadcastChannelInfo> civ;

    if (g_pTD->driver.GetChannelList == 0) {
        TV_LOG("%s: GetChannelList call is null", __FUNCTION__);
        return 0;
    }

    LOCKHAL();
    civ = (*g_pTD->driver.GetChannelList)(); 
    UNLOCKHAL();

    jclass cls = env->FindClass("com/broadcom/tvinput/ChannelInfo"); 
    if (cls == 0) {
        ALOGE("%s: could not find class", __FUNCTION__);
        return 0;
    }
    jmethodID cID = env->GetMethodID(cls, "<init>", "()V");
    if(cID == 0){
        ALOGE("%s: could not get constructor ID", __FUNCTION__);
    }
    jfieldID idID = env->GetFieldID(cls, "id", "Ljava/lang/String;");
    if(idID == 0){
        ALOGE("%s: could not get id ID", __FUNCTION__);
    }
    jfieldID nameID = env->GetFieldID(cls, "name", "Ljava/lang/String;");
    if(nameID == 0){
        ALOGE("%s: could not get name ID", __FUNCTION__);
    }
    jfieldID numberID = env->GetFieldID(cls, "number", "Ljava/lang/String;");
    if(numberID == 0){
        ALOGE("%s: could not get number ID", __FUNCTION__);
    }
    jfieldID onidID = env->GetFieldID(cls, "onid", "I");
    if(onidID == 0){
        ALOGE("%s: could not get onid ID", __FUNCTION__);
    }
    jfieldID tsidID = env->GetFieldID(cls, "tsid", "I");
    if(tsidID == 0){
        ALOGE("%s: could not get tsid ID", __FUNCTION__);
    }
    jfieldID sidID = env->GetFieldID(cls, "sid", "I");
    if(sidID == 0){
        ALOGE("%s: could not get sid ID", __FUNCTION__);
    }

    stringField(logoUrl)

    jobjectArray rv = env->NewObjectArray(civ.size(), cls, NULL);
    if (rv == 0) {
        ALOGE("%s: could not create array", __FUNCTION__);
        return 0;
    }
    for (unsigned i = 0; i < civ.size(); i++) {
        jobject o = env->NewObject(cls, cID);

        setStringField(env, o, idID, civ[i].id);

        env->SetIntField(o, onidID, civ[i].onid);
        env->SetIntField(o, tsidID, civ[i].tsid);
        env->SetIntField(o, sidID, civ[i].sid);

        setStringField(env, o, nameID, civ[i].name);
        setStringField(env, o, numberID, civ[i].number);
        setStringField(env, o, logoUrlID, civ[i].logoUrl);

        env->SetObjectArrayElement(rv, i, o);
        env->DeleteLocalRef(o);
    }
    ALOGE("%s: ok so far", __FUNCTION__);
    return rv;
}

JNIEXPORT jobjectArray JNICALL
Java_com_broadcom_tvinput_TunerHAL_getProgramList(JNIEnv *env, jclass /*thiz*/, jstring id)
{
    TV_LOG("%s: Fetching program list!!", __FUNCTION__);

    Vector<BroadcastProgramInfo> piv;

    if (g_pTD->driver.GetProgramList == 0) {
        TV_LOG("%s: GetProgramList call is null", __FUNCTION__);
        return 0;
    }

    const char *s8id = env->GetStringUTFChars(id, NULL);

    LOCKHAL();
    piv = (*g_pTD->driver.GetProgramList)(String8(s8id));
    UNLOCKHAL();

    env->ReleaseStringUTFChars(id, s8id);
     
    jclass cls = env->FindClass("com/broadcom/tvinput/ProgramInfo"); 
    if (cls == 0) {
        ALOGE("%s: could not find class", __FUNCTION__);
        return 0;
    }
    jmethodID cID = env->GetMethodID(cls, "<init>", "()V");
    if(cID == 0){
        ALOGE("%s: could not get constructor ID", __FUNCTION__);
    }
    jfieldID idID = env->GetFieldID(cls, "id", "Ljava/lang/String;");
    if(idID == 0){
        ALOGE("%s: could not get id ID", __FUNCTION__);
    }
    jfieldID channelIdID = env->GetFieldID(cls, "channel_id", "Ljava/lang/String;");
    if(channelIdID == 0){
        ALOGE("%s: could not get channel_id ID", __FUNCTION__);
    }
    jfieldID titleID = env->GetFieldID(cls, "title", "Ljava/lang/String;");
    if(titleID == 0){
        ALOGE("%s: could not get title ID", __FUNCTION__);
    }
    jfieldID descID = env->GetFieldID(cls, "short_description", "Ljava/lang/String;");
    if(descID == 0){
        ALOGE("%s: could not get short_description ID", __FUNCTION__);
    }
    jfieldID startID = env->GetFieldID(cls, "start_time_utc_millis", "J");
    if(startID == 0){
        ALOGE("%s: could not get start ID", __FUNCTION__);
    }
    jfieldID endID = env->GetFieldID(cls, "end_time_utc_millis", "J");
    if(endID == 0){
        ALOGE("%s: could not get end ID", __FUNCTION__);
    }
    jobjectArray rv = env->NewObjectArray(piv.size(), cls, NULL);
    for (unsigned i = 0; i < piv.size(); i++) {
        jobject o = env->NewObject(cls, cID); 
        setStringField(env, o, idID, piv[i].id);
        setStringField(env, o, channelIdID, piv[i].channel_id);
        setStringField(env, o, titleID, piv[i].title);
        setStringField(env, o, descID, piv[i].short_description);
        env->SetLongField(o, startID, piv[i].start_time_utc_millis);
        env->SetLongField(o, endID, piv[i].end_time_utc_millis);
        env->SetObjectArrayElement(rv, i, o);
        env->DeleteLocalRef(o);
    }
    ALOGE("%s: ok so far", __FUNCTION__);
    return rv;
}

JNIEXPORT jobject JNICALL
Java_com_broadcom_tvinput_TunerHAL_getScanInfo(JNIEnv *env, jclass /*thiz*/)
{

    TV_LOG("%s: Fetching scan info!!", __FUNCTION__);

    BroadcastScanInfo si;

    if (g_pTD->driver.GetScanInfo == 0) {
        TV_LOG("%s: GetScanInfo call is null", __FUNCTION__);
        si.valid = false;
    }
    else {
        LOCKHAL();
        si = (*g_pTD->driver.GetScanInfo)();
        UNLOCKHAL();
    }
     
    jclass cls = env->FindClass("com/broadcom/tvinput/ScanInfo"); 
    if (cls == 0) {
        ALOGE("%s: could not find class", __FUNCTION__);
        return 0;
    }
    jmethodID cID = env->GetMethodID(cls, "<init>", "()V");
    if(cID == 0){
        ALOGE("%s: could not get constructor ID", __FUNCTION__);
    }

    booleanField(busy)
    booleanField(valid)
    shortField(channels)
    shortField(progress)
    shortField(TVChannels)
    shortField(dataChannels)
    shortField(radioChannels)
    shortField(signalStrengthPercent)
    shortField(signalQualityPercent)

    jobject o = env->NewObject(cls, cID); 
    env->SetBooleanField(o, busyID, si.busy);
    env->SetBooleanField(o, validID, si.valid);
    if (si.valid) {
        env->SetShortField(o, progressID, si.progress); 
        env->SetShortField(o, channelsID, si.channels);
        env->SetShortField(o, TVChannelsID, si.TVChannels);
        env->SetShortField(o, dataChannelsID, si.dataChannels);
        env->SetShortField(o, radioChannelsID, si.radioChannels);
        env->SetShortField(o, signalStrengthPercentID, si.signalStrengthPercent);
        env->SetShortField(o, signalQualityPercentID, si.signalQualityPercent);
    }
    ALOGE("%s: ok so far", __FUNCTION__);
    return o;
}

JNIEXPORT jlong JNICALL Java_com_broadcom_tvinput_TunerHAL_getUtcTime(JNIEnv */*env*/, jclass /*thiz*/)
{
    if (g_pTD->driver.GetUtcTime == 0) {
        TV_LOG("%s: GetUtcTime call is null", __FUNCTION__);
        return 0;
    }
    else {
        jlong t;
        LOCKHAL();
        t = (*g_pTD->driver.GetUtcTime)();
        UNLOCKHAL();
        return t;
    }
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_stop(JNIEnv */*env*/, jclass /*thiz*/)
{
    jint rv = -1;

    TV_LOG("%s: Stopping the current channel!!", __FUNCTION__);

    if (g_pTD->driver.Stop == 0) {
        TV_LOG("%s: Stop call is null", __FUNCTION__);
    }
    else {
        LOCKHAL();
        rv = (*g_pTD->driver.Stop)();
        UNLOCKHAL();
    }

    return rv;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_startBlindScan(JNIEnv */*env*/, jclass /*thiz*/)
{
    jint rv = -1;

    if (g_pTD->driver.StartBlindScan == 0) {
        TV_LOG("%s: StartBlindScan call is null", __FUNCTION__);
    }
    else {
        LOCKHAL();
        rv = (*g_pTD->driver.StartBlindScan)();
        UNLOCKHAL();
    }

    return rv;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_stopScan(JNIEnv */*env*/, jclass /*thiz*/)
{
    jint rv = -1;

    if (g_pTD->driver.StopScan == 0) {
        TV_LOG("%s: StopScan call is null", __FUNCTION__);
    }
    else {
        LOCKHAL();
        rv = (*g_pTD->driver.StopScan)();
        UNLOCKHAL();
    }

    return rv;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_release(JNIEnv */*env*/, jclass /*thiz*/)
{
    jint rv = -1;

    TV_LOG("%s: Releasing!!", __FUNCTION__);

    if (g_pTD->driver.Release == 0) {
        TV_LOG("%s: Release call is null", __FUNCTION__);
    }
    else {
        LOCKHAL();
        rv = (*g_pTD->driver.Release)();
        UNLOCKHAL();
    }

    return rv;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_setSurface(JNIEnv */*env*/, jclass /*thiz*/)
{
    jint rv = -1;

    TV_LOG("%s: Setting surface!!", __FUNCTION__);

    if (m_hwcBinder) {
        int sbsi;
        m_hwcBinder->getsideband(0, sbsi);
        rv = 0;
    }

    return rv;
}

JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getVideoTrackInfoList(JNIEnv *env, jclass /*thiz*/)
{
    TV_LOG("%s: Fetching video track info list!!", __FUNCTION__);

    Vector<BroadcastVideoTrackInfo> vtiv;

    if (g_pTD->driver.GetVideoTrackInfoList == 0) {
        TV_LOG("%s: GetVideoTrackInfoList call is null", __FUNCTION__);
        return 0;
    }

    LOCKHAL();
    vtiv = (*g_pTD->driver.GetVideoTrackInfoList)(); 
    UNLOCKHAL();

    jclass cls = env->FindClass("com/broadcom/tvinput/VideoTrackInfo"); 
    if (cls == 0) {
        ALOGE("%s: could not find class", __FUNCTION__);
        return 0;
    }
    jmethodID cID = env->GetMethodID(cls, "<init>", "()V");
    if(cID == 0){
        ALOGE("%s: could not get constructor ID", __FUNCTION__);
    }

    stringField(id)
    shortField(squarePixelWidth)
    shortField(squarePixelHeight)
    floatField(frameRate)

    jobjectArray rv = env->NewObjectArray(vtiv.size(), cls, NULL);
    for (unsigned i = 0; i < vtiv.size(); i++) {
        jobject o = env->NewObject(cls, cID); 
        setStringField(env, o, idID, vtiv[i].id);
        env->SetShortField(o, squarePixelWidthID, vtiv[i].squarePixelWidth);
        env->SetShortField(o, squarePixelHeightID, vtiv[i].squarePixelHeight);
        env->SetFloatField(o, frameRateID, vtiv[i].frameRate);
        env->SetObjectArrayElement(rv, i, o);
        env->DeleteLocalRef(o);
    }
    ALOGE("%s: ok so far", __FUNCTION__);
    return rv;
}

void
TunerHAL_onBroadcastEvent(jint e, jint param, String8 s)
{
    if (g_pTD && g_pTD->vm && g_pTD->o) {
        JNIEnv *env;
        jstring js;
        g_pTD->vm->AttachCurrentThread(&env, NULL);
        jmethodID obeID = env->GetMethodID(gTunerServiceClass, "onBroadcastEvent", "(IILjava/lang/String;)V");
        js = env->NewStringUTF(s.string());
        env->CallVoidMethod(g_pTD->o, obeID, e, param, js);
        env->DeleteLocalRef(js);
        //g_pTD->vm->DetachCurrentThread();
    }
}

NexusIPCClientBase *
TunerHAL_getIPCClient()
{
    return g_pTD->ipcclient;
}

NexusClientContext *
TunerHAL_getClientContext()
{
    return g_pTD->nexus_client;
}

