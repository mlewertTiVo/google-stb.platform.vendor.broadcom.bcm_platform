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

using namespace android;

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

// The signature syntax is: Native-method-name, input-params, output-params & local-definition
static JNINativeMethod gMethods[] = 
{
	{"initialize",  "(I)I",      (void *)Java_com_broadcom_tvinput_TunerHAL_initialize},
	{"tune",        "(I)I",      (void *)Java_com_broadcom_tvinput_TunerHAL_tune},
};

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

	// Initiate a channel scan
	TV_LOG("%s: Initiating a channel scan!!", __FUNCTION__);

    return result;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_initialize(JNIEnv *env, jclass thiz, jint freq)
{
    TV_LOG("%s: Initializing the Tuner stack!!", __FUNCTION__);

    return 0;
}

JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_tune(JNIEnv *env, jclass thiz, jint channel)
{
    TV_LOG("%s: Tuning to a new channel!!", __FUNCTION__);

    return 0;
}