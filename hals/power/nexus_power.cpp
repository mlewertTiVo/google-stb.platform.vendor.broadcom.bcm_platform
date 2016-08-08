/******************************************************************************
 *    (c)2011-2016 Broadcom Corporation
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
#include <hardware_legacy/power.h>

unsigned NexusPower::NexusGpio::mInstances = 0;


sp<NexusPower::LinuxUInputRef> NexusPower::LinuxUInputRef::instantiate()
{
    sp<NexusPower::LinuxUInputRef> uInput = NULL;

    static const struct input_id id =
    {
        BUS_HOST, /* bustype */
        0, /* vendor */
        0, /* product */
        1  /* version */
    };

    uInput = new NexusPower::LinuxUInputRef();

    if (uInput != NULL)
    {
        if (!uInput->open() ||
            !uInput->register_event(EV_KEY) ||
            !uInput->register_key(KEY_WAKEUP) ||
            !uInput->start("NexusPower", id))
        {
            ALOGE("Failed to initialise LinuxUInputRef");
            uInput = NULL;
        }
    }

    return uInput;
}

NexusPower::LinuxUInputRef::LinuxUInputRef()
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);
}

NexusPower::LinuxUInputRef::~LinuxUInputRef()
{
    stop();
    close();
}

NexusPower::NexusPower() : mCecDeviceType(eCecDeviceType_eInvalid), mIpcClient(NULL), mClientContext(NULL)
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);
}

NexusPower::NexusPower(NexusIPCClientBase *pIpcClient, NexusClientContext *pClientContext) : mIpcClient(pIpcClient), mClientContext(pClientContext)
{
    ALOGV("%s: pIpcClient=%p, pClientContext=%p", __PRETTY_FUNCTION__, (void *)pIpcClient, (void *)pClientContext);
    mCecDeviceType = mIpcClient->getCecDeviceType();

    mUInput = NexusPower::LinuxUInputRef::instantiate();
}

NexusPower::~NexusPower()
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);

    if (mIpcClient != NULL) {
        if (mClientContext != NULL) {
            mIpcClient->destroyClientContext(mClientContext);
            mClientContext = NULL;
        }
        delete mIpcClient;
        mIpcClient = NULL;
    }

    mUInput = NULL;
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
        NexusClientContext *pClientContext = pIpcClient->createClientContext();

        if (pClientContext == NULL) {
            ALOGE("%s: Could not create Nexus Client Context!!!", __FUNCTION__);
            delete pIpcClient;
        }
        else {
            np = new NexusPower(pIpcClient, pClientContext);
            if (np != NULL) {
                ALOGV("%s: Successfully instantiated NexusPower.", __FUNCTION__);
            }
        }
    }
    return np;
}

status_t NexusPower::setVideoOutputsState(b_powerState state)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_DisplaySettings displaySettings;
    bool enableHdmiOutput = (state == ePowerState_S0);

    do {
        NxClient_GetDisplaySettings(&displaySettings);

        if (displaySettings.hdmiPreferences.enabled != enableHdmiOutput) {
            ALOGV("%s: %s Video outputs...", __FUNCTION__, enableHdmiOutput ? "Enabling" : "Disabling");
            displaySettings.hdmiPreferences.enabled      = enableHdmiOutput;
            displaySettings.componentPreferences.enabled = enableHdmiOutput;
            displaySettings.compositePreferences.enabled = enableHdmiOutput;
            rc = NxClient_SetDisplaySettings(&displaySettings);
        }

        if (rc) {
            ALOGE("%s: Could not set display settings [rc=%d]!!!", __FUNCTION__, rc);
        }
    } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);
    return (rc == NEXUS_SUCCESS) ? NO_ERROR : UNKNOWN_ERROR;
}

