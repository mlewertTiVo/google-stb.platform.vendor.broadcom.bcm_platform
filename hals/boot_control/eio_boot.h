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

#ifndef EIO_BOOT__H
#define EIO_BOOT__H

#define EIO_BOOT_MAGIC        0x331100BB
#define EIO_BOOT_NUM_ALT_PART 2

#define EIO_BOOT_SUFFIX_LEN   8

struct eio_boot_slot {
   char     suffix[EIO_BOOT_SUFFIX_LEN];
   int32_t  valid;
   int32_t  boot_try;
   int32_t  boot_ok;
   int32_t  boot_fail;
   int32_t  dmv_corrupt;
   int32_t  spare[128];
};

struct eio_boot {
   int32_t  magic;
   int32_t  version;
   int32_t  current;
   struct   eio_boot_slot slot[EIO_BOOT_NUM_ALT_PART];
   int32_t  onboot;
   int32_t  spare[255];
};

#endif /* EIO_BOOT__H */
