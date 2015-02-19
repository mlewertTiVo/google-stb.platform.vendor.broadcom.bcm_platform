/******************************************************************************
 *    (c)2014 Broadcom Corporation
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

#include <hardware/lights.h>

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_ipc_client_factory.h"

#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>

static int lights_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device);

static struct hw_module_methods_t lights_module_methods = {
    open: lights_device_open
};

// The lights module
struct hw_module_t HAL_MODULE_INFO_SYM = {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: LIGHTS_HARDWARE_MODULE_ID,
        name: "BRCM lights module",
        author: "Broadcom Corp.",
        methods: &lights_module_methods,
        dso: NULL,
        reserved: {}
};

static int lights_set_light_backlight(struct light_device_t *dev,
        struct light_state_t const *state)
{
    (void)dev;

    NexusIPCClientBase *ipcclient =
        NexusIPCClientFactory::getClient("generalSTBFunctions");
    if (!ipcclient || !state)
        return -EINVAL;

    LOGI("requested color: 0x%08x", state->color);

    // calculate luminance
    double r = ((state->color >> 16) & 0xff) / 255.0;
    double g = ((state->color >> 8) & 0xff) / 255.0;
    double b = ((state->color) & 0xff) / 255.0;
    double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    LOGV("relative luminance: %f", y);

    NEXUS_GraphicsColorSettings settings;
    ipcclient->getGraphicsColorSettings(0, &settings);
    LOGV("current brightness: %d", (int)settings.brightness);

    //scale to *half* of int16_t range for Nexus (full range is not usable)
    static const int full_range = 65535;
    settings.brightness = 0.5 * full_range * (y - 0.5);
    LOGV("new brightness: %d", (int)settings.brightness);
    ipcclient->setGraphicsColorSettings(0, &settings);

    return 0;
}

static int lights_device_close(struct hw_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}

static int lights_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device)
{
    int status = -EINVAL;
    if (strcmp(name, LIGHT_ID_BACKLIGHT) == 0) {
        light_device_t *dev = (light_device_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module = const_cast<hw_module_t*>(module);
        dev->common.close = lights_device_close;

        dev->set_light = lights_set_light_backlight;

        *device = reinterpret_cast<hw_device_t*>(dev);
        status = 0;
    }
    return status;
}
