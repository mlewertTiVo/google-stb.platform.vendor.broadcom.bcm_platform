/***************************************************************************
*     (c)2004-2014 Broadcom Corporation
*
*  This program is the proprietary software of Broadcom Corporation and/or its licensors,
*  and may only be used, duplicated, modified or distributed pursuant to the terms and
*  conditions of a separate, written license agreement executed between you and Broadcom
*  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
*  no license (express or implied), right to use, or waiver of any kind with respect to the
*  Software, and Broadcom expressly reserves all rights in and to the Software and all
*  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
*  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
*  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
*  Except as expressly set forth in the Authorized License,
*
*  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
*  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
*  and to use this information only in connection with your use of Broadcom integrated circuit products.
*
*  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
*  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
*  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
*  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
*  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
*  USE OR PERFORMANCE OF THE SOFTWARE.
*
*  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
*  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
*  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
*  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
*  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
*  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
*  ANY LIMITED REMEDY.
*
* $brcm_Workfile: $
* $brcm_Revision: $
* $brcm_Date: $
*
* API Description:
*   API name: Gpio
*    Specific APIs related to Gpio Control.
*
* Revision History:
*
* $brcm_Log: $
*
***************************************************************************/
#ifndef NEXUS_GPIO_H__
#define NEXUS_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/*=*********************
GPIO stands for General Purpose Input/Output.
GPIO pins are used for external communication to and from the chip.

A GPIO pin can be configured for output or input mode.
In output mode, a value can be set.
In input mode, a value can be read and callback can be received (based on an interrupt) when the value changes.

See nexus/examples/io/gpio.c for an example application.
***********************/

/***************************************************************************
Summary:
GPIO pin type macros for first param of NEXUS_Gpio_Open(TYPE, PIN, pSettings)

Description:
Chip-specific GPIO types are coded as macros to nexus_platform_features.h.
These types may be common across certain families of chips, but will vary among chips over time.
GPIO type values must be matched against the GPIO types implemented in the chip-specific nexus_gpio_table.c file.

The following macros are provided for backward-compatibility to the NEXUS_GpioType enumeration.
***************************************************************************/
#define NEXUS_GpioType unsigned
#define NEXUS_GpioType_eStandard 0
#define NEXUS_GpioType_eSpecial  1
#define NEXUS_GpioType_eTvMicro  2
#define NEXUS_GpioType_eMax      3

/***************************************************************************
Summary:
GPIO pin modes

Description:
NEXUS_GpioMode is an enumeration which represents all GPIO pin modes
supported by the GPIO interface.

There are three GPIO pin types. They are input, push-pull output, or
open-drain output. A standard GPIO could be programmed to be any one of
the three types, and a special GPIO pin could only work in type of input
and open-drain output.

When programmed to be type of input, it can only be read from, and will
not drive the bus or the port connected to it. The value read back could
0 (low) or 1 (high).

During working as type of push-pull output, it is ideally the unique
driver of the connected bus or port, till the type changes. The value
to drive out is either 0 (low) or 1 (high). It could change by setting
the value of the GPIO pin. However, it could also be read from, in order
to monitor the activity. Driving 0 out is also called pull-down, and
driving 1 out is called push-up.

A pin of open drain output type is typically connected to a wired-AND
bus line. The line is high (1) if no one pulls it down, and if any
device pulls down it becomes low (0). The application program could
read it and pulling it down (and then releasing it).

Please notice that the type could be dynamically changed by software. It
is possible that the application program sets the GPIO pin as input type,
reads a few bits, and then changes the pin type to push-pull output and
then writes a few bits, and so on.

Used in NEXUS_GpioSettings
***************************************************************************/
typedef enum NEXUS_GpioMode
{
    NEXUS_GpioMode_eInput,
    NEXUS_GpioMode_eOutputOpenDrain,
    NEXUS_GpioMode_eOutputPushPull,
    NEXUS_GpioMode_eMax
} NEXUS_GpioMode;