status_t NexusPower::setPowerState(b_powerState state)
{
    status_t ret = NO_ERROR;
    const uint32_t cecId = 0;   // Hardcoded CEC id to 0

    /* If powering out of standby, then ensure platform is brought up first before CEC,
       otherwise if powering down, then we must send CEC commands first. */
    if (state == ePowerState_S0) {
        // Setup the GPIO output values depending on the power state...
        ret = setGpios(state);

        if (ret != NO_ERROR) {
            ALOGE("%s: Could not set GPIO's for PowerState %s!!!", __FUNCTION__, NexusIPCClientBase::getPowerString(state));
        }
        else if (mIpcClient->setPowerState(state) != true) {
            ALOGE("%s: Could not set PowerState %s!", __FUNCTION__, NexusIPCClientBase::getPowerString(state));
            ret = INVALID_OPERATION;
        }
        else if (mCecDeviceType == eCecDeviceType_eInvalid && NexusIPCCommon::isCecEnabled(cecId) && NexusIPCCommon::getCecTransmitViewOn() == true &&
                                     mIpcClient->setCecPowerState(cecId, state) != true) {
            ALOGW("%s: Could not set CEC%d PowerState %s!", __FUNCTION__, cecId, NexusIPCClientBase::getPowerString(state));
        }
    }
    else {
        if (mCecDeviceType == eCecDeviceType_eInvalid && NexusIPCCommon::isCecEnabled(cecId) && NexusIPCCommon::getCecTransmitStandby() == true &&
                                mIpcClient->setCecPowerState(cecId, state) != true) {
            ALOGW("%s: Could not set CEC%d PowerState %s!", __FUNCTION__, cecId, NexusIPCClientBase::getPowerString(state));
        }

        if (mIpcClient->setPowerState(state) != true) {
            ALOGE("%s: Could not set PowerState %s!", __FUNCTION__, NexusIPCClientBase::getPowerString(state));
            ret = INVALID_OPERATION;
        }
        else {
            // Setup the GPIO output values depending on the power state...
            ret = setGpios(state);

            if (ret != NO_ERROR) {
                ALOGE("%s: Could not set GPIO's for PowerState %d!!!", __FUNCTION__, state);
            }
            else {
                // Finally clear any pending Gpio interrupts to avoid being accidentally woken up again when suspended
                ret = clearGpios();
                if (ret != NO_ERROR) {
                    ALOGE("%s: Could not clear GPIO's for PowerState %d!!!", __FUNCTION__, state);
                }
            }
        }
    }
    return ret;
}

status_t NexusPower::getPowerStatus(b_powerStatus *pPowerStatus)
{
    status_t ret = NO_ERROR;

    if (pPowerStatus == NULL) {
        ALOGE("%s: invalid parameter \"pPowerStatus\"!", __FUNCTION__);
        ret = BAD_VALUE;
    }
    else if (!mIpcClient->getPowerStatus(pPowerStatus)) {
        ret = UNKNOWN_ERROR;
        ALOGE("%s: Could not get power status!", __FUNCTION__);
    }
    else {
        ALOGD("%s: Power state = %s", __FUNCTION__, NexusIPCClientBase::getPowerString(pPowerStatus->pmState));
    }
    return ret;
}

