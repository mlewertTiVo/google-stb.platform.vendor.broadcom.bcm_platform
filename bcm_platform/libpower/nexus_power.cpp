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
 * This file contains functions to make Nexus IPC calls to the Nexus service
 * in order to set the power state and retrieve the power state.
 * 
 * Revision History:
 * 
 * $brcm_Log: $
 *****************************************************************************/
#include "nexus_power.h"

#include <string.h>
#include <utils/Errors.h>

unsigned NexusPower::NexusGpio::mInstances = 0;

NexusPower::NexusPower() : mIpcClient(NULL), mClientContext(NULL)
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);
}

NexusPower::NexusPower(NexusIPCClientBase *pIpcClient, NexusClientContext *pClientContext) : mIpcClient(pIpcClient), mClientContext(pClientContext)
{
    ALOGV("%s: pIpcClient=%p, pClientContext=%p", __PRETTY_FUNCTION__, (void *)pIpcClient, (void *)pClientContext);
}

NexusPower::~NexusPower()
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);

    if (mIpcClient != NULL) {
        if (mClientContext != NULL) {
            uninitialiseGpios();
            mIpcClient->destroyClientContext(mClientContext);
            mClientContext = NULL;
        }
        delete mIpcClient;
        mIpcClient = NULL;
    }
}

sp<NexusPower> NexusPower::instantiate()
{
    NexusIPCClientBase *pIpcClient;
    sp<NexusPower> np;

    pIpcClient = NexusIPCClientFactory::getClient("Android-Power");

    if (pIpcClient == NULL) {
        ALOGE("%s: Could not create Nexux Client!!!", __FUNCTION__);
    }
    else {
        NexusClientContext *pClientContext;
        b_refsw_client_client_configuration clientConfig;

        memset(&clientConfig, 0, sizeof(clientConfig));
        strncpy(clientConfig.name.string, "Android-Power", sizeof(clientConfig.name.string));

        pClientContext = pIpcClient->createClientContext(&clientConfig);
        if (pClientContext == NULL) {
            ALOGE("%s: Could not create Nexus Client Context!!!", __FUNCTION__); 
            delete pIpcClient;
        }
        else {
            np = new NexusPower(pIpcClient, pClientContext);
            if (np != NULL) {
                ALOGV("%s: Successfully instantiated NexusPower.", __FUNCTION__);

                if (np->initialiseGpios() == NO_ERROR) {
                    ALOGV("%s: Successfully initialised GPIOs", __FUNCTION__);
                }
            }
        }
    }
    return np;
}

/* Android-L sets up a property to define the device type and hence
   the logical address of the device.  */
int NexusPower::getDeviceType()
{
    char value[PROPERTY_VALUE_MAX];
    int type = -1;

    if (property_get("ro.hdmi.device_type", value, NULL)) {
        type = atoi(value);
    }
    return type;
}

status_t NexusPower::setPowerState(b_powerState state)
{
    status_t ret = NO_ERROR;
    const uint32_t cecId = 0;   // Hardcoded CEC id to 0
    int deviceType = getDeviceType();

    /* If powering out of standby, then ensure platform is brought up first before CEC,
       otherwise if powering down, then we must send CEC commands first. */
    if (state == ePowerState_S0) {
        if (mIpcClient->setPowerState(state) != true) {
            LOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
            ret = INVALID_OPERATION;
        }
        else if (deviceType == -1 && mIpcClient->isCecEnabled(cecId) && mIpcClient->setCecPowerState(cecId, state) != true) {
            LOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
            ret = INVALID_OPERATION;
        }
    }
    else {
        if (deviceType == -1 && mIpcClient->isCecEnabled(cecId) && mIpcClient->setCecPowerState(cecId, state) != true) {
            LOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
            ret = INVALID_OPERATION;
        }

        if (mIpcClient->setPowerState(state) != true) {
            LOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
            ret = INVALID_OPERATION;
        }

        if (ret == NO_ERROR) {
            // Finally clear any pending Gpio interrupts to avoid being accidentally woken up again when suspended
            ret = clearGpios();
        }
    }
    return ret;
}

status_t NexusPower::getPowerState(b_powerState *pState)
{
    if (pState == NULL) {
        LOGE("%s: invalid parameter \"pState\"!", __FUNCTION__);
        return BAD_VALUE;
    }

    *pState = mIpcClient->getPowerState();
    LOGD("%s: Power state = %d", __FUNCTION__, *pState);
    return NO_ERROR;
}

