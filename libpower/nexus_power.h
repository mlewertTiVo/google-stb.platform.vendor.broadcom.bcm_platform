/******************************************************************************
 *    (c)2011-2015 Broadcom Corporation
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
#define LOG_TAG "Brcmstb-PowerHAL"

#include <utils/Log.h>
#include <utils/PropertyMap.h>
#include <utils/RefBase.h>

#include "nexus_types.h"
#include "nexus_platform_features.h"
#include "nexus_ipc_client_factory.h"
#include "nxclient.h"
#include <nexus_gpio.h>
#include "linuxuinput.h"
#include "droid_pm.h"

using namespace android;

class NexusPower : public android::RefBase {

    public:
    static sp<NexusPower> instantiate(int powerFd);
    status_t preparePowerState(b_powerState state);
    status_t setPowerState(b_powerState state);
    status_t getPowerStatus(b_powerStatus *pPowerStatus);
    status_t setVideoOutputsState(b_powerState state);
    status_t initialiseGpios(int powerFd);
    void uninitialiseGpios();
    status_t setGpios(b_powerState state);
    status_t clearGpios();
    ~NexusPower();

    // LinuxUInput class with refcount
    class LinuxUInputRef : public LinuxUInput, public android::RefBase {
        public:
        static sp<LinuxUInputRef> instantiate();
        ~LinuxUInputRef();

        private:
        LinuxUInputRef();
    };

    class NexusGpio : public android::RefBase {
        public:
        static int const MAX_INSTANCES = 8;
        static int const MAX_POWER_STATES = 7;  // S0 through to S5
        static int const NUM_INP_PARAMETERS = 2; // gpio mode + interrupt mode
        static int const NUM_OUT_PARAMETERS = MAX_POWER_STATES + 1; // gpio mode + S0 through to S5 output values
        static int const MIN_PARAMETERS = NUM_INP_PARAMETERS;
        static int const MAX_PARAMETERS = NUM_OUT_PARAMETERS;
        static unsigned mInstances;

        // Public methods...
        static sp<NexusGpio> initialise(int powerFd, String8& gpioName, String8& gpioValue, int pin, unsigned pinType, sp<LinuxUInputRef> uInput);
        static String8  getConfigurationFilePath();
        static status_t loadConfigurationFile(String8 path, PropertyMap **configuration);
        static unsigned getInstances() { return mInstances; }

        String8& getPinName() { return mPinName; }
        unsigned getPin() { return mPin; }
        unsigned getPinType() { return mPinType; }
        unsigned getInstance() { return mInstance; }
        NEXUS_GpioMode getPinMode() { return mPinMode; }
        NEXUS_GpioInterrupt getPinInterruptMode() { return mPinInterruptMode; }
        NEXUS_GpioValue getPinOutputValue(b_powerState state) { return mPinOutputValues[state]; }
        NEXUS_GpioHandle getHandle() { return mHandle; }
        void setHandle(NEXUS_GpioHandle handle) { mHandle = handle; }
        void setPowerKeyEvent(bool enable) { mPowerKeyEvent = enable; }
        ~NexusGpio();

        private:
        String8 mPinName;
        unsigned mInstance;
        unsigned mPin;
        unsigned mPinType;
        NEXUS_GpioMode mPinMode;
        NEXUS_GpioInterrupt mPinInterruptMode;
        NEXUS_GpioValue mPinOutputValues[MAX_POWER_STATES];
        NEXUS_GpioHandle mHandle;
        sp<LinuxUInputRef> mUInput;
        bool mPowerKeyEvent;
        int mPowerFd;

        // Private methods...
        static sp<NexusGpio> instantiate(int powerFd, String8& pinName, unsigned pin, unsigned pinType,
                                         NEXUS_GpioMode pinMode,  NEXUS_GpioInterrupt interruptMode, sp<LinuxUInputRef> uInput);
        static sp<NexusGpio> instantiate(int powerFd, String8& pinName, unsigned pin, unsigned pinType,
                                         NEXUS_GpioMode pinMode, NEXUS_GpioValue *pOutputValues);
        static status_t parseGpioMode(String8& modeString, NEXUS_GpioMode *pMode);
        static status_t parseGpioInterruptMode(String8& interruptModeString, NEXUS_GpioInterrupt *pInterruptMode);
        static status_t parseGpioOutputValue(String8& outputValueString, NEXUS_GpioValue *pOutputValue);
        static status_t parseGpioParameters(String8& inString, size_t *pNumParameters, String8 parameters[]);
        static void gpioCallback(void *context, int param);

        // Disallow constructor and copy constructor...
        NexusGpio();
        NexusGpio(int powerFd, String8& pinName, unsigned pin, unsigned pinType, NEXUS_GpioMode mode,
                  NEXUS_GpioInterrupt interruptMode, sp<LinuxUInputRef> uInput);
        NexusGpio(int powerFd, String8& pinName, unsigned pin, unsigned pinType, NEXUS_GpioMode mode,
                  NEXUS_GpioValue *pOutputValues);
        NexusGpio &operator=(const NexusGpio &);
    };

    private:
    b_cecDeviceType mCecDeviceType;
    NexusIPCClientBase *mIpcClient;
    NexusClientContext *mClientContext;
    sp<NexusGpio> gpios[NexusGpio::MAX_INSTANCES];
    sp<LinuxUInputRef> mUInput;

    // Private methods...
    bool getCecTransmitStandby();
    bool getCecTransmitViewOn();

    // Disallow constructor and copy constructor...
    NexusPower();
    NexusPower(NexusIPCClientBase *pIpcClient, NexusClientContext *pClientContext);
    NexusPower &operator=(const NexusPower &);
};

#endif /* _NEXUS_POWER_H_ */