void NexusPower::NexusGpio::gpioCallback(void *context, int param)
{
    NexusPower::NexusGpio *pNexusGpio = reinterpret_cast<NexusPower::NexusGpio *>(context);
    bool enableKeyEvent = !!param;

    if (pNexusGpio != NULL) {
        NEXUS_GpioStatus gpioStatus;

        if (NEXUS_Gpio_GetStatus(pNexusGpio->getHandle(), &gpioStatus) == NEXUS_SUCCESS) {
            ALOGV("\n--> %s: %s interrupt <--\n", __FUNCTION__, pNexusGpio->getPinName().string());

            NEXUS_Gpio_ClearInterrupt(pNexusGpio->getHandle());

            // toggle the configured key, if any
            if (enableKeyEvent && pNexusGpio->mUInput != NULL && pNexusGpio->mKeyEvent) {
                ALOGD("%s: AON GPIO wakeup detected - spoofing wakeup key event...",
                      __FUNCTION__);
                // release->press->release will guarantee that the press event takes place
                pNexusGpio->mUInput->emit_key_state(pNexusGpio->mKeyEvent, false);
                pNexusGpio->mUInput->emit_syn();
                pNexusGpio->mUInput->emit_key_state(pNexusGpio->mKeyEvent, true);
                pNexusGpio->mUInput->emit_syn();
                pNexusGpio->mUInput->emit_key_state(pNexusGpio->mKeyEvent, false);
                pNexusGpio->mUInput->emit_syn();
            }
        }
        else {
            ALOGE("%s: Could NOT get %s interrupt status.", __FUNCTION__, pNexusGpio->getPinName().string());
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

/* The aon_gpio.cfg file is made up of key/value pairs for the GPIO/AON_GPIO/AON_SGPIO's.
   This comprises the name of the GPIO/SGPIO followed by an "=" and additional parameters
   separated by comma(s) and no whitespace within square brackets.  The parameters are
   used to inform the software how to configure the GPIO.  The first parameter is the
   "GPIO mode" of the pin.  The valid values are:

   1) "Input"     --> an input pin
   2) "PushPull"  --> a totem-pole output
   3) "OpenDrain" --> an open-drain output

   The following parameters depend on whether the GPIO is configured as an "input" or "output".

   If the pin is configured as an input, then the second parameter is used to indicate the type
   of "interrupt mode".  The valid values are:

   1) Disabled    --> no interrupt will be generated.
   2) RisingEdge  --> an interrupt will be generated on a rising edge  (i.e. low-->high).
   3) FallingEdge --> an interrupt will be generated on a falling edge (i.e. high-->low).
   4) Edge        --> an interrupt will be generated on either a rising or falling edge.
   5) High        --> an interrupt will be generated on a high level.
   6) Low         --> an interrupt will be generated on a low level.

   The third argument for an input pin is optional. If present, the valid values are:

   1) None        --> (default) no key input events will be generated on an interrupt
   2) Wakeup      --> a KEY_WAKEUP sequence will be generated on an interrupt

   If the pin is configured as an output, then the following 6 parameters are used to indicate
   the polarity of the output signal in power states S0, S0.5, S1, S2, S3, S4, S5 (note S4 is
   not used at this current time).  The valid values are:

   1) High
   2) Low

   Below are some examples of both input and output GPIO configurations.

   Example1: GPIO101 = [PushPull,High,Low,Low,Low,Low,Low,High]
             This configures GPIO_101 as a totem-pole output which is driven HIGH during S0,
             LOW during states S0.5 to S4 and HIGH again for state S5.

   Example2: AON_GPIO01 = [Input,FallingEdge]
             This configures AON_GPIO_01 as a negative edge triggered input.

   Example3: AON_SGPIO02 = [Input,Edge,Wakeup]
             This configures AON_SGPIO_02 as any edge triggered input, generating wakeup key
             events.

   Example4: AON_GPIO00 = [PushPull,High,High,Low,Low,Low,Low,Low]
             This configures AON_GPIO_00 as a totem-pole output which is driven HIGH during S0
             and S0.5 states and LOW in the other standby states S1 to S5.

   Example5: AON_GPIO03 = [OpenDrain,Low,High,High,High,High,High,High]
             This configures AON_GPIO_03 as an open-drain output which is driven LOW during S0
             and HIGH in all other standby states S0.5 to S5.
*/
status_t NexusPower::NexusGpio::loadConfigurationFile(String8 path, PropertyMap **configuration)
{
    status_t status = NAME_NOT_FOUND;

    if (!path.isEmpty()) {
        status = PropertyMap::load(path, configuration);
    }
    return status;
}

status_t NexusPower::NexusGpio::parseGpioKey(String8& keyString, unsigned *pKey)
{
    status_t status = NO_ERROR;
    String8 string(keyString);

    string.toLower();

    if (string == "none") {
        *pKey = KEY_RESERVED;
    }
    else if (string == "wakeup") {
        *pKey = KEY_WAKEUP;
    }
    else {
        *pKey = KEY_RESERVED;
        status = BAD_VALUE;
    }
    return status;
}

status_t NexusPower::NexusGpio::parseGpioMode(String8& pinModeString, NEXUS_GpioMode *pPinMode)
{
    status_t status = NO_ERROR;
    String8 string(pinModeString);

    string.toLower();

    if (string == "input") {
        *pPinMode = NEXUS_GpioMode_eInput;
    }
    else if (string == "pushpull") {
        *pPinMode = NEXUS_GpioMode_eOutputPushPull;
    }
    else if (string == "opendrain") {
        *pPinMode = NEXUS_GpioMode_eOutputOpenDrain;
    }
    else {
        *pPinMode = NEXUS_GpioMode_eMax;
        status = BAD_VALUE;
    }
    return status;
}

status_t NexusPower::NexusGpio::parseGpioInterruptMode(String8& interruptModeString, NEXUS_GpioInterrupt *pPinInterruptMode)
{
    status_t status = NO_ERROR;
    String8 string(interruptModeString);

    string.toLower();

    if (string == "disabled") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eDisabled;
    }
    else if (string == "risingedge") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eRisingEdge;
    }
    else if (string == "fallingedge") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eFallingEdge;
    }
    else if (string == "edge") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eEdge;
    }
    else if (string == "low") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eLow;
    }
    else if (string == "high") {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eHigh;
    }
    else {
        *pPinInterruptMode = NEXUS_GpioInterrupt_eMax;
        status = BAD_VALUE;
    }
    return status;
}

