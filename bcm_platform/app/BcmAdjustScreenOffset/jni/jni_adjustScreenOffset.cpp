/******************************************************************************
 *    (c)2012, 2015 Broadcom Corporation
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
 *****************************************************************************/
#include <jni.h>

#include <cutils/memory.h>
#include <cutils/properties.h>

#include <utils/Log.h>
#include  <stdlib.h>
#include  <stdio.h>

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "HwcCommon.h"
#include "Hwc.h"
#include "HwcListener.h"
#include "IHwc.h"
#include "HwcSvc.h"

#define LOG_TAG "bcm-overscan"

using namespace android;

typedef void (* HWC_APP_BINDER_NTFY_CB)(int, int, struct hwc_notification_info &);

class HwcAppBinder : public HwcListener
{
public:

    HwcAppBinder() : cb(NULL), cb_data(0) {};
    virtual ~HwcAppBinder() {};

    virtual void notify(int msg, struct hwc_notification_info &ntfy);

    inline void listen() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->registerListener(this, HWC_BINDER_COM);
       else
           ALOGE("%s: failed to associate %p with HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void hangup() {
       if (get_hwc(false) != NULL)
           get_hwc(false)->unregisterListener(this);
       else
           ALOGE("%s: failed to dissociate %p from HwcAppBinder service.", __FUNCTION__, this);
    };

    inline void getoverscan(struct hwc_position &position) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->getOverscanAdjust(this, position);
       }
    };

    inline void setoverscan(struct hwc_position &position) {
       if (get_hwc(false) != NULL) {
           get_hwc(false)->setOverscanAdjust(this, position);
       }
    };

private:
    HWC_APP_BINDER_NTFY_CB cb;
    int cb_data;
};

class HwcAppBinder_wrap
{
private:

   sp<HwcAppBinder> ihwc;

public:
   HwcAppBinder_wrap(void) {
      ALOGV("%s: allocated %p", __FUNCTION__, this);
      ihwc = new HwcAppBinder;
      ihwc.get()->listen();
   };

   virtual ~HwcAppBinder_wrap(void) {
      ALOGV("%s: cleared %p", __FUNCTION__, this);
      ihwc.get()->hangup();
      ihwc.clear();
   };

   void getoverscan(struct hwc_position &position) {
      ihwc.get()->getoverscan(position);
   }

   void setoverscan(struct hwc_position &position) {
      ihwc.get()->setoverscan(position);
   }

   HwcAppBinder *get(void) {
      return ihwc.get();
   }
};

void HwcAppBinder::notify(int msg, struct hwc_notification_info &ntfy)
{
   ALOGV( "%s: notify received: msg=%u", __FUNCTION__, msg);

   if (cb)
      cb(cb_data, msg, ntfy);
}

HwcAppBinder_wrap *m_appHwcBinder;

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
        ALOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        ALOGE("RegisterNatives failed for '%s'", className);
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

    (void)reserved;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        return result;
    }
    assert(env != NULL);


    if (register_adjustScreenOffset_jni(env) < 0)
    {
        ALOGE("ERROR: register adjustScreenOffset interface error failed\n");
        return result;
    }

    // connect to the HWC binder.
    m_appHwcBinder = new HwcAppBinder_wrap;
    if ( NULL == m_appHwcBinder )
    {
        ALOGE("Unable to connect to HwcBinder");
    }


    result = JNI_VERSION_1_4;

    return result;
}


JNIEXPORT void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_setScreenOffset
  (JNIEnv *env, jobject thisobj, jobject offset)
{
    (void)thisobj;

    struct hwc_position position;

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

    m_appHwcBinder->getoverscan(position);

    ALOGI("Changing composition [x,y,w,h] = [%d, %d, %d, %d] ----> [%d, %d, %d, %d] \n",
        position.x, position.y, position.w, position.h,
        left, top, right - left, bottom - top);

    position.x = left;
    position.y = top;
    position.w = right - left;
    position.h = bottom - top;

    m_appHwcBinder->setoverscan(position);
}

JNIEXPORT void JNICALL Java_com_android_adjustScreenOffset_native_1adjustScreenOffset_getScreenOffset
  (JNIEnv *env, jobject thisobj, jobject offset)
{
    (void)thisobj;

    struct hwc_position position;

    m_appHwcBinder->getoverscan(position);

    ALOGI("get [%d, %d, %d, %d]  \n",
        position.x, position.y, position.w, position.h);

    jint left = position.x;
    jint top = position.y;
    jint right = position.x + position.w;
    jint bottom = position.y + position.h;

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
