/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/boot_control.h>
#include <inttypes.h>

static void init(struct boot_control_module *module __unused) {
   return;
}

static unsigned getNumberSlots(struct boot_control_module *module __unused) {
   return 0;
}

static unsigned getCurrentSlot(struct boot_control_module *module __unused) {
   return 0;
}

static int markBootSuccessful(struct boot_control_module *module __unused) {
   return 0;
}

static int setActiveBootSlot(struct boot_control_module *module __unused, unsigned slot __unused) {
   return 0;
}

static int setSlotAsUnbootable(struct boot_control_module *module __unused, unsigned slot __unused) {
   return 0;
}

static int isSlotBootable(struct boot_control_module *module __unused, unsigned slot __unused) {
   return 0;
}

static const char* getSuffix(struct boot_control_module *module __unused, unsigned slot __unused) {
   return NULL;
}

static int isSlotMarkedSuccessful(struct boot_control_module *module __unused, unsigned slot __unused) {
   return 0;
}

static struct hw_module_methods_t boot_control_module_methods = {
    .open = NULL
};

struct boot_control_module HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = BOOT_CONTROL_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = BOOT_CONTROL_HARDWARE_MODULE_ID,
      .name               = "boot control hal for bcm platform",
      .author             = "Broadcom",
      .methods            = &boot_control_module_methods,
      .dso                = 0,
      .reserved           = {0}
    },
    .init                    = init,
    .getNumberSlots          = getNumberSlots,
    .getCurrentSlot          = getCurrentSlot,
    .markBootSuccessful      = markBootSuccessful,
    .setActiveBootSlot       = setActiveBootSlot,
    .setSlotAsUnbootable     = setSlotAsUnbootable,
    .isSlotBootable          = isSlotBootable,
    .getSuffix               = getSuffix,
    .isSlotMarkedSuccessful  = isSlotMarkedSuccessful,
};
