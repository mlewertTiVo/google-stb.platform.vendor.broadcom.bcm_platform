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

#ifndef _NEXUSIRHANDLER_H_
#define _NEXUSIRHANDLER_H_

#include "nexusirinput.h"
#include "linuxuinput.h"
#include "nexusirmap.h"

#include <utils/threads.h>
#include <utils/Condition.h>
#include <stdint.h>

class NexusIrMap;

/**
 * @brief Map Nexus IR key code to Linux kernel key event.
 *
 * The NexusIrHandler uses NexusIrInput as source and LinuxUInput as sink
 * for IR key events. The passed-in map is used to map Nexus IR events
 * to Linux key presses.
 */
class NexusIrHandler: private NexusIrInput::Observer
{
public:
    NexusIrHandler();
    virtual ~NexusIrHandler();

    /**
     * @brief Start IR handler.
     *
     * Start maping Nexus IR events to Linux input key presses.
     *
     * @param mode Nexus IR mode.
     * @param map Key map.
     * @return true on success, flase othewise.
     */
    bool start(NEXUS_IrInputMode mode, android::sp<NexusIrMap> map,
            uint64_t mask = 0);

    /**
     * @brief Stop IR handler.
     */
    void stop();

private:
    /** As per NexusIrInput::Observer */
    virtual void onIrInput(uint32_t key, bool repeat, unsigned interval);

private:
    /* Disallow copy constructor and assignment operator... */
    NexusIrHandler(const NexusIrHandler &other); //not implemented
    NexusIrHandler & operator=(const NexusIrHandler &other); //not implemented

    NEXUS_IrInputMode m_mode;
    NexusIrInput m_ir;
    LinuxUInput m_uinput;

    class KeyThread: public android::Thread
    {
    public:
        KeyThread(LinuxUInput &uinput);
        virtual ~KeyThread();

        void setMap(android::sp<NexusIrMap> map);
        void setTimeout(unsigned timeout); //in milliseconds
        void signal(uint32_t nexus_key, bool repeat, unsigned interval);

        /** As per android::Thread */
        virtual bool threadLoop();

    private:
        android::sp<NexusIrMap> m_map;
        LinuxUInput &m_uinput;

        android::Mutex m_mutex;
        android::Condition m_cond;
        __u16 m_old_key; //previous key - needed to emit key release event
        __u16 m_key; //current key - needed to emit key press event
        bool m_repeat;
        unsigned m_interval;
        nsecs_t m_timeout;
    };

    KeyThread m_key_thread;
};

#endif /* _NEXUSIRHANDLER_H_ */