status_t NexusPower::NexusGpio::parseGpioOutputValue(String8& outputValueString, NEXUS_GpioValue *pPinOutputValue)
{
    status_t status = NO_ERROR;
    String8 string(outputValueString);

    string.toLower();

    if (string == "high") {
        *pPinOutputValue = NEXUS_GpioValue_eHigh;
    }
    else if (string == "low") {
        *pPinOutputValue = NEXUS_GpioValue_eLow;
    }
    else {
        *pPinOutputValue = NEXUS_GpioValue_eMax;
        status = BAD_VALUE;
    }
    return status;
}

status_t NexusPower::NexusGpio::parseGpioParameters(String8& inString, size_t *pNumParameters, String8 parameters[])
{
    String8 string(inString);
    const char *charString = string.string();
    ssize_t length = string.length();
    size_t numPars;
    ssize_t absStartPos;
    ssize_t absEndBracketPos;
    ssize_t relCommaPos;

    if (length < 5) {   // "[a,b]" is minimum length
        ALOGW("%s: GPIO parameter section length=%zu is invalid!", __FUNCTION__, length);
        return BAD_VALUE;
    }

    // Check for initial "[" square bracket...
    if (string.find("[") != 0) {
        ALOGW("%s: No \"[\" found at the beginning of the GPIO parameter section!", __FUNCTION__);
        return BAD_VALUE;
    }

    // Check for the final "]" square bracket...
    absEndBracketPos = string.find("]");
    if (absEndBracketPos != length-1) {
        ALOGW("%s: No \"]\" found at the end of the GPIO parameter section!", __FUNCTION__);
        return BAD_VALUE;
    }

    absStartPos = 1;    // Start off by skipping the initial "[" square bracket

    for (numPars = 0; numPars < NexusGpio::MAX_PARAMETERS, absStartPos < absEndBracketPos; numPars++) {
        string.setTo(&charString[absStartPos], length-absStartPos);

        // Check for a ","...
        relCommaPos = string.find(",");
        if (relCommaPos < 0) {
            parameters[numPars] = String8(&charString[absStartPos], absEndBracketPos-absStartPos);
            absStartPos = absEndBracketPos;
        }
        else {
            parameters[numPars] = String8(&charString[absStartPos], relCommaPos);
            absStartPos += relCommaPos + 1;
        }
    }

    if (numPars <= 1) {
        ALOGW("%s: Invalid number of parameters [%zu] provided in the GPIO parameter section!", __FUNCTION__, numPars);
        return BAD_VALUE;
    }
    *pNumParameters = numPars;
    return NO_ERROR;
}

NexusPower::NexusGpio::NexusGpio()
{
    ALOGV("%s: called", __FUNCTION__);
}

