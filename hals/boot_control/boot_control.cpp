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
#include <sys/stat.h>

#include <cutils/properties.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/boot_control.h>
#include <inttypes.h>

#include <fs_mgr.h>
#include "eio_boot.h"

// local copy loaded on 'init', gets data from bootloader.
static struct eio_boot bc_eio;
static char suffix[EIO_BOOT_SUFFIX_LEN+1];
static int verbose = 0;

static void wait_for_device(const char* fn) {
   int tries = 0;
   int ret;
   do {
      ++tries;
      struct stat buf;
      ret = stat(fn, &buf);
      if (ret == -1) {
         ALOGE("failed to stat \"%s\" try %d: %s", fn, tries, strerror(errno));
         sleep(1);
      }
   } while (ret && tries < 10);

   if (ret) {
      ALOGE("failed to stat \"%s\"", fn);
   }
}

static int write_device(struct eio_boot *bc_eio) {
   char fstab_name[2*PROPERTY_VALUE_MAX];
   char hardware[PROPERTY_VALUE_MAX];
   property_get("ro.hardware", hardware, "");
   sprintf(fstab_name, "/fstab.%s", hardware);
   struct fstab *fstab = fs_mgr_read_fstab(fstab_name);
   if (fstab == nullptr) {
      ALOGE("failed to read fstab (%s)", fstab_name);
      return -EINVAL;
   }
   struct fstab_rec *v = fs_mgr_get_entry_for_mount_point(fstab, "/eio");
   if (v == nullptr) {
      ALOGE("failed to load partition /eio!");
      return -EINVAL;
   }
   if (strcmp(v->fs_type, "emmc") == 0) {
      wait_for_device(v->blk_device);
      FILE* f = fopen(v->blk_device, "wb");
      if (f == nullptr) {
        ALOGE("failed to open-wb \"%s\": %s", v->blk_device, strerror(errno));
        return -EIO;
      }
      int count = fwrite(bc_eio, sizeof(struct eio_boot), 1, f);
      if (count != 1) {
        ALOGE("failed to write \"%s\": %s", v->blk_device, strerror(errno));
        return -EIO;
      }
      if (fclose(f) != 0) {
        ALOGE("failed to close \"%s\": %s", v->blk_device, strerror(errno));
        return -EIO;
      }
      return 0;
   }
   ALOGE("unknown eio partition fs_type \"%s\"", v->fs_type);
   return -EINVAL;
}

static void init(struct boot_control_module *module __unused) {
   memset(&bc_eio, 0, sizeof(bc_eio));
   char fstab_name[2*PROPERTY_VALUE_MAX];
   char hardware[PROPERTY_VALUE_MAX];
   property_get("ro.hardware", hardware, "");
   sprintf(fstab_name, "/fstab.%s", hardware);
   struct fstab *fstab = fs_mgr_read_fstab(fstab_name);
   if (fstab == nullptr) {
      ALOGE("failed to read fstab (%s)", fstab_name);
      return;
   }
   struct fstab_rec *v = fs_mgr_get_entry_for_mount_point(fstab, "/eio");
   if (v == nullptr) {
      ALOGE("failed to load partition /eio!");
      return;
   }
   if (strcmp(v->fs_type, "emmc") == 0) {
      wait_for_device(v->blk_device);
      FILE* f = fopen(v->blk_device, "rb");
      if (f == nullptr) {
        ALOGE("failed to open-rb \"%s\": %s", v->blk_device, strerror(errno));
        return;
      }
      struct eio_boot temp;
      int count = fread(&temp, sizeof(temp), 1, f);
      if (count != 1) {
        ALOGE("failed to read \"%s\": %s", v->blk_device, strerror(errno));
        return;
      }
      if (fclose(f) != 0) {
        ALOGE("failed to close \"%s\": %s", v->blk_device, strerror(errno));
        return;
      }
      memcpy(&bc_eio, &temp, sizeof(temp));
      ALOGI("init: read magic (%x)", bc_eio.magic);
      return;
   }
   ALOGE("unknown eio partition fs_type \"%s\"", v->fs_type);
   return;
}

static unsigned getNumberSlots(struct boot_control_module *module __unused) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("getNumberSlots: bad magic (%x), not setup?", bc_eio.magic);
      return 0;
   }
   // if setup; we always guarantee the proper number as bootloader would have
   // asserted the correct partitions set are available for each slot based
   // partitions.
   ALOGI_IF(verbose, "getNumberSlots(%d)", EIO_BOOT_NUM_ALT_PART);
   return EIO_BOOT_NUM_ALT_PART;
}