void NexusPower::NexusGpio::gpioCallback(void *context __unused, int param __unused)
{
    NexusPower::NexusGpio *pNexusGpio = reinterpret_cast<NexusPower::NexusGpio *>(context);

    if (pNexusGpio != NULL) {
        NEXUS_GpioStatus gpioStatus;

        if (NEXUS_Gpio_GetStatus(pNexusGpio->getHandle(), &gpioStatus) == NEXUS_SUCCESS) {
            ALOGV("%s: AON_%s%02d interrupt %spending.", __FUNCTION__,
                   (pNexusGpio->getPinType() == NEXUS_GpioType_eAonStandard) ? "GPIO" : "SGPIO", param,
                   gpioStatus.interruptPending ? "" : "not ");

            if (gpioStatus.interruptPending) {
                NEXUS_Gpio_ClearInterrupt(pNexusGpio->getHandle());
            }
        }
        else {
            ALOGE("%s: Could NOT get AON_%s%02d interrupt status.", __FUNCTION__,
                   (pNexusGpio->getPinType() == NEXUS_GpioType_eAonStandard) ? "GPIO" : "SGPIO", param);
        }

    }
    else {
        ALOGE("%s: Invalid context!!!", __FUNCTION__);
    }
}

String8 NexusPower::NexusGpio::getConfigurationFilePath()
{
    String8 path;

    path.setTo(getenv("ANDROID_ROOT"));
    path.append("/vendor/power/aon_gpio.cfg");

    if (!access(path.string(), R_OK)) {
        ALOGV("%s: Found \"%s\".", __FUNCTION__, path.string());
        return path;
    }
    else {
        ALOGV("%s: Could not find \"%s\"!", __FUNCTION__, path.string());
        return String8();
    }
}

/* The aon_gpio.cfg file is made up of key/value pairs for the AON GPIO/SGPIO's.  This comprises
   the name of the GPIO/SGPIO followed by an "=" and the interrupt mode (e.g. "FallingEdge").

   Example1: AON_GPIO01 = FallingEdge
   Example2: AON_SGPIO02 = Edge
*/
status_t NexusPower::NexusGpio::loadConfigurationFile(String8 path, PropertyMap **configuration)
{
    status_t status = NAME_NOT_FOUND;

    if (!path.isEmpty()) {
        status = PropertyMap::load(path, configuration);
    }
    return status;
}

NEXUS_GpioInterrupt NexusPower::NexusGpio::parseGpioInterruptMode(String8& modeString)
{
    String8 string(modeString);

    string.toLower();

    if (string == "disabled") {
        return NEXUS_GpioInterrupt_eDisabled;
    }
    else if (string == "risingedge") {
        return NEXUS_GpioInterrupt_eRisingEdge;
    }
    else if (string == "fallingedge") {
        return NEXUS_GpioInterrupt_eFallingEdge;
    }
    else if (string == "edge") {
        return NEXUS_GpioInterrupt_eEdge;
    }
    else if (string == "low") {
        return NEXUS_GpioInterrupt_eLow;
    }
    else if (string == "high") {
        return NEXUS_GpioInterrupt_eHigh;
    }
    else {
        return NEXUS_GpioInterrupt_eMax;
    }
}

NexusPower::NexusGpio::NexusGpio()
{
    ALOGV("%s: called", __PRETTY_FUNCTION__);
}

NexusPower::NexusGpio::NexusGpio(NexusPower *pNexusPower, unsigned pin, unsigned pinType, NEXUS_GpioInterrupt interruptMode) : mNexusPower(pNexusPower),
                                                                                                                               mPin(pin),
                                                                                                                               mPinType(pinType),
                                                                                                                               mInterruptMode(interruptMode)
{
    mInstance = mInstances++;
    ALOGV("%s: instance %d: pin=%d, type=%d", __PRETTY_FUNCTION__, mInstance, pin, pinType);
}

NexusPower::NexusGpio::~NexusGpio()
{
    ALOGV("%s: instance %d: pin=%d, type=%d", __PRETTY_FUNCTION__, mInstance, mPin, mPinType);
    mInstances--;
    NEXUS_Gpio_Close(mHandle);
}