// Constructor for a GPIO input...
NexusPower::NexusGpio::NexusGpio(String8& pinName,
                                 unsigned pin,
                                 unsigned pinType,
                                 NEXUS_GpioMode pinMode,
                                 NEXUS_GpioInterrupt pinInterruptMode,
                                 sp<LinuxUInputRef> uInput,
                                 unsigned key) : mPinName(pinName),
                                                 mPin(pin),
                                                 mPinType(pinType),
                                                 mPinMode(pinMode),
                                                 mPinInterruptMode(pinInterruptMode),
                                                 mUInput(uInput),
                                                 mKeyEvent(key)
{
    mInstance = mInstances++;
    ALOGV("%s: instance %d: pin=%d, type=%d, pin mode=%d, interrupt mode=%d, key=%d",
          __PRETTY_FUNCTION__, mInstance, pin, pinType, pinMode, pinInterruptMode, key);
}

// Constructor for a GPIO output...
NexusPower::NexusGpio::NexusGpio(String8& pinName,
                                 unsigned pin,
                                 unsigned pinType,
                                 NEXUS_GpioMode pinMode,
                                 NEXUS_GpioValue *pPinOutputValues) : mPinName(pinName),
                                                                      mPin(pin),
                                                                      mPinType(pinType),
                                                                      mPinMode(pinMode),
                                                                      mPinInterruptMode(NEXUS_GpioInterrupt_eDisabled)
{
    mInstance = mInstances++;
    ALOGV("%s: instance %d: pin=%d, type=%d, pin mode=%d", __PRETTY_FUNCTION__, mInstance, pin, pinType, pinMode);
    for (int cnt = 0; cnt < NexusGpio::MAX_POWER_STATES; cnt++) {
        mPinOutputValues[cnt] = *pPinOutputValues++;
    }
}

// Common destructor for both input and output GPIOs...
NexusPower::NexusGpio::~NexusGpio()
{
    ALOGV("%s: instance %d: pin=%d, type=%d, pin mode=%d", __PRETTY_FUNCTION__, mInstance, mPin, mPinType, mPinMode);
    mInstances--;
    NEXUS_Gpio_Close(mHandle);

    mUInput = NULL;
}

// Factory method for instantiating an input GPIO...
sp<NexusPower::NexusGpio> NexusPower::NexusGpio::instantiate(String8& pinName,
                                                             unsigned pin,
                                                             unsigned pinType,
                                                             NEXUS_GpioMode pinMode,
                                                             NEXUS_GpioInterrupt pinInterruptMode,
                                                             sp<LinuxUInputRef> uInput,
                                                             unsigned key)
{
    sp<NexusPower::NexusGpio> gpio = new NexusGpio(pinName, pin, pinType, pinMode, pinInterruptMode, uInput, key);

    if (gpio.get() != NULL) {
        NEXUS_GpioSettings settings;
        NEXUS_GpioHandle handle;

        NEXUS_Gpio_GetDefaultSettings((NEXUS_GpioType)pinType, &settings);
        settings.mode = pinMode;
        settings.interruptMode = pinInterruptMode;
        settings.maskEdgeInterrupts = true;
        settings.interrupt.callback = NexusPower::NexusGpio::gpioCallback;
        settings.interrupt.context  = gpio.get();
        settings.interrupt.param    = NexusGpio::ENABLE_KEYEVENT;

        handle = NEXUS_Gpio_Open(pinType, pin, &settings);
        if (handle != NULL) {
            ALOGV("%s: Successfully opened %s as an input", __FUNCTION__, pinName.string());
            gpio->setHandle(handle);
        }
        else {
            ALOGE("%s: Could not open %s as an input!!!", __FUNCTION__, pinName.string());
            gpio = NULL;
        }
    }
    return gpio;
}

