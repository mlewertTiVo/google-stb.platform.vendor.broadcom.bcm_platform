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

#ifndef _NEXUSIRINPUT_H_
#define _NEXUSIRINPUT_H_

#include "nexus_types.h"
#include "nexus_ir_input.h"

#include <stdint.h>

#define NEXUSIRINPUT_NO_KEY 0xDEADC0DE

/**
 * @brief Nexus IR input wrapper.
 */
class NexusIrInput
{
public:
    /**
     * @brief An observer interface to receive IR key codes.
     */
    class Observer
    {
    public:
        Observer() {}
        virtual ~Observer() {}
        virtual void onIrInput(uint32_t key, bool repeat,
                unsigned interval) = 0;
    };

    NexusIrInput();
    ~NexusIrInput();

    /**
     * @brief Open Nexus IR input and start receiving IR events.
     *
     * @param mode Remote mode.
     * @param Observer An instance of the Observer to receive key events.
     * @param power_key A key input controls suspending or resuming the system
     * @param mask A mask to be applied to all received remote codes.
     * Each bit set in the mask ignores a corresponding bit in the IR code
     * (i.e. result = ir_code & ~mask). This is useful for remote protocols
     * that send variable codes, for example RC6.
     * @param power_key_two A key input that will wake up system from suspension
     * @param mask_two A mask to be applied to received power_key_two
     *
     * @return true on success, otherwise false.
     */
    bool start(NEXUS_IrInputMode mode, Observer &Observer,
            uint32_t power_key = NEXUSIRINPUT_NO_KEY, uint64_t mask = 0,
            uint32_t power_key_two = NEXUSIRINPUT_NO_KEY, uint64_t mask_two = 0);

    /**
     * @brief Stop receiving IR events and close Nexus IR input.
     *
     * Stopping an instance that's not started is a no-op.
     */
    void stop();

    /**
     * @brief Get repeat timeout (for the first repeat event)
     * @return timeout in milliseconds
     */
    unsigned initialRepeatTimeout();

    /**
     * @brief Get repeat timeout (for subsequent repeat events)
     * @return timeout in milliseconds
     */
    unsigned repeatTimeout();

private:
    /* Disallow copy constructor and assignment operator... */
    NexusIrInput(const NexusIrInput &other); //not implemented
    NexusIrInput & operator=(const NexusIrInput &other); //not implemented

    static void dataReady(void *context, int param);
    void dataReady();

    NEXUS_IrInputHandle m_handle;
    Observer *m_observer;
    uint32_t m_power_key;
    uint32_t m_power_key_two;
    uint64_t m_mask;
    uint64_t m_mask_two;

    bool m_clear_on_boot;
};

#endif /* _NEXUS_IR_INPUT_H_ */
