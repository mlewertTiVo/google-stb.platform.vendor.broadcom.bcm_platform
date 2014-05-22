/******************************************************************************
 *    (c)2013 Broadcom Corporation
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
 * $brcm_Workfile: BcmFP_jni.cpp $
 * $brcm_Revision: 1 $
 * $brcm_Date: 2/26/13 11:15a $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/app/BcmFlashPlayer/jni/BcmFP_jni.cpp $
 * 
 * 1   2/26/13 11:15a hvasudev
 * SWANDROID-342: This app will initiate the switch to the Trellis side
 *  (it opens youtube.com/xl by default)
 * 
 *****************************************************************************/
#include <jni.h>
#include <utils/Log.h>
#include <assert.h>

#include "osapi.h"
#include "Defs.h"
#include "Window.h"
#include "BAMClient.h"

//using namespace android;
using namespace Trellis;
using namespace Trellis::Application;

static jint JNICALL Java_com_broadcom_flashplayer_BcmFlashPlayer_JNI_switchToTrellis(JNIEnv *env, jobject obj);

static JNINativeMethod gMethods[] = 
{
    {"switchToTrellis", "()I", (void *)Java_com_broadcom_flashplayer_BcmFlashPlayer_JNI_switchToTrellis},
};

static int registerNativeMethods(JNIEnv* env, const char* className, JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) 
    {
        LOGE("libbcmfp_jni: Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }

    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) 
    {
        LOGE("libbcmfp_jni: RegisterNatives failed for '%s'", className);
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
        LOGE("libbcmfp_jni: GetEnv failed");
        return result;
    }
    assert(env != NULL);
        
    LOGE("libbcmfp_jni: GetEnv succeeded!!");
    if (registerNativeMethods(env, "com/broadcom/flashplayer/BcmFlashPlayer_JNI", gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0)
    {
        LOGE("libbcmfp_jni: register interface error!!");
        return result;
    }
    LOGE("libbcmfp_jni: register interface succeeded!!");

    result = JNI_VERSION_1_4;

    return result;
}

JNIEXPORT jint JNICALL Java_com_broadcom_flashplayer_BcmFlashPlayer_JNI_switchToTrellis(JNIEnv *env, jobject obj)
{
    BAMClient *bamClient;

    LOGE("libbcmfp_jni: Into switchToTrellis");

    bamClient = (BAMClient *)BAMClient::create(BAMClient::RPCType);
    if (bamClient == NULL) 
    {
        LOGE("libbcmfp_jni: BAMClient create failed!!");
    }

    bamClient->init();

    LOGE("libbcmfp_jni: launching Trellis Browser!!");
//    bamClient->launch(IWindowManager::LaunchParameters("", IWindowManager::APPLICATION_TYPE_WEBAPP, "http://www.google.com", "", ISurfaceManagement::CreationSettings(0, ISurfaceCompositing::Dimensions(1280, 720), false, 255, 11, ISurfaceCompositing::Rect(0, 0, 1280, 720), false, false, false), true, false));
    bamClient->launch(IWindowManager::LaunchParameters("", IWindowManager::APPLICATION_TYPE_WEBAPP, "http://www.youtube.com/xl", "", ISurfaceManagement::CreationSettings(0, ISurfaceCompositing::Dimensions(1280, 720), false, 255, 11, ISurfaceCompositing::Rect(0, 0, 1280, 720), false, false, false), true, false));

    return 1;
}
