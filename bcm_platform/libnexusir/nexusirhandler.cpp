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

#include "nexusirhandler.h"
#include "nexusirinput.h"
#include "nexusirmap.h"

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "NexusIR"
#include <cutils/log.h>

NexusIrHandler::NexusIrHandler() :
        m_mode(NEXUS_IrInputMode_eRemoteA), m_map(0), m_ir(), m_uinput()
{
}

NexusIrHandler::~NexusIrHandler()
{
    stop();
}

bool NexusIrHandler::start(NEXUS_IrInputMode mode,
        android::sp<NexusIrMap> map)
{
    static const struct input_id id =
    {
    BUS_HOST, /* bustype */
    0, /* vendor */
    0, /* product */
    1  /* version */
    };

    LOGI("NexusIrHandler start");

    m_mode = mode;
    m_map = map;
    bool success = m_map.get() && m_uinput.open() &&
        m_uinput.register_event(EV_KEY);

    size_t size = m_map->size();
    uint32_t power_key = NEXUSIRINPUT_NO_POWER_KEY;
    for (size_t i = 0; success && (i < size); ++i)
    {
        if (m_map->linuxKeyAt(i) == KEY_POWER) {
            power_key = m_map->nexusIrCodeAt(i);
        }
        success = success && m_uinput.register_key(m_map->linuxKeyAt(i));
        //Note: success == false breaks the loop
    }
    success = success
            && m_uinput.start("NexusIrHandler", id)
            && m_ir.start(m_mode, *this, power_key);

    if (!success)
    {
        LOGE("NexusIrHandler::start failed!\n");
        stop();
    }

    LOGI("NexusIrHandler::start returns %s\n", success ? "true" : "false");
    return success;
}

void NexusIrHandler::stop()
{
    LOGI("NexusIrHandler::stop\n");
    m_ir.stop();
    m_uinput.stop();
    m_uinput.close();
    m_map = 0;
}

void NexusIrHandler::enterStandby()
{
    m_ir.enterStandby();
}

void NexusIrHandler::exitStandby()
{
    m_ir.exitStandby();
}

/*virtual*/ void NexusIrHandler::onIrInput(uint32_t nexus_key, bool repeat)
{
    LOGV("NexusIrHandler: got Nexus key %u%s\n", (unsigned)nexus_key,
            repeat ? " (repeat)" : "");
    if (repeat)
        return; //ignore repeat keys

    __u16 linux_key = m_map->get(nexus_key);
    if (linux_key != KEY_RESERVED) {
        LOGV("NexusIrHandler: emit Linux key event %d\n", (int)linux_key);
        m_uinput.emit_key(linux_key);
    } else {
        LOGW("No Linux key mapping for Nexus IR code %u\n", (unsigned)nexus_key);
    }
}