/***************************************************************************
Summary:
The GPIO pin value

Description:
There are two possible values Low (0) and High (0) for a GPIO pin.
NEXUS_GpioValue is an enumeration which represents them.

A push-pull GPIO pin could drive both 0 (low) and 1 (high) out to the
bus or connection port wired to this GPIO pin. A open-drain GPIO pin
could pull-down the connected AND-wired bus line (0), till it releases
it (1).

The value of any GPIO pin could be read at any time. If a push-pull GPIO
pin is read, its value would be the value it drives out. When a open-
drain GPIO pin pulls-down, the value of 0 (low) will be returned by
reading. After it releases the bus, the value read from the pin depends
whether there is any other device on the bus that is pulling down the
bus.

Used in NEXUS_GpioSettings and NEXUS_GpioStatus
***************************************************************************/
typedef enum NEXUS_GpioValue
{
    NEXUS_GpioValue_eLow = 0,         /* low, driving low out means pull-down */
    NEXUS_GpioValue_eHigh = 1,        /* high, driven for PushPull - released for OpenDrain */
    NEXUS_GpioValue_eMax
} NEXUS_GpioValue;

/***************************************************************************
Summary:
GPIO pin interrupt mode

Description:
NEXUS_GpioInterrupt is an enumeration which represents whether an
interrupt is disabled or enabled for a GPIO pin, and in what condition
the GPIO pin should generate interrupt if it is enabled.

Some chips only support edge triggering interrupt mode. In this mode the
pin fires interrupt when its value changes.  By default, the interrupt will remain
enabled after firing. See NEXUS_GpioSettings.maskEdgeInterrupts for another option.

Besides edge triggering mode, some chips also support level
triggering. This corresponds to NEXUS_GpioInterrupt_eLow and eHigh.
In this mode the pin drives the interrupt bit when it has the
specified value. To clear a level-triggered interrupt and reenable interrupts,
the user must call NEXUS_Gpio_ClearInterrupt.

Used in NEXUS_GpioSettings
***************************************************************************/
typedef enum NEXUS_GpioInterrupt
{
    NEXUS_GpioInterrupt_eDisabled,      /* No interrupt */
    NEXUS_GpioInterrupt_eRisingEdge,    /* Interrupt on a 0->1 transition */
    NEXUS_GpioInterrupt_eFallingEdge,   /* Interrupt on a 1->0 transition */
    NEXUS_GpioInterrupt_eEdge,          /* Interrupt on both a 0->1 and a 1->0 transition */
    NEXUS_GpioInterrupt_eLow,           /* Interrupt on a 0 value */
    NEXUS_GpioInterrupt_eHigh,          /* Interrupt on a 1 value */
    NEXUS_GpioInterrupt_eMax
} NEXUS_GpioInterrupt;

/***************************************************************************
Summary:
GPIO pin settings
***************************************************************************/
typedef struct NEXUS_GpioSettings
{
    NEXUS_GpioMode mode;               /* Set if this is an input or output pin, and what type of output pin. */
    NEXUS_GpioValue value;             /* Used to set the value of an output pin. */
    NEXUS_GpioInterrupt interruptMode; /* Set the interrupt mode of an input pin. */
    NEXUS_CallbackDesc interrupt;      /* Callback when value is changed in input mode. */
    bool maskEdgeInterrupts; /* If true, all GPIO interrupts will be masked when processed. The user must call NEXUS_Gpio_ClearInterrupt to unmask.
                                If false, edge-triggered (default) are not masked. Level-triggered interrupts are always masked. */
} NEXUS_GpioSettings;

/***************************************************************************
Summary:
Get default settings for the structure.

Description:
This is required in order to make application code resilient to the addition of new structure members in the future.
***************************************************************************/
void NEXUS_Gpio_GetDefaultSettings(
    NEXUS_GpioType type, /* unused */
    NEXUS_GpioSettings *pSettings /* [out] */
    );

