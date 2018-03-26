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

#include "linuxuinput.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "NexusIR"
#include <log/log.h>

LinuxUInput::LinuxUInput() :
        m_fd(-1)
{
}

LinuxUInput::~LinuxUInput()
{
    close();
}

bool LinuxUInput::open(const char * path)
{
    m_fd = path ? ::open(path, O_WRONLY | O_NONBLOCK) : -1;
    return m_fd != -1;
}

bool LinuxUInput::start(const char * name, const struct input_id &id)
{
    struct uinput_user_dev uidev;
    size_t bytes;

    ALOGI("LinuxUInput start");

    memset(&uidev, 0, sizeof(uidev));
    if (name)
    {
        snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "%s", name);
    }
    uidev.id = id;
    bytes = write(m_fd, &uidev, sizeof(uidev));
    if (bytes != sizeof(uidev))
        goto err;

    if (ioctl(m_fd, UI_DEV_CREATE) < 0)
        goto err;

    ALOGI("LinuxUInput start success");
    return true;

err:
    ALOGE("LinuxUInput start failed: %d - %s", errno, strerror(errno));
    stop();
    return false;
}

void LinuxUInput::stop()
{
    ALOGI("LinuxUInput stop");
    if (m_fd != -1)
    {
        ioctl(m_fd, UI_DEV_DESTROY);
    }
}

void LinuxUInput::close()
{
    if (m_fd != -1)
    {
        stop();
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LinuxUInput::register_event(int type)
{
    int result = ioctl(m_fd, UI_SET_EVBIT, type);
    if (result == -1)
    {
        ALOGI("LinuxUInput register_event type %d failed: %d - %s",
                type, errno, strerror(errno));
    }
    return result != -1;
}

bool LinuxUInput::register_key(__u16 code)
{
    int result = ioctl(m_fd, UI_SET_KEYBIT, (int)code);
    if (result == -1)
    {
        ALOGE("LinuxUInput register_key %d failed: %d - %s",
                (int)code, errno, strerror(errno));
    }
    else
    {
        ALOGV("LinuxUInput register_key: %d", (int)code);
    }
    return result != -1;
}

bool LinuxUInput::emit(const struct input_event &event)
{
    ALOGV("LinuxUInput emit type: %0x02x, code: %0x02x, value: %d\n",
            (unsigned)event.type, (unsigned)event.code, (int)event.value);
    ssize_t bytes = write(m_fd, &event, sizeof(event));
    return bytes == sizeof(event);
}

bool LinuxUInput::emit_syn()
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    return emit(ev);
}

bool LinuxUInput::emit_key_state(__u16 code, bool pressed)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = pressed ? 1 : 0;
    return emit(ev);
}

bool LinuxUInput::emit_key(__u16 code)
{
    return emit_key_state(code, true) && emit_syn()
            && emit_key_state(code, false) && emit_syn();
}
