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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 * 
 * Module Description:
 * Header file to complement nexus_power.cpp
 * 
 * Revision History:
 * 
 * $brcm_Log: $
 *****************************************************************************/
#ifndef _NEXUS_POWER_H_
#define _NEXUS_POWER_H_

// Uncomment the line below to enable Verbose messages
//#define LOG_NDEBUG 0
#define LOG_TAG "Brcmstb PowerHAL"

#include <utils/Log.h>
#include <utils/PropertyMap.h>
#include <utils/RefBase.h>

#include "nexus_types.h"
#include "nexus_platform_features.h"
#include "nexus_ipc_client_factory.h"
#include <nexus_gpio.h>

using namespace android;

class NexusPower : public android::RefBase {

    public:
    static sp<NexusPower> instantiate();
    status_t setPowerState(b_powerState state);
    status_t getPowerState(b_powerState *pState);
    status_t initialiseGpios();
    void uninitialiseGpios();
    status_t clearGpios();
    ~NexusPower();

    class NexusGpio : public android::RefBase {
        public:
        static int const MAX_INSTANCES = 4;
        static unsigned mInstances;

        static sp<NexusGpio> instantiate(NexusPower *pNexusPower, unsigned pin, unsigned pinType, NEXUS_GpioInterrupt interruptMode);
        static String8 getConfigurationFilePath();
        static status_t loadConfigurationFile(String8 path, PropertyMap **configuration);

        unsigned getPinType() { return mPinType; }
        unsigned getPin() { return mPin; }
        NEXUS_GpioInterrupt getInterruptMode() { return mInterruptMode; }
        void setHandle(NEXUS_GpioHandle handle) { mHandle = handle; }
        NEXUS_GpioHandle getHandle() { return mHandle; }
        unsigned getInstance() { return mInstance; }
        static unsigned getInstances() { return mInstances; }
        static NEXUS_GpioInterrupt parseGpioInterruptMode(String8& modeString);
        ~NexusGpio();

        private:
        sp<NexusPower> mNexusPower;
        unsigned mInstance;
        unsigned mPin;
        unsigned mPinType;
        NEXUS_GpioInterrupt mInterruptMode;
        NEXUS_GpioHandle mHandle;

        static void gpioCallback(void *context, int param);

        // Disallow constructor and copy constructor...
        NexusGpio();
        NexusGpio(NexusPower *pNexusPower, unsigned pin, unsigned pinType, NEXUS_GpioInterrupt interruptMode);
        NexusGpio &operator=(const NexusGpio &);
    };

    private:
    NexusIPCClientBase *mIpcClient;
    NexusClientContext *mClientContext;
    sp<NexusGpio> gpios[NexusGpio::MAX_INSTANCES];

    // Disallow constructor and copy constructor...
    NexusPower();
    NexusPower(NexusIPCClientBase *pIpcClient, NexusClientContext *pClientContext);
    NexusPower &operator=(const NexusPower &);

    int getDeviceType();
};

#endif /* _NEXUS_POWER_H_ */