/***************************************************************************
Summary:
Open a GPIO Pin.

Description:
Users should call NEXUS_Gpio_Open, not NEXUS_Gpio_OpenAux.
The internal NEXUS_Gpio_OpenAux implementation is required to marshall callbacks through a proxy layer.
***************************************************************************/
#define NEXUS_Gpio_Open(NEXUS_GPIOTYPE,PIN_NUMBER,PSETTINGS) NEXUS_Gpio_OpenAux((NEXUS_GPIOTYPE)<<16|(PIN_NUMBER),PSETTINGS)

NEXUS_GpioHandle NEXUS_Gpio_OpenAux( /* attr{destructor=NEXUS_Gpio_Close} */
    unsigned typeAndPin, /* The number and type of the pin. These values are muxed together by the
                            NEXUS_Gpio_Open macro for kernel mode proxy support. */
    const NEXUS_GpioSettings *pSettings /* attr{null_allowed=y} additional settings for the pin.
                    If NULL, current hardware settings are inherited. Call GetSettings to learn them. */
    );

/***************************************************************************
Summary:
Close a GPIO Pin.
***************************************************************************/
void NEXUS_Gpio_Close(
    NEXUS_GpioHandle handle
    );

/***************************************************************************
Summary:
Set the settings for a GPIO pin.
***************************************************************************/
NEXUS_Error NEXUS_Gpio_SetSettings(
    NEXUS_GpioHandle handle,
    const NEXUS_GpioSettings *pSettings
    );

/***************************************************************************
Summary:
Get the settings for a GPIO pin.

Description:
This is not the same as NEXUS_Gpio_GetStatus.
This returns whatever was last set with NEXUS_Gpio_SetSettings,
but the value may not be the actual value on a GPIO output pin.
***************************************************************************/
void NEXUS_Gpio_GetSettings(
    NEXUS_GpioHandle handle,
    NEXUS_GpioSettings *pSettings     /* [out] */
    );

/***************************************************************************
Summary:
Status of a GPIO pin.
***************************************************************************/
typedef struct NEXUS_GpioStatus
{
    NEXUS_GpioValue value; /* The data value of the pin.
                              If the pin is in output mode, this will be the value set by NEXUS_GpioSettings.value.
                              If the pin is in input mode, this will be the current value being driven to this pin. */
    bool interruptPending; /* True if the interrupt callback has fired and NEXUS_Gpio_ClearInterrupt has not been cleared. */
} NEXUS_GpioStatus;

/***************************************************************************
Summary:
Get the status of a GPIO pin.
***************************************************************************/
NEXUS_Error NEXUS_Gpio_GetStatus(
    NEXUS_GpioHandle handle,
    NEXUS_GpioStatus *pStatus     /* [out] */
    );

/***************************************************************************
Summary:
Clear a pending interrupt on a GPIO pin.

Description:
This call must be made after the NEXUS_GpioSettings.interrupt callback to receive the next interrupt callback.
***************************************************************************/
NEXUS_Error NEXUS_Gpio_ClearInterrupt(
    NEXUS_GpioHandle handle
    );

/***************************************************************************
Summary:
Get RDB pin mux information about GPIO pins

Description:
Nexus will not change the pinmux itself because it doesn't know dependencies
and what harm may result. But Nexus does provide pinmux information so the
app can change pinmux easily if it knows it is safe.

Example code:

    NEXUS_Gpio_GetPinMux(NEXUS_GpioType_eStandard<<16|1, &addr, &mask, &shift);
    value = NEXUS_Platform_ReadRegister(addr);
    value &= ~mask; // set to zero to enable GPIO
    NEXUS_Platform_WriteRegister(addr, value);

Returns a failure if the typeAndPin is invalid.
***************************************************************************/
NEXUS_Error NEXUS_Gpio_GetPinMux(
    unsigned typeAndPin, /* see NEXUS_Gpio_OpenAux for definition */
    uint32_t *pAddr, /* [out] pinmux register address */
    uint32_t *pMask, /* [out] bit mask for pinmux register (no shift required) */
    unsigned *pShift /* [out] bit shift for pinmux register */
    );

#ifdef __cplusplus
}
#endif

#endif /* #ifndef NEXUS_GPIO_H__ */
