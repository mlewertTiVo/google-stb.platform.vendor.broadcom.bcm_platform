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
#include <jni.h>

#include <cutils/memory.h>
#include <cutils/properties.h>

#include <utils/Log.h>
/*
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <string.h>
#include <cutils/atomic.h>
*/
#include  <stdlib.h>
#include  <stdio.h>

#include "nexus_platform.h"
#include "nexus_display.h"

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "nexus_interface.h"
#include "nexusservice.h"

#include "nexus_surface_client.h"
#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
#include "nexus_ipc_client_factory.h"
#else
#include "nexus_ipc_client.h"
#endif

using namespace android;

static void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_setScreenOffset(JNIEnv *env, jobject thisobj, jobject offset);

static void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_getScreenOffset(JNIEnv *env, jobject thisobj, jobject offset);

static JNINativeMethod gMethods[] = {

        {"setScreenOffset",   "(Landroid/graphics/Rect;)V", (void *)Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_setScreenOffset},

        {"getScreenOffset",   "(Landroid/graphics/Rect;)V", (void *)Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_getScreenOffset}
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




static int register_adjustScreenOffset_jni(JNIEnv *env){

    return registerNativeMethods(env,"com/android/adjustScreenOffset/native_adjustScreenOffset", gMethods,  sizeof(gMethods) / sizeof(gMethods[0]));
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    BSTD_UNUSED(reserved);

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("ERROR: GetEnv failed\n");
        return result;
    }
    assert(env != NULL);


    if (register_adjustScreenOffset_jni(env) < 0)
    {
        LOGE("ERROR: register adjustScreenOffset interface error failed\n");
        return result;
    }


    result = JNI_VERSION_1_4;

    return result;
}


JNIEXPORT void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_setScreenOffset
  (JNIEnv *env, jobject thisobj, jobject offset)
{
    BSTD_UNUSED(thisobj);

    jclass offset_class = env->GetObjectClass(offset);

    jfieldID left_id = env->GetFieldID(offset_class, "left", "I");
    if (left_id == NULL) return;
    jint left = env->GetIntField(offset, left_id);

    jfieldID right_id = env->GetFieldID(offset_class, "right", "I");
    if (right_id == NULL) return;
    jint right = env->GetIntField(offset, right_id);

    jfieldID top_id = env->GetFieldID(offset_class, "top", "I");
    if (top_id == NULL) return;
    jint top = env->GetIntField(offset, top_id);

    jfieldID bottom_id = env->GetFieldID(offset_class, "bottom", "I");
    if (bottom_id == NULL) return;
    jint bottom = env->GetIntField(offset, bottom_id);

#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("adjustScreenOffset");
#else
    NexusIPCClient *pIpcClient = new NexusIPCClient;
#endif
    NEXUS_SurfaceComposition composition;

    pIpcClient->getClientComposition(NULL, &composition);

    LOGE("Changing composition [x,y,w,h] = [%d, %d, %d, %d] ----> [%d, %d, %d, %d] \n",
        composition.position.x, composition.position.y,
        composition.position.width, composition.position.height,
        left, top, right - left, bottom - top);

    composition.position.x = left;
    composition.position.y = top;
    composition.position.width = right - left;
    composition.position.height = bottom - top;

    pIpcClient->setClientComposition(NULL, &composition);

    delete pIpcClient;
}

JNIEXPORT void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_getScreenOffset
  (JNIEnv *env, jobject thisobj, jobject offset)
{
    BSTD_UNUSED(thisobj);

#ifdef ANDROID_SUPPORTS_NEXUS_IPC_CLIENT_FACTORY
    NexusIPCClientBase *pIpcClient = NexusIPCClientFactory::getClient("adjustScreenOffset");
#else
    NexusIPCClient *pIpcClient = new NexusIPCClient;
#endif
    NEXUS_SurfaceComposition composition;

    pIpcClient->getClientComposition(NULL, &composition);

    LOGE("get [%d, %d, %d, %d]  \n",
        composition.position.x, composition.position.y,
        composition.position.width, composition.position.height);

    jint left = composition.position.x;
    jint top = composition.position.y;
    jint right = composition.position.x + composition.position.width;
    jint bottom = composition.position.y + composition.position.height;
    delete pIpcClient;

    jclass offset_class = env->GetObjectClass(offset);

    jfieldID left_id = env->GetFieldID(offset_class, "left", "I");
    if (left_id == NULL) return;
    env->SetIntField(offset, left_id, left);

    jfieldID right_id = env->GetFieldID(offset_class, "right", "I");
    if (right_id == NULL) return;
    env->SetIntField(offset, right_id, right);

    jfieldID top_id = env->GetFieldID(offset_class, "top", "I");
    if (top_id == NULL) return;
    env->SetIntField(offset, top_id, top);

    jfieldID bottom_id = env->GetFieldID(offset_class, "bottom", "I");
    if (bottom_id == NULL) return;
    env->SetIntField(offset, bottom_id, bottom);
}