static unsigned getCurrentSlot(struct boot_control_module *module __unused) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("getCurrentSlot: bad magic (%x), not setup?", bc_eio.magic);
      return EIO_BOOT_NUM_ALT_PART;
   }
   ALOGI_IF(verbose, "getCurrentSlot(%d)", bc_eio.current);
   return bc_eio.current;
}

static int markBootSuccessful(struct boot_control_module *module __unused) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("markBootSuccessful: bad magic (%x), not setup?", bc_eio.magic);
      return 0;
   }
   bc_eio.slot[bc_eio.current].boot_ok   = 1;
   bc_eio.slot[bc_eio.current].boot_fail = 0;
   bc_eio.slot[bc_eio.current].boot_try  = 0;
   ALOGI_IF(verbose, "markBootSuccessful(%d)", bc_eio.current);
   return write_device(&bc_eio);
}

static int setActiveBootSlot(struct boot_control_module *module __unused, unsigned slot) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("setActiveBootSlot: bad magic (%x), not setup?", bc_eio.magic);
      return 0;
   }
   if (slot >= EIO_BOOT_NUM_ALT_PART) {
      ALOGE("setActiveBootSlot: bad slot index (%u)", slot);
      return -EINVAL;
   }
   /* application wants us to boot into this new slot.
    */
   bc_eio.current = slot;
   /* reset prior settings for the slot; it gets a clean start.
    */
   bc_eio.slot[bc_eio.current].boot_ok   = 0;
   bc_eio.slot[bc_eio.current].boot_fail = 0;
   bc_eio.slot[bc_eio.current].boot_try  = 0;
   ALOGI_IF(verbose, "setActiveBootSlot(%d): reset stats", bc_eio.current);
   return write_device(&bc_eio);
}

static int setSlotAsUnbootable(struct boot_control_module *module __unused, unsigned slot) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("setSlotAsUnbootable: bad magic (%x), not setup?", bc_eio.magic);
      return 0;
   }
   if (slot >= EIO_BOOT_NUM_ALT_PART) {
      ALOGE("setSlotAsUnbootable: bad slot index (%u)", slot);
      return -EINVAL;
   }
   /* typically happens at onset of update to prevent using the slot until
    * update process completes.
    */
   bc_eio.slot[slot].boot_fail = 1;
   ALOGI_IF(verbose, "setSlotAsUnbootable(%d,current:%d)", slot, bc_eio.current);
   return write_device(&bc_eio);
}

static int isSlotBootable(struct boot_control_module *module __unused, unsigned slot __unused) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("isSlotBootable: bad magic (%x), not setup?", bc_eio.magic);
      return 0;
   }
   if (slot >= EIO_BOOT_NUM_ALT_PART) {
      ALOGE("setSlotAsUnbootable: bad slot index (%u)", slot);
      return -EINVAL;
   }
   ALOGI_IF(verbose, "isSlotBootable(%d,current:%d)=%d", slot, bc_eio.current, bc_eio.slot[slot].boot_try);
   return bc_eio.slot[slot].boot_try;
}

static const char* getSuffix(struct boot_control_module *module __unused, unsigned slot) {
   int i = 0;
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("getSuffix: bad magic (%x), not setup?", bc_eio.magic);
      return NULL;
   }
   if (slot >= EIO_BOOT_NUM_ALT_PART) {
      ALOGE("getSuffix: bad slot: %u", slot);
      return NULL;
   }
   memset(suffix, 0, EIO_BOOT_SUFFIX_LEN+1);
   if (bc_eio.slot[slot].suffix[0] != '_') {
      suffix[0] = '_';
      i = 1;
   }
   snprintf(&suffix[i], EIO_BOOT_SUFFIX_LEN, "%s", bc_eio.slot[slot].suffix);
   return suffix;
}

static int isSlotMarkedSuccessful(struct boot_control_module *module __unused, unsigned slot) {
   if (bc_eio.magic != EIO_BOOT_MAGIC) {
      ALOGE("isSlotMarkedSuccessful: bad magic (%x), not setup?", bc_eio.magic);
      return -ENOMEM;
   }
   if (slot >= EIO_BOOT_NUM_ALT_PART) {
      ALOGE("isSlotMarkedSuccessful: bad slot index (%u)", slot);
      return -EINVAL;
   }
   ALOGI_IF(verbose, "isSlotMarkedSuccessful(%d,current:%d)=%d", slot, bc_eio.current, bc_eio.slot[slot].boot_ok);
   return bc_eio.slot[slot].boot_ok;
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