// Factory method for instantiating an output GPIO...
sp<NexusPower::NexusGpio> NexusPower::NexusGpio::instantiate(b_powerState state, String8& pinName, unsigned pin, unsigned pinType,
                                                             NEXUS_GpioMode pinMode, NEXUS_GpioValue *pPinOutputValues)
{
    sp<NexusPower::NexusGpio> gpio = new NexusGpio(pinName, pin, pinType, pinMode, pPinOutputValues);

    if (gpio.get() != NULL) {
        NEXUS_GpioSettings settings;
        NEXUS_GpioHandle handle;

        NEXUS_Gpio_GetDefaultSettings((NEXUS_GpioType)pinType, &settings);
        settings.mode = pinMode;
        settings.value = gpio->getPinOutputValue(state);

        handle = NEXUS_Gpio_Open(pinType, pin, &settings);
        if (handle != NULL) {
            ALOGV("%s: Successfully opened %s as %s output for PowerState %s", __FUNCTION__, pinName.string(),
                   (settings.value==NEXUS_GpioValue_eLow) ? "LOW" : "HIGH", NexusIPCClientBase::getPowerString(state));
            gpio->setHandle(handle);
        }
        else {
            ALOGE("%s: Could not open %s as %s output for PowerState %s!!!", __FUNCTION__, pinName.string(),
                   (settings.value==NEXUS_GpioValue_eLow) ? "LOW" : "HIGH", NexusIPCClientBase::getPowerString(state));
            gpio = NULL;
        }
    }
    return gpio;
}

