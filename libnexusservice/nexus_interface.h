/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
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
 * $brcm_Workfile: nexus_interface.h $
 * 
 *****************************************************************************/
#ifndef _NEXUS_INTERFACE_H_
#define _NEXUS_INTERFACE_H_

#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/RefBase.h>
#include <utils/String16.h>

#define NEXUS_INTERFACE_NAME "com.broadcom.bse.nexusInterface"


#define android_DECLARE_META_INTERFACE(INTERFACE)                               \
        static const android::String16 descriptor;                      \
        static android::sp<I##INTERFACE> asInterface(const android::sp<android::IBinder>& obj); \
        static android::String16 getInterfaceDescriptor();

#define android_IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
        const android::String16 I##INTERFACE::descriptor(NAME);         \
        android::String16 I##INTERFACE::getInterfaceDescriptor() { \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
        android::sp<I##INTERFACE> I##INTERFACE::asInterface(const android::sp<android::IBinder>& obj) \
    {                                                                   \
            android::sp<I##INTERFACE> intr;                             \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Bp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }


typedef enum {
        API_OVER_BINDER,
        SET_HDMI_CEC_MESSAGE_EVENT_LISTENER,
        ADD_HDMI_HOTPLUG_EVENT_LISTENER,
        REMOVE_HDMI_HOTPLUG_EVENT_LISTENER,
        ADD_DISPLAY_CHANGED_EVENT_LISTENER,
        REMOVE_DISPLAY_CHANGED_EVENT_LISTENER
} NEXUS_TRANSACT_ID;

class INexusClient: public android::IInterface {
public:
        android_DECLARE_META_INTERFACE(NexusClient)

        virtual void NexusHandles(NEXUS_TRANSACT_ID eTransactId, int32_t *pHandle) = 0;

        typedef struct api_data api_data;
        virtual void api_over_binder(api_data *cmd) = 0;
        virtual android::IBinder* get_remote() = 0;
};

#endif

