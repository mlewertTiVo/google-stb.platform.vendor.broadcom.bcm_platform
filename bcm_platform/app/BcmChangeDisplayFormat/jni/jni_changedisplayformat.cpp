/******************************************************************************
 *    (c)2012 Broadcom Corporation
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
 * $brcm_Workfile: jni_changedisplayformat.cpp $
 * $brcm_Revision: 2 $
 * $brcm_Date: 12/3/12 3:19p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/app/BcmChangeDisplayFormat/jni/jni_changedisplayformat.cpp $
 * 
 * 2   12/3/12 3:19p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 *****************************************************************************/
//#define LOG_NDEBUG 0
#include <jni.h>

#include <cutils/memory.h>

#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "nexus_interface.h"
#include "nexusservice.h"
#include "nexus_core_utils.h"
#include "nexus_surface_client.h"

#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
#include "nexus_ipc_client_factory.h"
#define GET_DISPLAY_SETTINGS(id, settings) getDisplaySettings((id), (settings))
#define SET_DISPLAY_SETTINGS(id, settings) setDisplaySettings((id), (settings))
#else
#include "nexus_ipc_client.h"
#define GET_DISPLAY_SETTINGS(id, settings) getDisplaySettings(NULL, (id), (settings))
#define SET_DISPLAY_SETTINGS(id, settings) setDisplaySettings(NULL, (id), (settings))
#endif

using namespace android;

static jint JNICALL Java_com_android_changedisplayformat_native_1changedisplayformat_setDisplayFormat(JNIEnv *env, jobject obj, jint displayFormat);

static jint JNICALL Java_com_android_changedisplayformat_native_1changedisplayformat_getDisplayFormat(JNIEnv *env, jobject obj);

static JNINativeMethod gMethods[] = {

        {"setDisplayFormat",   "(I)I", (void *)Java_com_android_changedisplayformat_native_1changedisplayformat_setDisplayFormat},
        {"getDisplayFormat",   "()I", (void *)Java_com_android_changedisplayformat_native_1changedisplayformat_getDisplayFormat}
};

#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
static NexusIPCClientBase *gIpcClient=NULL;
#else
static NexusIPCClient     *gIpcClient=NULL;
#endif

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

static int register_changedisplayformat_jni(JNIEnv *env){

    return registerNativeMethods(env,"com/android/changedisplayformat/native_changedisplayformat", gMethods,  sizeof(gMethods) / sizeof(gMethods[0]));
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


    if (register_changedisplayformat_jni(env) < 0)
    {
        LOGE("ERROR: register changedisplayformat interface error failed\n");
        return result;
    }

#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
    gIpcClient = NexusIPCClientFactory::getClient("changeDisplayFormat");
#else
    gIpcClient = new NexusIPCClient;
#endif

    result = JNI_VERSION_1_4;

    return result;
}

JNIEXPORT jint JNICALL Java_com_android_changedisplayformat_native_1changedisplayformat_setDisplayFormat
  (JNIEnv *env, jobject obj, jint displayFormat)
{
    NEXUS_DisplaySettings displaySettings, displaySettings_sd;

    gIpcClient->GET_DISPLAY_SETTINGS(0, &displaySettings);

    switch(displayFormat)
    {
        case 0:
            displaySettings.format = NEXUS_VideoFormat_e720p50hz;
            break;
        case 1:
            displaySettings.format = NEXUS_VideoFormat_e720p;
            break;
        case 2:
            displaySettings.format = NEXUS_VideoFormat_e1080i50hz;
            break;
        case 3:
            displaySettings.format = NEXUS_VideoFormat_e1080i;
            break;
        case 4:
            displaySettings.format = NEXUS_VideoFormat_e1080p50hz;
            break;
        case 5:
            displaySettings.format = NEXUS_VideoFormat_e1080p;	
            break;
        case 6:
            displaySettings.format = NEXUS_VideoFormat_e720p_3DOU_AS;
            break;
        case 7:
            displaySettings.format = NEXUS_VideoFormat_e1080p24hz;
            break;
        case 8:
            displaySettings.format = NEXUS_VideoFormat_e1080p24hz_3DOU_AS;
            break;
        case 9:
            displaySettings.format = NEXUS_VideoFormat_e3840x2160p30hz;
            break;

        default:
            displaySettings.format = NEXUS_VideoFormat_e720p;
    }

    gIpcClient->SET_DISPLAY_SETTINGS(0, &displaySettings);	

    /* sd display format should be changed based on hd display format 50Hz or 60Hz */
    gIpcClient->GET_DISPLAY_SETTINGS(1, &displaySettings_sd);	   
    switch (displaySettings.format)
    {
        case NEXUS_VideoFormat_e720p50hz:
        case NEXUS_VideoFormat_e1080i50hz:
        case NEXUS_VideoFormat_e1080p50hz:
            displaySettings_sd.format = NEXUS_VideoFormat_ePal;
            break;
        case NEXUS_VideoFormat_e720p:
        case NEXUS_VideoFormat_e1080i:
        case NEXUS_VideoFormat_e1080p:
        case NEXUS_VideoFormat_e720p_3DOU_AS:
        case NEXUS_VideoFormat_e1080p24hz:
        case NEXUS_VideoFormat_e1080p24hz_3DOU_AS:
        case NEXUS_VideoFormat_e3840x2160p30hz:
            displaySettings_sd.format = NEXUS_VideoFormat_eNtsc;
            break;
        default:
            displaySettings_sd.format = NEXUS_VideoFormat_eNtsc;
    }
    gIpcClient->SET_DISPLAY_SETTINGS(1, &displaySettings_sd);

    return 0;
}


JNIEXPORT jint JNICALL Java_com_android_changedisplayformat_native_1changedisplayformat_getDisplayFormat
  (JNIEnv *env, jobject obj)
{
	jint format = 0;
    NEXUS_DisplaySettings displaySettings;

    gIpcClient->GET_DISPLAY_SETTINGS(0, &displaySettings);

	LOGE("Got DisplaySettings from the server... displayFormat %d", displaySettings.format);

	switch(displaySettings.format)
	{
	    case NEXUS_VideoFormat_e720p50hz:
		    format = 0;
		    break;
	    case NEXUS_VideoFormat_e720p:
		    format = 1;
		    break;
	    case NEXUS_VideoFormat_e1080i50hz:
		    format = 2;
		    break;
	    case NEXUS_VideoFormat_e1080i:
		    format = 3;
		    break;
	    case NEXUS_VideoFormat_e1080p50hz:
		    format = 4;
		    break;
	    case NEXUS_VideoFormat_e1080p:
		    format = 5;
		    break;
	    case NEXUS_VideoFormat_e720p_3DOU_AS:
		    format = 6;
		    break;
	    case NEXUS_VideoFormat_e1080p24hz:
		    format = 7;
		    break;
	    case NEXUS_VideoFormat_e1080p24hz_3DOU_AS:
		    format = 8;
		    break;
	    case NEXUS_VideoFormat_e3840x2160p30hz:
		    format = 9;
		    break;

	    default:
		    format = 1;
	}

	return (jint)format;
}