sp<NexusPower::NexusGpio> NexusPower::NexusGpio::instantiate(NexusPower *pNexusPower, unsigned pin, unsigned pinType, NEXUS_GpioInterrupt interruptMode)
{
    sp<NexusPower::NexusGpio> gpio = new NexusGpio(pNexusPower, pin, pinType, interruptMode);

    if (NexusPower::NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES) {
        NEXUS_GpioSettings settings;
        NEXUS_GpioHandle handle;

        NEXUS_Gpio_GetDefaultSettings(pinType, &settings);
        settings.mode = NEXUS_GpioMode_eInput;
        settings.interruptMode = interruptMode;
        settings.maskEdgeInterrupts = true;
        settings.interrupt.callback = NexusPower::NexusGpio::gpioCallback;
        settings.interrupt.context  = gpio.get();
        settings.interrupt.param    = pin;

        handle = NEXUS_Gpio_Open(pinType, pin, &settings);
        if (handle != NULL) {
            ALOGV("%s: Successfully opened AON_GPIO%02d", __FUNCTION__, pin);
            gpio->setHandle(handle);
        }
        else {
            ALOGE("%s: Could not open AON_%s%02d!!!", __FUNCTION__, (pinType == NEXUS_GpioType_eAonStandard) ? "GPIO" : "SGPIO", pin);
            gpio = NULL;
        }
    }
    return gpio;
}

status_t NexusPower::initialiseGpios()
{
    status_t status;
    NEXUS_GpioInterrupt gpioMode;
    int pin;
    PropertyMap* config;
    String8 path;

    path = NexusGpio::getConfigurationFilePath();
    if (path.isEmpty()) {
        ALOGW("%s: Could not find configuration file, so not initialising AON GPIOs!", __FUNCTION__, path.string());
        status = NAME_NOT_FOUND;
    }
    else {
        status = NexusGpio::loadConfigurationFile(path, &config);
        if (status == NO_ERROR) {
            String8 gpioName;
            String8 gpioValue;
            sp<NexusGpio> gpio;

            /* Setup AON_GPIO custom configuration */
            for (pin = 0; pin < NEXUS_NUM_AON_GPIO_PINS && (status == NO_ERROR) && NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES; pin++) {
                gpioName.setTo("AON_GPIO");
                gpioName.appendFormat("%02d", pin);

                if (config->tryGetProperty(gpioName, gpioValue)) {
                    gpioMode = NexusGpio::parseGpioInterruptMode(gpioValue);
                    ALOGV("%s: %s=%s", __FUNCTION__, gpioName.string(), gpioValue.string());

                    gpio = NexusGpio::instantiate(this, pin, NEXUS_GpioType_eAonStandard, gpioMode);
                    if (gpio.get() == NULL) {
                        ALOGE("%s: Could not instantiate Nexus AON_GPIO%02d!!!", __FUNCTION__, pin);
                        status = NO_INIT;
                    }
                    else {
                        gpios[gpio->getInstance()] = gpio;
                    }
                }
            }

            /* Setup AON_SGPIO custom configuration */
            for (pin = 0; pin < NEXUS_NUM_AON_SGPIO_PINS && (status == NO_ERROR) && NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES; pin++) {
                gpioName.setTo("AON_SGPIO");
                gpioName.appendFormat("%02d", pin);

                if (config->tryGetProperty(gpioName, gpioValue)) {
                    gpioMode = NexusGpio::parseGpioInterruptMode(gpioValue);
                    ALOGV("%s: %s=%s", __FUNCTION__, gpioName.string(), gpioValue.string());

                    gpio = NexusGpio::instantiate(this, pin, NEXUS_GpioType_eAonSpecial, gpioMode);
                    if (gpio.get() == NULL) {
                        ALOGE("%s: Could not instantiate Nexus AON_SGPIO%02d!!!", __FUNCTION__, pin);
                        status = NO_INIT;
                    }
                    else {
                        gpios[gpio->getInstance()] = gpio;
                    }
                }
            }
            delete config;
        }
        else {
            ALOGE("%s: Could not load configuration file \"%s\" [ret=%d]!!!", __FUNCTION__, path.string(), status);
        }
    }
    return status;
}

void NexusPower::uninitialiseGpios()
{
    for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
        if (gpios[gpio] != NULL) {
            gpios[gpio] = NULL;
        }
    }
}

status_t NexusPower::clearGpios()
{
    status_t status = NO_ERROR;
    NEXUS_Error rc;
    sp<NexusGpio> pNexusGpio;

    for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
        pNexusGpio = gpios[gpio];
        if (pNexusGpio.get() != NULL) {
            rc = NEXUS_Gpio_ClearInterrupt(pNexusGpio->getHandle());
            if (rc != NEXUS_SUCCESS) {
                ALOGE("%s: Could not clear AON_%s%d [rc=%d]!!!", __FUNCTION__, (pNexusGpio->getPinType() == NEXUS_GpioType_eAonStandard) ? "GPIO" : "SGPIO", gpio, rc);
                status = INVALID_OPERATION;
            }
        }
    }
    return status;
}

