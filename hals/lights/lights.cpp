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

//#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "bcm_light"

#include <hardware/lights.h>

#include "nexus_types.h"
#include "nexus_platform.h"
#include "nxclient.h"
#include "nxclient_config.h"
#include <nxwrap.h>

#include <stdlib.h>
#include <string.h>

#include <cutils/log.h>

#define NEXUS_DISPLAY_OBJECTS 4


struct private_device_t {
    struct light_device_t light;
    NxWrap *pNxWrap;
    NEXUS_DisplayHandle dispHandle;
};

static int lights_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device);

static struct hw_module_methods_t lights_module_methods = {
   .open = lights_device_open
};

// The lights module
struct hw_module_t HAL_MODULE_INFO_SYM = {
   .tag                = HARDWARE_MODULE_TAG,
   .module_api_version = HARDWARE_MODULE_API_VERSION(0, 1),
   .hal_api_version    = HARDWARE_HAL_API_VERSION,
   .id                 = LIGHTS_HARDWARE_MODULE_ID,
   .name               = "lights for set-top-box platforms",
   .author             = "Broadcom",
   .methods            = &lights_module_methods,
   .dso                = 0,
   .reserved           = {0}
};

static int lights_set_light_backlight(struct light_device_t *dev,
        struct light_state_t const *state)
{
    struct private_device_t *priv_dev = reinterpret_cast<struct private_device_t *>(dev);

    if (!state || priv_dev == NULL)
    {
        return -EINVAL;
    }

    ALOGV("requested color: 0x%08x", state->color);

    // calculate luminance
    double r = ((state->color >> 16) & 0xff) / 255.0;
    double g = ((state->color >> 8) & 0xff) / 255.0;
    double b = ((state->color) & 0xff) / 255.0;
    double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    ALOGV("relative luminance: %f", y);

    NEXUS_GraphicsColorSettings settings;
    NEXUS_Display_GetGraphicsColorSettings(priv_dev->dispHandle, &settings);
    ALOGV("current brightness: %d", (int)settings.brightness);

    //scale to *half* of int16_t range for Nexus (full range is not usable)
    static const int full_range = 65535;
    settings.brightness = 0.5 * full_range * (y - 0.5);
    ALOGV("new brightness: %d", (int)settings.brightness);
    NEXUS_Display_SetGraphicsColorSettings(priv_dev->dispHandle, &settings);

    return 0;
}

static int lights_device_close(struct hw_device_t *dev)
{
    struct private_device_t *priv_dev = reinterpret_cast<struct private_device_t *>(dev);

    if (priv_dev) {
        if (priv_dev->pNxWrap) {
            priv_dev->pNxWrap->leave();
            delete priv_dev->pNxWrap;
        }
        free(priv_dev);
    }
    return 0;
}

static int lights_device_open(const struct hw_module_t *module,
        const char *name, struct hw_device_t **device)
{
    int status = -EINVAL;

    *device = NULL;
    if (strcmp(name, LIGHT_ID_BACKLIGHT) == 0) {
        NEXUS_InterfaceName interfaceName;
        NEXUS_PlatformObjectInstance objects[NEXUS_DISPLAY_OBJECTS]; /* won't overflow. */
        size_t num = NEXUS_DISPLAY_OBJECTS;
        NEXUS_Error nrc;

        struct private_device_t *dev = (struct private_device_t *)malloc(sizeof(*dev));

        *device = reinterpret_cast<hw_device_t *>(dev);

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        dev->pNxWrap = new NxWrap("lights");
        if (!dev->pNxWrap) {
            ALOGE("%s: failed NxWrap", __FUNCTION__);
            goto out;
        }
        dev->pNxWrap->join();

        /* retrieve Nexus display handle */
        strcpy(interfaceName.name, "NEXUS_Display");
        nrc = NEXUS_Platform_GetObjects(&interfaceName, &objects[0], num, &num);
        if (nrc != NEXUS_SUCCESS)
        {
            ALOGE("%s: failed to get display handle (%u)", __FUNCTION__, nrc);
            goto out;
        }

        /* initialize the procs */
        dev->light.common.tag = HARDWARE_DEVICE_TAG;
        dev->light.common.version = 0;
        dev->light.common.module = const_cast<hw_module_t*>(module);
        dev->light.common.close = lights_device_close;

        dev->light.set_light = lights_set_light_backlight;

        dev->dispHandle = reinterpret_cast<NEXUS_DisplayHandle>(objects[0].object);

        status = 0;
    }
out:
    if (status) {
        lights_device_close(*device);
        *device = NULL;
    }

    return status;
}
