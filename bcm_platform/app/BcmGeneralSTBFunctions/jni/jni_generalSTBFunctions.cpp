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

#include  <stdlib.h>
#include  <stdio.h>

#include "nexus_platform.h"
#include "nexus_display.h"

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "nexus_interface.h"

#include "nexus_ipc_client_factory.h"

using namespace android;

static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setBrightness(JNIEnv *env, jobject obj, jint progress);
static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getBrightness(JNIEnv *env, jobject obj);

static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setContrast(JNIEnv *env, jobject obj, jint progress);
static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getContrast(JNIEnv *env, jobject obj);

static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setSaturation(JNIEnv *env, jobject obj, jint progress);
static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getSaturation(JNIEnv *env, jobject obj);

static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setHue(JNIEnv *env, jobject obj, jint progress);
static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getHue(JNIEnv *env, jobject obj);

static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_removeOutput(JNIEnv *env, jobject obj);
static jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_addOutput(JNIEnv *env, jobject obj);

static JNINativeMethod gMethods[] = {

    {"setBrightness",   "(I)I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setBrightness},
    {"getBrightness",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getBrightness},

    {"setContrast",   "(I)I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setContrast},
    {"getContrast",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getContrast},

    {"setSaturation",   "(I)I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setSaturation}, 
    {"getSaturation",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getSaturation},

    {"setHue",   "(I)I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setHue},
    {"getHue",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getHue},

    {"removeOutput",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_removeOutput}, 
    {"addOutput",   "()I", (void *)Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_addOutput}, 
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

static int register_generalSTBFunctions_jni(JNIEnv *env){

    return registerNativeMethods(env,"com/android/generalSTBFunctions/native_generalSTBFunctions", gMethods,  sizeof(gMethods) / sizeof(gMethods[0]));
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

    if (register_generalSTBFunctions_jni(env) < 0)
    {
        LOGE("ERROR: register generalSTBFunctions interface error failed\n");
        return result;
    }

    result = JNI_VERSION_1_4;
    return result;
}

#define VALUE_RANGE 65535 /*32768+32767*/
#define VALUE_MIN -32768

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getBrightness
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_GraphicsColorSettings settings;
    int progress;
    int brightness;

    /*get the value*/
    ipcclient->getGraphicsColorSettings(0, &settings); 
    LOGI("getBrightness brightness %d\n",settings.brightness);

    /*calculate the percentage*/
    brightness = (settings.brightness-VALUE_MIN)*100;
    progress = brightness/VALUE_RANGE;
    if(brightness%VALUE_RANGE)
        progress+=1;
    LOGI("getBrightness %d\n",progress);

    delete ipcclient;

    return (jint)progress;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setBrightness
(JNIEnv *env, jobject obj, jint progress)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_PictureCtrlCommonSettings settings1;
    NEXUS_GraphicsColorSettings settings2;
    int brightness;

    /*calculate the value from percentage*/
    brightness = VALUE_MIN + (VALUE_RANGE*progress)/100 ;
    LOGI("setBrightness brightness %d\n",brightness);

    /*set value to video window 0 and display 0*/
    ipcclient->getPictureCtrlCommonSettings(0, &settings1); 
    settings1.brightness = brightness;
    ipcclient->setPictureCtrlCommonSettings(0, &settings1);

    ipcclient->getGraphicsColorSettings(0, &settings2); 
    settings2.brightness = brightness;
    ipcclient->setGraphicsColorSettings(0, &settings2); 

    /*set value to video window 1 and display 1*/
    ipcclient->getPictureCtrlCommonSettings(1, &settings1); 
    settings1.brightness = brightness;
    ipcclient->setPictureCtrlCommonSettings(1, &settings1);

    ipcclient->getGraphicsColorSettings(1, &settings2); 
    settings2.brightness = brightness;
    ipcclient->setGraphicsColorSettings(1, &settings2); 

    LOGI("setBrightness %d \n",progress);

    delete ipcclient;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getContrast
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_GraphicsColorSettings settings;
    int progress;
    int contrast;

    ipcclient->getGraphicsColorSettings(0, &settings); 
    LOGI("getContrast contrast %d\n",settings.contrast);

    contrast = (settings.contrast-VALUE_MIN)*100;
    progress = contrast/VALUE_RANGE;
    if(contrast%VALUE_RANGE)
        progress+=1;
    LOGI("getContrast %d\n",progress);

    delete ipcclient;
    return (jint)progress;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setContrast
(JNIEnv *env, jobject obj, jint progress)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_PictureCtrlCommonSettings settings1;
    NEXUS_GraphicsColorSettings settings2;
    int contrast;

    contrast = VALUE_MIN + (VALUE_RANGE*progress)/100 ;
    LOGI("setContrast contrast %d\n",contrast);

    /*set 0*/
    ipcclient->getPictureCtrlCommonSettings(0, &settings1); 
    settings1.contrast = contrast;
    ipcclient->setPictureCtrlCommonSettings(0, &settings1);

    ipcclient->getGraphicsColorSettings(0, &settings2); 
    settings2.contrast = contrast;
    ipcclient->setGraphicsColorSettings(0, &settings2); 

    /*set 1*/
    ipcclient->getPictureCtrlCommonSettings(1, &settings1); 
    settings1.contrast = contrast;
    ipcclient->setPictureCtrlCommonSettings(1, &settings1);

    ipcclient->getGraphicsColorSettings(1, &settings2); 
    settings2.contrast = contrast;
    ipcclient->setGraphicsColorSettings(1, &settings2);

    LOGI("setContrast %d \n",progress);

    delete ipcclient;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getSaturation
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_GraphicsColorSettings settings;
    int progress;
    int saturation;

    ipcclient->getGraphicsColorSettings(0, &settings); 
    LOGI("getSaturation saturation %d\n",settings.saturation);

    saturation = (settings.saturation-VALUE_MIN)*100;
    progress = saturation/VALUE_RANGE;
    if(saturation%VALUE_RANGE)
        progress+=1;
    LOGI("getSaturation %d\n",progress);

    delete ipcclient;
    return (jint)progress;
}


JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setSaturation
(JNIEnv *env, jobject obj, jint progress)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_PictureCtrlCommonSettings settings1;
    NEXUS_GraphicsColorSettings settings2;
    int saturation;

    saturation = VALUE_MIN + (VALUE_RANGE*progress)/100 ;
    LOGI("setSaturation saturation %d\n",saturation);

    /*set 0*/
    ipcclient->getPictureCtrlCommonSettings(0, &settings1); 
    settings1.saturation = saturation;
    ipcclient->setPictureCtrlCommonSettings(0, &settings1);

    ipcclient->getGraphicsColorSettings(0, &settings2); 
    settings2.saturation = saturation;
    ipcclient->setGraphicsColorSettings(0, &settings2);

    /*set 1*/
    ipcclient->getPictureCtrlCommonSettings(1, &settings1); 
    settings1.saturation = saturation;
    ipcclient->setPictureCtrlCommonSettings(1, &settings1);

    ipcclient->getGraphicsColorSettings(1, &settings2); 
    settings2.saturation = saturation;
    ipcclient->setGraphicsColorSettings(1, &settings2);

    LOGI("setSaturation %d \n",progress);

    delete ipcclient;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_getHue
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_GraphicsColorSettings settings;
    int progress;
    int hue;

    ipcclient->getGraphicsColorSettings(0, &settings); 
    LOGI("getHue hue %d\n",settings.hue);

    hue = (settings.hue-VALUE_MIN)*100;
    progress = hue/VALUE_RANGE;
    if(hue%VALUE_RANGE)
        progress+=1;
    LOGI("getHue %d\n",progress);

    delete ipcclient;
    return (jint)progress;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_setHue
(JNIEnv *env, jobject obj, jint progress)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");
    NEXUS_PictureCtrlCommonSettings settings1;
    NEXUS_GraphicsColorSettings settings2;
    int hue;

    hue = VALUE_MIN + (VALUE_RANGE*progress)/100 ;
    LOGI("setHue hue %d\n",hue);

    /*set 0*/
    ipcclient->getPictureCtrlCommonSettings(0, &settings1); 
    settings1.hue = hue;
    ipcclient->setPictureCtrlCommonSettings(0, &settings1);

    ipcclient->getGraphicsColorSettings(0, &settings2); 
    settings2.hue = hue;
    ipcclient->setGraphicsColorSettings(0, &settings2);

    /*set 1*/
    ipcclient->getPictureCtrlCommonSettings(1, &settings1); 
    settings1.hue = hue;
    ipcclient->setPictureCtrlCommonSettings(1, &settings1);

    ipcclient->getGraphicsColorSettings(1, &settings2); 
    settings2.hue = hue;
    ipcclient->setGraphicsColorSettings(1, &settings2);

    LOGI("setHue %d \n",hue);

    delete ipcclient;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_removeOutput
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");

    LOGI("removeOutput call setDisplayOutputs 0 \n");
    ipcclient->setDisplayOutputs(0);

    LOGI("removeOutput call setAudioMute 1 \n");
    ipcclient->setAudioMute(1);

    delete ipcclient;
    return 0;
}

JNIEXPORT jint JNICALL Java_com_android_generalSTBFunctions_native_1generalSTBFunctions_addOutput
(JNIEnv *env, jobject obj)
{
    NexusIPCClientBase *ipcclient = NexusIPCClientFactory::getClient("generalSTBFunctions");

    ipcclient->setDisplayOutputs(1); 
    ipcclient->setAudioMute(0);

    delete ipcclient;
    return 0;
}

