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
            !uInput->register_key(KEY_POWER) ||
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

NexusPower::NexusPower() : deviceType(-1), mIpcClient(NULL), mClientContext(NULL)
{
    ALOGV("%s: Called", __PRETTY_FUNCTION__);
}

NexusPower::NexusPower(NexusIPCClientBase *pIpcClient, NexusClientContext *pClientContext) : mIpcClient(pIpcClient), mClientContext(pClientContext)
{
    ALOGV("%s: pIpcClient=%p, pClientContext=%p", __PRETTY_FUNCTION__, (void *)pIpcClient, (void *)pClientContext);
    deviceType = getDeviceType();

    mUInput = NexusPower::LinuxUInputRef::instantiate();
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

bool NexusPower::getCecTransmitStandby()
{
    char value[PROPERTY_VALUE_MAX];
    bool tx=false;

    if (property_get(PROPERTY_HDMI_TX_STANDBY_CEC, value, DEFAULT_PROPERTY_HDMI_TX_STANDBY_CEC)) {
        tx = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return tx;
}

bool NexusPower::getCecTransmitViewOn()
{
    char value[PROPERTY_VALUE_MAX];
    bool tx=false;

    if (property_get(PROPERTY_HDMI_TX_VIEW_ON_CEC, value, DEFAULT_PROPERTY_HDMI_TX_VIEW_ON_CEC)) {
        tx = (strncmp(value, "1", PROPERTY_VALUE_MAX) == 0);
    }
    return tx;
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
            ALOGE("%s: Could not set GPIO's for PowerState %d!!!", __FUNCTION__, state);
        }
        else if (mIpcClient->setPowerState(state) != true) {
            ALOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
            ret = INVALID_OPERATION;
        }
        else if (deviceType == -1 && mIpcClient->isCecEnabled(cecId) && getCecTransmitViewOn() == true &&
                                     mIpcClient->setCecPowerState(cecId, state) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
            ret = INVALID_OPERATION;
        }
    }
    else {
        if (deviceType == -1 && mIpcClient->isCecEnabled(cecId) && getCecTransmitStandby() == true &&
                                mIpcClient->setCecPowerState(cecId, state) != true) {
            ALOGE("%s: Could not set CEC%d PowerState %d!", __FUNCTION__, cecId, state);
            ret = INVALID_OPERATION;
        }

        if (mIpcClient->setPowerState(state) != true) {
            ALOGE("%s: Could not set PowerState %d!", __FUNCTION__, state);
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

status_t NexusPower::getPowerState(b_powerState *pState)
{
    if (pState == NULL) {
        ALOGE("%s: invalid parameter \"pState\"!", __FUNCTION__);
        return BAD_VALUE;
    }

    *pState = mIpcClient->getPowerState();
    ALOGD("%s: Power state = %d", __FUNCTION__, *pState);
    return NO_ERROR;
}

void NexusPower::NexusGpio::gpioCallback(void *context, int param __unused)
{
    NexusPower::NexusGpio *pNexusGpio = reinterpret_cast<NexusPower::NexusGpio *>(context);

    if (pNexusGpio != NULL) {
        NEXUS_GpioStatus gpioStatus;

        if (NEXUS_Gpio_GetStatus(pNexusGpio->getHandle(), &gpioStatus) == NEXUS_SUCCESS) {
            ALOGV("%s: %s interrupt.", __FUNCTION__, pNexusGpio->getPinName().string());

            NEXUS_Gpio_ClearInterrupt(pNexusGpio->getHandle());

            // toggle the power key only when needed
            if (pNexusGpio->mUInput != NULL && pNexusGpio->mPowerKeyEvent)
            {
                // release->press->release will guarantee that the press event takes place
                pNexusGpio->mUInput->emit_key_state(KEY_POWER, false);
                pNexusGpio->mUInput->emit_syn();
                pNexusGpio->mUInput->emit_key_state(KEY_POWER, true);
                pNexusGpio->mUInput->emit_syn();
                pNexusGpio->mUInput->emit_key_state(KEY_POWER, false);
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

   The second parameter depends on whether the GPIO is configurred as an "input" or "output".

   If the pin is configured as an input, then the second parameter is used to indicate the type
   of "interrupt mode".  The valid value are:

   1) Disabled    --> no interrupt will be generated.
   2) RisingEdge  --> an interrupt will be generated on a rising edge  (i.e. low-->high).
   3) FallingEdge --> an interrupt will be generated on a falling edge (i.e. high-->low).
   4) Edge        --> an interrupt will be generated on either a rising or falling edge.
   5) High        --> an interrupt will be generated on a high level.
   6) Low         --> an interrupt will be generated on a low level.

   If the pin is configurred as an output, then the following 6 parameters are used to indicate
   the polarity of the output signal in power states S0 through to S5 (note S4 is not used at
   this current time).  The valid values are:

   1) High
   2) Low

   Below are some examples of both input and output GPIO configurations.

   Example1: GPIO101 = [PushPull,High,High,Low,Low,Low,High]
             So this configures GPIO_101 as a totem-pole output which is driven HIGH during S0,
             LOW during states S1 to S4 and HIGH again for state S5

   Example2: AON_GPIO01 = [Input,FallingEdge]
             This configures AON_GPIO_01 as a negative edge triggered input

   Example3: AON_SGPIO02 = [Input,Edge]
             this configures AON_SGPIO_02 as any edge triggered input

   Example4: AON_GPIO00 = [PushPull,High,High,Low,Low,Low,Low]
             So this configures AON_GPIO_00 as a totem-pole output which is driven HIGH during S0
             and S1 states and LOW in the other standby states S2 to S5.

   Example5: AON_GPIO03 = [OpenDrain,Low,High,High,High,High,High]
             So this configures AON_GPIO_03 as an open-drain output which is driven LOW during S0
             and HIGH in all other standby states S1 to S5.
