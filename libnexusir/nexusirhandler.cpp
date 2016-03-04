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

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "NexusIR"

#include "nexusirhandler.h"
#include "nexusirinput.h"
#include "nexusirmap.h"

#include <cutils/log.h>

#define MS_TO_NS(t) (nsecs_t(t) * 1000 * 1000)
#define NS_TO_MS(t) (nsecs_t(t) / (1000 * 1000))
#define DEFAULT_TIMEOUT MS_TO_NS(250)

NexusIrHandler::NexusIrHandler() :
        m_mode(NEXUS_IrInputMode_eRemoteA),
        m_ir(),
        m_uinput(),
        m_key_thread(m_uinput)
{
}

NexusIrHandler::~NexusIrHandler()
{
    stop();
}

bool NexusIrHandler::start(NEXUS_IrInputMode mode,
        android::sp<NexusIrMap> map, uint64_t mask)
{
    static const struct input_id id =
    {
    BUS_HOST, /* bustype */
    0, /* vendor */
    0, /* product */
    1  /* version */
    };

    ALOGI("NexusIrHandler start");

    m_mode = mode;
    bool success = map.get() && m_uinput.open() &&
    m_uinput.register_event(EV_KEY);

    size_t size = map->size();
    uint32_t power_key = NEXUSIRINPUT_NO_KEY;
    for (size_t i = 0; success && (i < size); ++i)
    {
        if (map->linuxKeyAt(i) == KEY_POWER) {
            power_key = map->nexusIrCodeAt(i);
        }
        success = success && m_uinput.register_key(map->linuxKeyAt(i));
        //Note: success == false breaks the loop
    }
    success = success
            && m_uinput.start("NexusIrHandler", id)
            && m_ir.start(m_mode, *this, power_key, mask);

    m_key_thread.setMap(map);
    m_key_thread.setInitialTimeout(m_ir.initialRepeatTimeout());
    m_key_thread.setTimeout(m_ir.repeatTimeout());
    success = success
            && (m_key_thread.run() == android::NO_ERROR);

    if (!success)
    {
        ALOGE("NexusIrHandler::start failed!\n");
        stop();
    }

    ALOGI("NexusIrHandler::start returns %s\n", success ? "true" : "false");
    return success;
}

void NexusIrHandler::stop()
{
    ALOGI("NexusIrHandler::stop\n");

    m_key_thread.requestExit();
    m_key_thread.signal(NEXUSIRINPUT_NO_KEY, false, 0);
    m_key_thread.join();
    m_key_thread.setMap(0);

    m_ir.stop();
    m_uinput.stop();
    m_uinput.close();
}

/*virtual*/ void NexusIrHandler::onIrInput(uint32_t nexus_key, bool repeat,
        unsigned interval)
{
    ALOGV("NexusIrHandler: got Nexus key 0x%08x%s, interval: %u",
            (unsigned)nexus_key, repeat ? " (repeat)" : "", interval);

    m_key_thread.signal(nexus_key, repeat, interval);
}

NexusIrHandler::KeyThread::KeyThread(LinuxUInput &uinput) :
        m_map(0),
        m_uinput(uinput),
        m_mutex("NexusIrHandler::KeyThread::m_mutex"),
        m_cond(android::Condition::WAKE_UP_ONE),
        m_old_key(KEY_RESERVED),
        m_key(KEY_RESERVED),
        m_repeat(false),
        m_interval(0),
        m_initial_timeout(DEFAULT_TIMEOUT),
        m_timeout(DEFAULT_TIMEOUT)
{
}

NexusIrHandler::KeyThread::~KeyThread()
{
}

void NexusIrHandler::KeyThread::setMap(android::sp<NexusIrMap> map)
{
    m_map = map;
}

void NexusIrHandler::KeyThread::setInitialTimeout(unsigned timeout)
{
    m_initial_timeout = timeout ? MS_TO_NS(timeout) : DEFAULT_TIMEOUT;
}

void NexusIrHandler::KeyThread::setTimeout(unsigned timeout)
{
    m_timeout = timeout ? MS_TO_NS(timeout) : DEFAULT_TIMEOUT;
}

void NexusIrHandler::KeyThread::signal(uint32_t nexus_key, bool repeat,
        unsigned interval)
{
    android::Mutex::Autolock _l(m_mutex);

    ALOGV("%s: nexus_key=0x%08x repeat=%d interval=%u, m_old_key=%d", __PRETTY_FUNCTION__, nexus_key, repeat, interval, m_old_key);

    // If we have received a repeat before we have had a chance to process it in "threadloop",
    // then we need to behave as if a new key press has been made.  This typically occurs when
    // holding down the POWER button on the R/C to exit standby.
    repeat &= (m_old_key != KEY_RESERVED);
    m_key = repeat ? m_old_key : m_map->get(nexus_key);

    if (m_key == KEY_RESERVED) {
        ALOGW("No Linux key mapping for Nexus IR code 0x%08x",
                (unsigned)nexus_key);
    }
    else {
        ALOGV("Linux key mapping for Nexus IR code 0x%08x = %d",
                (unsigned)nexus_key, (int)m_key);
    }

    m_repeat = repeat;
    m_interval = interval;
    m_cond.signal();
}

/*virtual*/ bool NexusIrHandler::KeyThread::threadLoop()
{
    nsecs_t timeout = m_timeout;

    ALOGI("KeyThread start");

    while (isRunning())
    {
        android::Mutex::Autolock _l(m_mutex);

        //enable timeout only if we saw a key
        android::status_t res;
        if (m_key != KEY_RESERVED) {
            ALOGI("waiting for key repeat, timeout: %u ms",
                    (unsigned)NS_TO_MS(timeout));
            res = m_cond.waitRelative(m_mutex, timeout);
        } else {
            ALOGI("waiting for key, no timeout");
            res = m_cond.wait(m_mutex);
        }

        if (res == android::OK) {
            ALOGI("got key: %d, old key: %d, interval %u ms",
                    (int)m_key, (int)m_old_key, m_interval);
            timeout = m_timeout;
        } else if (res == android::TIMED_OUT) {
           ALOGI("repeat time out for key: %d, old key: %d",
                   (int)m_key, (int)m_old_key);

            //simulate key release
            m_key = KEY_RESERVED;
            m_repeat = false;
            m_interval = 0;
        } else {
            ALOGE("wait error %d", (int)res);
        }

        // We've got something to handle either if a new key was pressed
        // or if we stopped seeing repeats. Repeated keys, wait errors
        // or exit requests don't generate key events.
        bool key_event =
            ((res == android::OK) || (res == android::TIMED_OUT))
            && !m_repeat
            && !exitPending();

        if (key_event) {
            if (m_old_key != KEY_RESERVED) {
                ALOGI("emit key release: %d", (int)m_old_key);
                m_uinput.emit_key_state(m_old_key, false) && m_uinput.emit_syn();
            }
            if (m_key != KEY_RESERVED) {
                ALOGI("emit key press: %d", (int)m_key);
                m_uinput.emit_key_state(m_key, true) && m_uinput.emit_syn();
                timeout = m_initial_timeout;
            }

            m_old_key = m_key;
        }
    }

    ALOGI("KeyThread end");
    return false;
}