sp<NexusPower::NexusGpio> NexusPower::NexusGpio::initialise(b_powerState state, String8& gpioName, String8& gpioValue, int pin,
                                                            unsigned pinType, sp<LinuxUInputRef> uInput)
{
    status_t status;
    sp<NexusGpio> gpio;
    size_t numGpioParameters;
    String8 gpioParameters[NexusGpio::MAX_PARAMETERS];

    ALOGV("%s: %s=%s", __FUNCTION__, gpioName.string(), gpioValue.string());

    status = NexusGpio::parseGpioParameters(gpioValue, &numGpioParameters, gpioParameters);
    if (status != NO_ERROR) {
        ALOGE("%s: Could not parse %s parameters!!!", __FUNCTION__, gpioName.string());
    }
    else if (numGpioParameters < NexusGpio::MIN_PARAMETERS || numGpioParameters > NexusGpio::MAX_PARAMETERS) {
        ALOGE("%s: Invalid number of parameters %zu for %s!!!", __FUNCTION__, numGpioParameters, gpioName.string());
        status = BAD_VALUE;
    }
    else {
        NEXUS_GpioMode gpioMode;
        status = NexusGpio::parseGpioMode(gpioParameters[0], &gpioMode);
        if (status != NO_ERROR) {
            ALOGE("%s: Could not parse %s pin mode!!!", __FUNCTION__, gpioName.string());
        }
        else if (gpioMode == NEXUS_GpioMode_eInput) {
            if (numGpioParameters < NexusGpio::MIN_INP_PARAMETERS ||
                numGpioParameters > NexusGpio::MAX_INP_PARAMETERS) {
                ALOGE("%s: Invalid number of input parameters for %s (must be %d to %d)!!!",
                      __FUNCTION__, gpioName.string(),
                      NexusGpio::MIN_INP_PARAMETERS,
                      NexusGpio::MAX_INP_PARAMETERS);
                status = BAD_VALUE;
            }
            else {
                unsigned key = KEY_RESERVED;
                NEXUS_GpioInterrupt gpioInterruptMode;
                status = NexusGpio::parseGpioInterruptMode(gpioParameters[1], &gpioInterruptMode);
                if (status != NO_ERROR) {
                    ALOGE("%s: Could not parse %s interrupt mode!!!", __FUNCTION__, gpioName.string());
                }
                else if (numGpioParameters > NexusGpio::MIN_INP_PARAMETERS) {
                    status = NexusGpio::parseGpioKey(gpioParameters[2], &key);
                    if (status != NO_ERROR) {
                        ALOGE("%s: Could not parse %s key event!!!",
                              __FUNCTION__, gpioName.string());
                        status = BAD_VALUE;
                    }
                }
                if (status == NO_ERROR) {
                    gpio = NexusGpio::instantiate(gpioName, pin, pinType, gpioMode, gpioInterruptMode, uInput, key);
                    if (gpio.get() == NULL) {
                        ALOGE("%s: Could not instantiate %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
                    }
                    else {
                        ALOGV("%s: Instantiated %s :)", __FUNCTION__, gpioName.string());
                    }
                }
            }
        }
        else {  // Pin is either an open-drain or push-pull output...
            if (numGpioParameters != NexusGpio::NUM_OUT_PARAMETERS) {
                ALOGE("%s: Invalid number of output parameters for %s (must be %d)!!!", __FUNCTION__, gpioName.string(), NexusGpio::NUM_OUT_PARAMETERS);
                status = BAD_VALUE;
            }
            else {
                NEXUS_GpioValue gpioOutputValues[NexusGpio::MAX_POWER_STATES];
                for (unsigned cnt = 0; cnt < sizeof(gpioOutputValues)/sizeof(gpioOutputValues[0]); cnt++) {
                    status = NexusGpio::parseGpioOutputValue(gpioParameters[cnt+1], &gpioOutputValues[cnt]);
                    if (status != NO_ERROR) {
                        break;
                    }
                }

                if (status != NO_ERROR) {
                    ALOGE("%s: Could not parse output values for %s!!!", __FUNCTION__, gpioName.string());
                }
                else {
                    gpio = NexusGpio::instantiate(state, gpioName, pin, pinType, gpioMode, gpioOutputValues);
                    if (gpio.get() == NULL) {
                        ALOGE("%s: Could not instantiate %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
                    }
                    else {
                        ALOGV("%s: Instantiated %s :)", __FUNCTION__, gpioName.string());
                    }
                }
            }
        }
    }
    return gpio;
}

status_t NexusPower::initialiseGpios(b_powerState state)
{
    status_t status;
    String8 path;

    path = NexusGpio::getConfigurationFilePath();
    if (path.isEmpty()) {
        ALOGW("%s: Could not find configuration file \"%s\", so not initialising GPIOs!", __FUNCTION__, path.string());
        status = NAME_NOT_FOUND;
    }
    else {
        PropertyMap* config;
        status = NexusGpio::loadConfigurationFile(path, &config);
        if (status == NO_ERROR) {
            int pin;
            String8 gpioName;
            String8 gpioValue;
            sp<NexusGpio> gpio;
            unsigned cnt = 0;
            size_t numEntries = config->getProperties().size();

            ALOGV("%s: There %s %zu %s in configuration file \"%s\".", __FUNCTION__, (numEntries == 1) ? "is" : "are",
                   numEntries, (numEntries == 1) ? "entry" : "entries", path.string());

            /* Setup standard GPIO custom configuration */
            for (pin = 0; (pin < NEXUS_NUM_GPIO_PINS) && (status == NO_ERROR) &&
                          (cnt < numEntries) && (NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES); pin++) {
                gpioName.setTo("GPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    cnt++;
                    gpio = NexusGpio::initialise(state, gpioName, gpioValue, pin, NEXUS_GpioType_eStandard, mUInput);

                    if (gpio.get() != NULL) {
                        gpios[gpio->getInstance()] = gpio;
                    }
                    else {
                        ALOGE("%s: Could not initialise %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
                    }
                }
                else {
                    gpioName.setTo("GPIO");
                    gpioName.appendFormat("%03d", pin);
                    if (config->tryGetProperty(gpioName, gpioValue)) {
                        cnt++;
                        gpio = NexusGpio::initialise(state, gpioName, gpioValue, pin, NEXUS_GpioType_eStandard, mUInput);

                        if (gpio.get() != NULL) {
                            gpios[gpio->getInstance()] = gpio;
                        }
                        else {
                            ALOGE("%s: Could not initialise %s!!!", __FUNCTION__, gpioName.string());
                            status = NO_INIT;
                        }
                    }
                }
            }

            /* Setup AON_GPIO custom configuration */
            for (pin = 0; (pin < NEXUS_NUM_AON_GPIO_PINS) && (status == NO_ERROR) &&
                          (cnt < numEntries) && (NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES); pin++) {
                gpioName.setTo("AON_GPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    cnt++;
                    gpio = NexusGpio::initialise(state, gpioName, gpioValue, pin, NEXUS_GpioType_eAonStandard, mUInput);

                    if (gpio.get() != NULL) {
                        gpios[gpio->getInstance()] = gpio;
                    }
                    else {
                        ALOGE("%s: Could not initialise %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
                    }
                }
            }

            /* Setup AON_SGPIO custom configuration */
            for (pin = 0; (pin < NEXUS_NUM_AON_SGPIO_PINS) && (status == NO_ERROR) &&
                          (cnt < numEntries) && (NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES); pin++) {
                gpioName.setTo("AON_SGPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    cnt++;
                    gpio = NexusGpio::initialise(state, gpioName, gpioValue, pin, NEXUS_GpioType_eAonSpecial, mUInput);

                    if (gpio.get() != NULL) {
                        gpios[gpio->getInstance()] = gpio;
                    }
                    else {
                        ALOGE("%s: Could not initialise %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
                    }
                }
            }
            if (cnt != numEntries) {
                ALOGE("%s: Error(s) found in configuration file \"%s\"!", __FUNCTION__,  path.string());
                status = NO_INIT;
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

status_t NexusPower::setGpios(b_powerState state)
{
    status_t status = NO_ERROR;
    NEXUS_Error rc;
    sp<NexusGpio> pNexusGpio;

    // Run through the list of GPIO inputs first, as we may need to disable the
    // interrupt prior to setting a GPIO output that could trigger it.
    if (state == ePowerState_S5) {
        for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
            pNexusGpio = gpios[gpio];
            if (pNexusGpio.get() != NULL && pNexusGpio->getPinMode() == NEXUS_GpioMode_eInput) {
                NEXUS_GpioSettings gpioSettings;

                NEXUS_Gpio_GetSettings(pNexusGpio->getHandle(), &gpioSettings);
                gpioSettings.interrupt.param = NexusGpio::DISABLE_KEYEVENT;
                rc = NEXUS_Gpio_SetSettings(pNexusGpio->getHandle(), &gpioSettings);

                if (rc != NEXUS_SUCCESS) {
                    ALOGE("%s: Could not disable interrupt callback for %s [rc=%d]!!!", __FUNCTION__, pNexusGpio->getPinName().string(), rc);
                    status = INVALID_OPERATION;
                }
                else {
                    ALOGV("%s: Successfully disabled interrupt callback for %s", __FUNCTION__, pNexusGpio->getPinName().string());
                }
            }
        }
    }

    for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
        pNexusGpio = gpios[gpio];
        if (pNexusGpio.get() != NULL && pNexusGpio->getPinMode() != NEXUS_GpioMode_eInput) {
            NEXUS_GpioSettings gpioSettings;

            NEXUS_Gpio_GetSettings(pNexusGpio->getHandle(), &gpioSettings);
            gpioSettings.value = pNexusGpio->getPinOutputValue(state);
            rc = NEXUS_Gpio_SetSettings(pNexusGpio->getHandle(), &gpioSettings);
            if (rc != NEXUS_SUCCESS) {
                ALOGE("%s: Could not set %s [rc=%d]!!!", __FUNCTION__, pNexusGpio->getPinName().string(), rc);
                status = INVALID_OPERATION;
            }
            else {
                ALOGV("%s: Successfully set %s to %s", __FUNCTION__, pNexusGpio->getPinName().string(),
                      (gpioSettings.value == NEXUS_GpioValue_eLow) ? "LOW" : "HIGH");
            }
        }
    }
    return status;
}

status_t NexusPower::clearGpios()
{
    status_t status = NO_ERROR;
    NEXUS_Error rc;
    sp<NexusGpio> pNexusGpio;

    for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
        pNexusGpio = gpios[gpio];
        if (pNexusGpio.get() != NULL && pNexusGpio->getPinMode() == NEXUS_GpioMode_eInput) {
            NEXUS_GpioStatus gpioStatus;

            rc = NEXUS_Gpio_ClearInterrupt(pNexusGpio->getHandle());
            if (rc != NEXUS_SUCCESS) {
                ALOGE("%s: Could not clear %s [rc=%d]!!!", __FUNCTION__, pNexusGpio->getPinName().string(), rc);
                status = INVALID_OPERATION;
            }
            else {
                ALOGV("%s: Successfully cleared %s", __FUNCTION__, pNexusGpio->getPinName().string());
            }
        }
    }
    return status;
}
