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
 * $brcm_Workfile: jni_adjustScreenOffset.cpp $
 * $brcm_Revision: 2 $
 * $brcm_Date: 12/3/12 3:19p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/app/BcmAdjustScreenOffset/jni/jni_adjustScreenOffset.cpp $
 * 
 * 2   12/3/12 3:19p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 *****************************************************************************/
#include <com_android_bcmnativeplayer_native_BcmNativePlayer.h>

#include <cutils/memory.h>
#include <cutils/properties.h>
#include <utils/Log.h>

#include <stdlib.h>
#include <stdio.h>

#include "media_player.h"

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

using namespace android;

static JNINativeMethod gMethods[] = {
    {"start", "(Ljava/lang/String;)I", (void *)Java_com_android_bcmnativeplayer_native_1BcmNativePlayer_start},
    {"stop",  "()I", (void *)Java_com_android_bcmnativeplayer_native_1BcmNativePlayer_stop},
};

static media_player_t player = NULL;

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

static int register_BcmNativePlayer_jni(JNIEnv *env)
{
    return registerNativeMethods(env,"com/android/bcmnativeplayer/native_BcmNativePlayer", gMethods,  sizeof(gMethods) / sizeof(gMethods[0]));
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

    if (register_BcmNativePlayer_jni(env) < 0)
    {
        LOGE("ERROR: register BcmNativePlayer interface error failed\n");
        return result;
    }

    result = JNI_VERSION_1_4;
    return result;
}

JNIEXPORT jint JNICALL Java_com_android_bcmnativeplayer_native_1BcmNativePlayer_start
(JNIEnv *env, jobject obj, jstring uri)
{
	jboolean isCopy;
    const char *nativeBytes;

    nativeBytes = env->GetStringUTFChars(uri, &isCopy);
    // Print the uri string
    if (nativeBytes != NULL)
    {
        ALOGE("JNI_setDataSource: uri = %s, strlen = %d", nativeBytes, strlen(nativeBytes));
    }
    else
    {
        ALOGE("JNI_setDataSource: uri is NULL!!");
    }

    player = media_player_create(NULL);
    if (!player) return -1;

    media_player_start_settings start_settings;
    media_player_get_default_start_settings(&start_settings);
    start_settings.stream_url = nativeBytes;
    int rc = media_player_start(player, &start_settings);
    env->ReleaseStringUTFChars(uri, nativeBytes);

    return (jint)rc;
}

JNIEXPORT jint JNICALL Java_com_android_bcmnativeplayer_native_1BcmNativePlayer_stop
(JNIEnv *env, jobject obj)
{
    int bogus;
    LOGI("stop\n");

    if (player) {
        media_player_stop(player);
        media_player_destroy(player);
        player = NULL;
    }

    return (jint)bogus;
}


