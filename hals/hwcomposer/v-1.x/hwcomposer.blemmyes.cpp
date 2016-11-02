/*
 * Copyright (C) 2016 Broadcom
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/hardware.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hwcomposer.h>

struct hwc_context_t {
    hwc_composer_device_1_t device;
    hwc_procs_t const* procs;
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "hwc for blemmyes",
        author: "broadcom canada ltd.",
        methods: &hwc_module_methods,
    }
};

static int hwc_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays) {
    if (displays && (displays[0]->flags & HWC_GEOMETRY_CHANGED)) {
        for (size_t i=0 ; i<displays[0]->numHwLayers ; i++) {
            displays[0]->hwLayers[i].compositionType = HWC_OVERLAY;
        }
    }
    return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    return 0;
}

static int hwc_device_setPowerMode(struct hwc_composer_device_1* dev,
        int disp, int mode)
{
    return 0;
}

static int hwc_device_eventControl(struct hwc_composer_device_1* dev,
        int disp, int event, int enabled)
{
    return 0;
}

static int hwc_device_query(struct hwc_composer_device_1* dev,
        int what, int* value)
{
    switch (what) {
       case HWC_BACKGROUND_LAYER_SUPPORTED:
           *value = 0;
       break;
       case HWC_VSYNC_PERIOD:
           *value = (int)16666667;
       break;
       case HWC_DISPLAY_TYPES_SUPPORTED:
           *value = HWC_DISPLAY_PRIMARY_BIT;
       break;
    }
    return 0;
}

static void hwc_device_registerProcs(struct hwc_composer_device_1* dev,
         hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ctx->procs = (hwc_procs_t *)procs;
}

static void hwc_device_dump(struct hwc_composer_device_1* dev,
         char *buff, int buff_len)
{
    return;
}

static int hwc_device_getDisplayConfigs(struct hwc_composer_device_1* dev,
    int disp, uint32_t* configs, size_t* numConfigs)
{
    int ret = 0;

    if (*numConfigs == 0 || configs == NULL)
       return ret;

    if (disp == HWC_DISPLAY_PRIMARY) {
        *numConfigs = 1;
        configs[0] = 0;
    } else {
        ret = -1;
    }
    return ret;
}

static int hwc_device_getDisplayAttributes(struct hwc_composer_device_1* dev,
        int disp, uint32_t config, const uint32_t* attributes, int32_t* values)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int i;

    if (disp == HWC_DISPLAY_PRIMARY) {
       while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE) {
          switch (attributes[i]) {
          case HWC_DISPLAY_VSYNC_PERIOD:
             values[i] = (int32_t)16666667;
          break;
          case HWC_DISPLAY_WIDTH:
             values[i] = 1280;
          break;
          case HWC_DISPLAY_HEIGHT:
             values[i] = 720;
          break;
          case HWC_DISPLAY_DPI_X:
             values[i] = 160;
          break;
          case HWC_DISPLAY_DPI_Y:
             values[i] = 160;
          break;
          default:
          break;
          }
          i++;
       }
       return 0;
    }
    return -EINVAL;
}

static int hwc_device_getActiveConfig(struct hwc_composer_device_1* dev,
        int disp)
{
    if (disp == HWC_DISPLAY_PRIMARY)
        return 0;
    return -1;
}

static int hwc_device_setActiveConfig(struct hwc_composer_device_1* dev,
        int disp, int index)
{
    if (disp == HWC_DISPLAY_PRIMARY && index == 0)
       return 0;
    return -EINVAL;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx) {
        free(ctx);
    }
    return 0;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_DEVICE_API_VERSION_1_1;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare                        = hwc_prepare;
        dev->device.set                            = hwc_set;
        dev->device.setPowerMode                   = hwc_device_setPowerMode;
        dev->device.eventControl                   = hwc_device_eventControl;
        dev->device.query                          = hwc_device_query;
        dev->device.registerProcs                  = hwc_device_registerProcs;
        dev->device.dump                           = hwc_device_dump;
        dev->device.getDisplayConfigs              = hwc_device_getDisplayConfigs;
        dev->device.getDisplayAttributes           = hwc_device_getDisplayAttributes;
        dev->device.getActiveConfig                = hwc_device_getActiveConfig;
        dev->device.setActiveConfig                = hwc_device_setActiveConfig;
        dev->device.setCursorPositionAsync         = NULL;

        *device = &dev->device.common;
        status = 0;
    }
    return status;
}
