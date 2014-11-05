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

// Enable this for debug prints
#define DEBUG_JNI

#ifdef DEBUG_JNI
#define TV_LOG	ALOGE
#else
#define TV_LOG
#endif

static struct {
    int id;
    char *number;
    char *name;
    int onid;
    int tsid;
    int sid;
    int freqKHz;
    int vpid;
} lineup[] = {
    { 0, "8", "8madrid", 0x22d4, 0x0027, 0x0f3d, 577000, 0x100 },
    { 1, "13", "13tv Madrid", 0x22d4, 0x0027, 0x0f3e, 577000, 0x200 },
    { 2, "800", "ASTROCANAL SHOP", 0x22d4, 0x0027, 0x0f43, 577000, 0x700 },
    { 3, "801", "Kiss TV", 0x22d4, 0x0027, 0x0f40, 577000, 0x401 },
    { 4, "802", "INTER TV", 0x22d4, 0x0027, 0x0f3f, 577000, 0x300 },
    { 5, "803", "MGustaTV", 0x22d4, 0x0027, 0x1392, 577000, 0x1000 },
    { -1 }
};

// All globals must be JNI primitives, any other data type 
// will fail to hold its value across different JNI methods
void *j_main;
PTuner_Data g_pTD;

// The signature syntax is: Native-method-name, signature & fully-qualified-name
static JNINativeMethod gMethods[] = 
{
	{"initialize",     "()I",     (void *)Java_com_broadcom_tvinput_TunerHAL_initialize},
	{"tune",           "(I)I",     (void *)Java_com_broadcom_tvinput_TunerHAL_tune},
	{"getChannelList", "()[Lcom/broadcom/tvinput/ChannelInfo;",     (void *)Java_com_broadcom_tvinput_TunerHAL_getChannelList},
    {"stop",           "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_stop},  
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

jint JNI_OnLoad(JavaVM* vm, void* reserved)
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
    g_pTD = (PTuner_Data) malloc(sizeof(Tuner_Data));

	// Launch the binder service
    NexusTunerService::instantiate();
    ALOGE("%s: NexusTunerService is now ready", __FUNCTION__);
//    IPCThreadState::self()->joinThreadPool();

    return result;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_initialize(JNIEnv *env, jclass thiz)
{
    int rc;

    TV_LOG("%s: Initializing the Tuner stack!!", __FUNCTION__);

    rc = Broadcast_Initialize();

    return rc;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_tune(JNIEnv *env, jclass thiz, jint channel_id)
{
    TV_LOG("%s: Tuning to a new channel (%d)!!", __FUNCTION__, channel_id);

    Broadcast_Tune(channel_id);

    return 0;
}

JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getChannelList(JNIEnv *env, jclass thiz)
{
    TV_LOG("%s: Fetching channel list!!", __FUNCTION__);

    Vector<ChannelInfo> civ;

    civ = Broadcast_GetChannelList();

    jclass cls = env->FindClass("com/broadcom/tvinput/ChannelInfo"); 
    if (cls == 0) {
        ALOGE("%s: could not find class", __FUNCTION__);
        return 0;
    }
    jmethodID cID = env->GetMethodID(cls, "<init>", "()V");
    if(cID == 0){
        ALOGE("%s: could not get constructor ID", __FUNCTION__);
    }
    jfieldID idID = env->GetFieldID(cls, "id", "I");
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
    jobjectArray rv = env->NewObjectArray(civ.size(), cls, NULL);
    if (rv == 0) {
        ALOGE("%s: could not create array", __FUNCTION__);
        return 0;
    }
    for (unsigned i = 0; i < civ.size(); i++) {
        jobject o = env->NewObject(cls, cID); 
        env->SetIntField(o, idID, civ[i].id);
        env->SetIntField(o, onidID, civ[i].onid);
        env->SetIntField(o, tsidID, civ[i].tsid);
        env->SetIntField(o, sidID, civ[i].sid);
        env->SetObjectField(o, nameID, env->NewStringUTF(civ[i].name.string()));
        env->SetObjectField(o, numberID, env->NewStringUTF(civ[i].number.string()));
        env->SetObjectArrayElement(rv, i, o);
    }
    ALOGE("%s: ok so far", __FUNCTION__);
    return rv;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_stop(JNIEnv *env, jclass thiz)
{
    TV_LOG("%s: Stopping the current channel!!", __FUNCTION__);

    Broadcast_Stop();

    return 0;
}

static int Broadcast_Initialize()
{
    NEXUS_FrontendAcquireSettings frontendAcquireSettings;

    ALOGE("%s: Enter", __FUNCTION__);

    b_refsw_client_client_configuration         config;
    b_refsw_client_client_info                  client_info;
    b_refsw_client_connect_resource_settings    connectSettings;

    g_pTD->ipcclient= NexusIPCClientFactory::getClient("TunerHAL");

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string), "TunerHAL");

    config.resources.screen.required = true;
    config.resources.audioDecoder = false;
    config.resources.audioPlayback = false;
    config.resources.videoDecoder = true;

    g_pTD->nexus_client = g_pTD->ipcclient->createClientContext(&config);

    if (g_pTD->nexus_client == NULL)
        ALOGE("%s: createClientContext failed", __FUNCTION__);

    ALOGE("%s: nexus_client = %p", __FUNCTION__, g_pTD->nexus_client);

    g_pTD->videoDecoder = g_pTD->ipcclient->acquireVideoDecoderHandle();

    g_pTD->ipcclient->getClientInfo(g_pTD->nexus_client, &client_info);

    // Now connect the client resources
    g_pTD->ipcclient->getDefaultConnectClientSettings(&connectSettings);

    connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
    connectSettings.simpleVideoDecoder[0].windowId = 0;

    connectSettings.simpleVideoDecoder[0].windowCaps.type = eVideoWindowType_eMain;

    if (true != g_pTD->ipcclient->connectClientResources(g_pTD->nexus_client, &connectSettings))
        ALOGE("%s: connectClientResources failed", __FUNCTION__);

    b_video_window_settings window_settings;
    g_pTD->ipcclient->getVideoWindowSettings(g_pTD->nexus_client, 0, &window_settings);
    window_settings.visible = true;
    g_pTD->ipcclient->setVideoWindowSettings(g_pTD->nexus_client, 0, &window_settings);

    g_pTD->stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    g_pTD->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
    if (!g_pTD->stcChannel || !g_pTD->parserBand)
    {
        ALOGE("%s: Unable to create stcChannel or parserBand", __FUNCTION__);
        return -1;
    }

    NEXUS_Frontend_GetDefaultAcquireSettings(&frontendAcquireSettings);
    frontendAcquireSettings.capabilities.ofdm = true;
    g_pTD->frontend = NEXUS_Frontend_Acquire(&frontendAcquireSettings);
    if (!g_pTD->frontend)
    {
        ALOGE("%s: Unable to find OFDM-capable frontend", __FUNCTION__);
        return -1;
    }

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}

static int Broadcast_Tune(int channel_id)
{
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_FrontendOfdmSettings ofdmSettings;
    NEXUS_FrontendUserParameters userParams;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_VideoCodec video_codec = NEXUS_VideoCodec_eMpeg2;
    NEXUS_Error rc;
    int video_pid;

    unsigned i;
    for (i = 0; lineup[i].id >= 0; i++) {
        if (lineup[i].id == channel_id) {
            break;
        }
    }

    if (lineup[i].id < 0) {
        ALOGE("%s: channel_id %d invalid", __FUNCTION__, channel_id);
        return -1;
    }

    // Enable the tuner
    ALOGE("%s: Tuning on frequency %d...", __FUNCTION__, lineup[i].freqKHz);

    NEXUS_Frontend_GetDefaultOfdmSettings(&ofdmSettings);
    ofdmSettings.frequency = lineup[i].freqKHz * 1000;
    ofdmSettings.acquisitionMode = NEXUS_FrontendOfdmAcquisitionMode_eAuto;
    ofdmSettings.terrestrial = true;
    ofdmSettings.spectrum = NEXUS_FrontendOfdmSpectrum_eAuto;
    ofdmSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;
    NEXUS_Frontend_GetUserParameters(g_pTD->frontend, &userParams);
    NEXUS_ParserBand_GetSettings(g_pTD->parserBand, &parserBandSettings);

    if (userParams.isMtsif)
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eMtsif;
        parserBandSettings.sourceTypeSettings.mtsif = NEXUS_Frontend_GetConnector(g_pTD->frontend); /* NEXUS_Frontend_TuneXyz() will connect this frontend to this parser band */
    }

    else
    {
        parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
        parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
    }

    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    rc = NEXUS_ParserBand_SetSettings(g_pTD->parserBand, &parserBandSettings);

    if (rc)
    {
        ALOGE("%s: ParserBand Setting failed", __FUNCTION__);
        return -1;
    }

    rc = NEXUS_Frontend_TuneOfdm(g_pTD->frontend, &ofdmSettings);
    if (rc)
    {
        ALOGE("%s: Frontend TuneOfdm failed", __FUNCTION__);
        return -1;
    }

    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);

    // Set up the video PID
    video_pid = lineup[i].vpid;

    videoProgram.settings.pidChannel = NEXUS_PidChannel_Open(g_pTD->parserBand, video_pid, NULL);
    videoProgram.settings.codec = video_codec;

    if (videoProgram.settings.pidChannel)
    {
        rc = NEXUS_SimpleVideoDecoder_SetStcChannel(g_pTD->videoDecoder, g_pTD->stcChannel);
        if (rc)
        {
            ALOGE("%s: SetStcChannel failed", __FUNCTION__);
            return -1;
        }
    }

    if (videoProgram.settings.pidChannel)
    {
        g_pTD->pidChannel = videoProgram.settings.pidChannel;

        rc = NEXUS_SimpleVideoDecoder_Start(g_pTD->videoDecoder, &videoProgram);
        if (rc)
        {
            ALOGE("%s: VideoDecoderStart failed", __FUNCTION__);
            return -1;
        }
    }

    ALOGE("%s: Tuner has started streaming!!", __FUNCTION__);
    return 0;
}

static Vector<ChannelInfo> Broadcast_GetChannelList()
{
    Vector<ChannelInfo> civ;
    ChannelInfo ci;
    unsigned i;
    for (i = 0; lineup[i].id >= 0; i++) {
        ci.id = lineup[i].id; 
        ci.name = lineup[i].name;
        ci.number = lineup[i].number;
        ci.onid = lineup[i].onid;
        ci.tsid = lineup[i].tsid;
        ci.sid = lineup[i].sid;
        civ.push_back(ci);
    }
    return civ;
}

static int Broadcast_Stop()
{
    // Stop the video decoder
    NEXUS_SimpleVideoDecoder_Stop(g_pTD->videoDecoder);

    // Close the PID channel
    NEXUS_PidChannel_Close(g_pTD->pidChannel);

    return 0;
}