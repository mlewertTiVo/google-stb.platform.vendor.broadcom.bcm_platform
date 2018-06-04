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
 *****************************************************************************/

#include "nexusirinput.h"

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "NexusIR"
#include <log/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Override for the hardcoded repeatTimeout values in bkir module */
#define PROPERTY_IR_INITIAL_TIMEOUT "ro.nx.ir_remote.initial_timeout"
#define PROPERTY_IR_TIMEOUT "ro.nx.ir_remote.timeout"

#define PROPERTY_BOOT_REASON "sys.boot.reason"
#define BOOT_FROM_DEEP_SLEEP "warm,s3_wakeup"

#define PROPERTY_NX_BOOT_KEY_TWO "dyn.nx.boot.key2"
#define PROPERTY_BOOT_COMPLETED "ro.nx.boot_completed"

NexusIrInput::NexusIrInput() :
        m_handle(0),
        m_observer(0),
        m_power_key(NEXUSIRINPUT_NO_KEY),
        m_power_key_two(NEXUSIRINPUT_NO_KEY),
        m_mask(0),
        m_mask_two(0)
{
}

NexusIrInput::~NexusIrInput()
{
    stop();
}

bool NexusIrInput::start(NEXUS_IrInputMode mode,
        NexusIrInput::Observer &observer, uint32_t power_key, uint64_t mask,
        uint32_t power_key_two, uint64_t mask_two)
{
    ALOGI("NexusIrInput start");

    m_observer = &observer;
    m_power_key = power_key;
    m_power_key_two = power_key_two;
    m_mask = mask;
    m_mask_two = mask_two;

    NEXUS_IrInputSettings irSettings;
    NEXUS_IrInput_GetDefaultSettings(&irSettings);
    irSettings.mode = mode;
    irSettings.dataReady.callback = dataReady;
    irSettings.dataReady.context = this;
    m_handle = NEXUS_IrInput_Open(0, &irSettings);

    if (!m_handle) {
        ALOGE("NexusIrInput start failed!");
    }
    else {
        /* Determine if the system is woken up by the wakeup key */
        char value[PROPERTY_VALUE_MAX];
        property_get(PROPERTY_BOOT_REASON, value, "");
        if (strlen(value) > 0 && !strncmp(value, BOOT_FROM_DEEP_SLEEP, strlen(value))) {
            NEXUS_IrInputEvent event;
            if (NEXUS_IrInput_ReadEvent(m_handle, &event) == NEXUS_SUCCESS && event.code == m_power_key_two) {
                ALOGV("Woken up by power key two");
                property_set(PROPERTY_NX_BOOT_KEY_TWO, "1");
            }
        }

        NEXUS_IrInputDataFilter irPattern;

        NEXUS_IrInput_GetDefaultDataFilter(&irPattern );
        irPattern.filterWord[0].patternWord = m_power_key;
        irPattern.filterWord[0].mask = m_mask;
        irPattern.filterWord[0].enabled = (m_power_key != NEXUSIRINPUT_NO_KEY);
        irPattern.filterWord[1].patternWord = m_power_key_two;
        irPattern.filterWord[1].mask = m_mask_two;
        irPattern.filterWord[1].enabled = (m_power_key_two != NEXUSIRINPUT_NO_KEY);
        NEXUS_IrInput_SetDataFilter(m_handle, &irPattern);
    }
    return m_handle != 0;
}

void NexusIrInput::stop()
{
    if (m_handle)
    {
        ALOGI("NexusIrInput stop");
        NEXUS_IrInput_Close(m_handle);
        m_handle = 0;
    }
    m_observer = 0;
}

unsigned NexusIrInput::initialRepeatTimeout()
{
    unsigned timeout = property_get_int32(PROPERTY_IR_INITIAL_TIMEOUT, 0);
    if (timeout) {
        return timeout;
    }
    return repeatTimeout();
}

unsigned NexusIrInput::repeatTimeout()
{
    unsigned timeout = property_get_int32(PROPERTY_IR_TIMEOUT, 0);
    if (timeout) {
        return timeout;
    }

    NEXUS_IrInputSettings irSettings;
    NEXUS_IrInput_GetSettings(m_handle, &irSettings);

    if (!irSettings.customSettings.repeatTimeout) {
        NEXUS_IrInput_GetCustomSettingsForMode(irSettings.mode,
                &irSettings.customSettings);
    }
    return irSettings.customSettings.repeatTimeout;
}

/*static*/ void NexusIrInput::dataReady(void *context, int param)
{
    (void)param;
    static_cast<NexusIrInput *>(context)->dataReady();
}

void NexusIrInput::dataReady()
{
    size_t numEvents = 1;
    NEXUS_Error rc = 0;
    bool overflow;

    while (numEvents && !rc)
    {
        NEXUS_IrInputEvent irEvent;
        rc = NEXUS_IrInput_GetEvents(m_handle, &irEvent, 1, &numEvents,
                &overflow);
        if (numEvents)
        {
            ALOGV("Nexus IR callback: rc: %d, code: %08x, mask: %llx, repeat: %s, interval: %u",
               (int)rc, (unsigned) irEvent.code, (unsigned long long)m_mask,
               irEvent.repeat ? "true" : "false",
               irEvent.interval);
            m_observer->onIrInput(irEvent.code & ~m_mask, irEvent.repeat,
                    irEvent.interval);
        }
    }
}