*/
status_t NexusPower::NexusGpio::loadConfigurationFile(String8 path, PropertyMap **configuration)
{
    status_t status = NAME_NOT_FOUND;

    if (!path.isEmpty()) {
        status = PropertyMap::load(path, configuration);
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
        ALOGW("%s: GPIO parameter section length=%d is invalid!", __FUNCTION__, length);
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
        ALOGW("%s: Invalid number of parameters [%d] provided in the GPIO parameter section!", __FUNCTION__, numPars);
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
NexusPower::NexusGpio::NexusGpio(String8& pinName, unsigned pin, unsigned pinType, NEXUS_GpioMode pinMode, NEXUS_GpioInterrupt pinInterruptMode, sp<LinuxUInputRef> uInput) :
                                                                                        mPinName(pinName),
                                                                                        mPin(pin),
                                                                                        mPinType(pinType),
                                                                                        mPinMode(pinMode),
                                                                                        mPinInterruptMode(pinInterruptMode),
                                                                                        mUInput(uInput)
{
    mInstance = mInstances++;
    ALOGV("%s: instance %d: pin=%d, type=%d, pin mode=%d, interrupt mode=%d", __PRETTY_FUNCTION__, mInstance, pin, pinType, pinMode, pinInterruptMode);
}

// Constructor for a GPIO output...
NexusPower::NexusGpio::NexusGpio(String8& pinName, unsigned pin, unsigned pinType, NEXUS_GpioMode pinMode, NEXUS_GpioValue *pPinOutputValues) :
                                                                                        mPinName(pinName),
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
sp<NexusPower::NexusGpio> NexusPower::NexusGpio::instantiate(String8& pinName,  unsigned pin, unsigned pinType,
                                                             NEXUS_GpioMode pinMode, NEXUS_GpioInterrupt pinInterruptMode, sp<LinuxUInputRef> uInput)
{
    sp<NexusPower::NexusGpio> gpio = new NexusGpio(pinName, pin, pinType, pinMode, pinInterruptMode, uInput);

    if (gpio.get() != NULL) {
        NEXUS_GpioSettings settings;
        NEXUS_GpioHandle handle;

        NEXUS_Gpio_GetDefaultSettings(pinType, &settings);
        settings.mode = pinMode;
        settings.interruptMode = pinInterruptMode;
        settings.maskEdgeInterrupts = true;
        settings.interrupt.callback = NexusPower::NexusGpio::gpioCallback;
        settings.interrupt.context  = gpio.get();
        settings.interrupt.param    = pin;

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
sp<NexusPower::NexusGpio> NexusPower::NexusGpio::instantiate(String8& pinName, unsigned pin, unsigned pinType, NEXUS_GpioMode pinMode, NEXUS_GpioValue *pPinOutputValues)
{
    sp<NexusPower::NexusGpio> gpio = new NexusGpio(pinName, pin, pinType, pinMode, pPinOutputValues);

    if (gpio.get() != NULL) {
        NEXUS_GpioSettings settings;
        NEXUS_GpioHandle handle;

        NEXUS_Gpio_GetDefaultSettings(pinType, &settings);
        settings.mode = pinMode;
        settings.value = pPinOutputValues[0];

        handle = NEXUS_Gpio_Open(pinType, pin, &settings);
        if (handle != NULL) {
            ALOGV("%s: Successfully opened %s as an output", __FUNCTION__, pinName.string());
            gpio->setHandle(handle);
        }
        else {
            ALOGE("%s: Could not open %s as an output!!!", __FUNCTION__, pinName.string());
            gpio = NULL;
        }
    }
    return gpio;
}

sp<NexusPower::NexusGpio> NexusPower::NexusGpio::initialise(String8& gpioName, String8& gpioValue, int pin, unsigned pinType, sp<LinuxUInputRef> uInput)
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
        ALOGE("%s: Invalid number of parameters %d for %s!!!", __FUNCTION__, gpioName.string(), numGpioParameters);
        status = BAD_VALUE;
    }
    else {
        NEXUS_GpioMode gpioMode;
        status = NexusGpio::parseGpioMode(gpioParameters[0], &gpioMode);
        if (status != NO_ERROR) {
            ALOGE("%s: Could not parse %s pin mode!!!", __FUNCTION__, gpioName.string());
        }
        else if (gpioMode == NEXUS_GpioMode_eInput) {
            if (numGpioParameters != NexusGpio::NUM_INP_PARAMETERS) {
                ALOGE("%s: Invalid number of input parameters for %s (must be %d)!!!", __FUNCTION__, gpioName.string(), NexusGpio::NUM_INP_PARAMETERS);
                status = BAD_VALUE;
            }
            else {
                NEXUS_GpioInterrupt gpioInterruptMode;
                status = NexusGpio::parseGpioInterruptMode(gpioParameters[1], &gpioInterruptMode);
                if (status != NO_ERROR) {
                    ALOGE("%s: Could not parse %s interrupt mode!!!", __FUNCTION__, gpioName.string());
                }
                else {
                    gpio = NexusGpio::instantiate(gpioName, pin, pinType, gpioMode, gpioInterruptMode, uInput);
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
                    gpio = NexusGpio::instantiate(gpioName, pin, pinType, gpioMode, gpioOutputValues);
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

status_t NexusPower::initialiseGpios()
{
    status_t status;
    String8 path;

    path = NexusGpio::getConfigurationFilePath();
    if (path.isEmpty()) {
        ALOGW("%s: Could not find configuration file, so not initialising GPIOs!", __FUNCTION__, path.string());
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

            /* Setup standard GPIO custom configuration */
            for (pin = 0; pin < NEXUS_NUM_GPIO_PINS && (status == NO_ERROR) && NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES; pin++) {
                gpioName.setTo("GPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    gpio = NexusGpio::initialise(gpioName, gpioValue, pin, NEXUS_GpioType_eStandard, mUInput);

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
                        gpio = NexusGpio::initialise(gpioName, gpioValue, pin, NEXUS_GpioType_eStandard, mUInput);

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
            for (pin = 0; pin < NEXUS_NUM_AON_GPIO_PINS && (status == NO_ERROR) && NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES; pin++) {
                gpioName.setTo("AON_GPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    gpio = NexusGpio::initialise(gpioName, gpioValue, pin, NEXUS_GpioType_eAonStandard, mUInput);

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
            for (pin = 0; pin < NEXUS_NUM_AON_SGPIO_PINS && (status == NO_ERROR) && NexusGpio::getInstances() < NexusGpio::MAX_INSTANCES; pin++) {
                gpioName.setTo("AON_SGPIO");
                gpioName.appendFormat("%02d", pin);
                if (config->tryGetProperty(gpioName, gpioValue)) {
                    gpio = NexusGpio::initialise(gpioName, gpioValue, pin, NEXUS_GpioType_eAonSpecial, mUInput);

                    if (gpio.get() != NULL) {
                        gpios[gpio->getInstance()] = gpio;
                    }
                    else {
                        ALOGE("%s: Could not initialise %s!!!", __FUNCTION__, gpioName.string());
                        status = NO_INIT;
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

status_t NexusPower::setGpios(b_powerState state)
{
    status_t status = NO_ERROR;
    NEXUS_Error rc;
    sp<NexusGpio> pNexusGpio;

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

status_t NexusPower::setGpioPowerKeyEvent(bool enable)
{
    status_t status = NO_ERROR;
    NEXUS_Error rc;
    sp<NexusGpio> pNexusGpio;

    for (unsigned gpio = 0; gpio < NexusGpio::MAX_INSTANCES; gpio++) {
        pNexusGpio = gpios[gpio];
        if (pNexusGpio.get() != NULL && pNexusGpio->getPinMode() == NEXUS_GpioMode_eInput &&
                pNexusGpio->getPinInterruptMode() != NEXUS_GpioInterrupt_eDisabled) {

            pNexusGpio->setPowerKeyEvent(enable);
        }
    }
    return status;
}
