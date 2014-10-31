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

// All globals must be JNI primitives, any other data type 
// will fail to hold its value across different JNI methods
void *j_main;
PTuner_Data g_pTD;

// The signature syntax is: Native-method-name, signature & fully-qualified-name
static JNINativeMethod gMethods[] = 
{
	{"initialize",  "(I)I",     (void *)Java_com_broadcom_tvinput_TunerHAL_initialize},
	{"tune",        "(I)I",     (void *)Java_com_broadcom_tvinput_TunerHAL_tune},
    {"stop",        "()I",      (void *)Java_com_broadcom_tvinput_TunerHAL_stop},    
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
        case API_OVER_BINDER:
        {
            int rc = -1;
            api_data cmd;
            data.read(&cmd, sizeof(api_data));
            ALOGE("NexusTunerService::onTransact, cmd.api = 0x%x", cmd.api);

            switch (cmd.api)
            {
                case api_get_client_context:
                    ALOGE("NexusTunerService::onTransact, case api_get_client_context");
                    cmd.param.clientContext.out.nexus_client = g_pTD->nexus_client;
                    rc = 0;
                break;

                default:
                    ALOGE("NexusTunerService::onTransact, invalid api!!");
                break;
            }

            if (!rc)
            {
                ALOGE("NexusTunerService::onTransact, Writing the output for api-code = 0x%x", code);
                reply->write(&cmd.param, sizeof(cmd.param));
                return NO_ERROR;
            }

            else
            {
                ALOGE("NexusTunerService::onTransact, FAILED_TRANSACTION for api-code = 0x%x", code);
                return FAILED_TRANSACTION;
            }
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

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_initialize(JNIEnv *env, jclass thiz, jint freq)
{
    int rc;

    TV_LOG("%s: Initializing the Tuner stack on frequency %d!!", __FUNCTION__, freq);

    g_pTD->freq = freq;
    rc = Broadcast_Initialize();

    return rc;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_tune(JNIEnv *env, jclass thiz, jint channel_id)
{
    TV_LOG("%s: Tuning to a new channel (%d)!!", __FUNCTION__, channel_id);

    Broadcast_Tune(channel_id);

    return 0;
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

    // Enable the tuner
    ALOGE("%s: Tuning on frequency %d...", __FUNCTION__, g_pTD->freq);

    NEXUS_Frontend_GetDefaultOfdmSettings(&ofdmSettings);
    ofdmSettings.frequency = g_pTD->freq * 1000000;
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
    (channel_id == 1) ? (video_pid = VIDEO_PID_1) : (video_pid = VIDEO_PID_2);

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

static int Broadcast_Stop()
{
    // Stop the video decoder
    NEXUS_SimpleVideoDecoder_Stop(g_pTD->videoDecoder);

    // Close the PID channel
    NEXUS_PidChannel_Close(g_pTD->pidChannel);

    return 0;
}