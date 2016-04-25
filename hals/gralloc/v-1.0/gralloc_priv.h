/*
 * Copyright (C) 2016-2017 The Android Open Source Project
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

#ifndef GRALLOC1_PRIV_H_
#define GRALLOC1_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc1.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/native_handle.h>
#include "nexus_base_mmap.h"
#include "nexus_video_decoder_types.h"
#include "nexus_striped_surface.h"
#include "nexus_surface.h"
#include "nexus_graphics2d.h"
#include "bstd_defs.h"
#include "berr.h"
#include "bkni.h"
#include <time.h>
#include <linux/time.h>
#include <assert.h>
#include "nx_ashmem.h"

#define CHECKPOINT_TIMEOUT      (5000)

struct gr1_priv_t;

typedef struct __SHARED_DATA_ {
   struct {
      volatile int32_t windowIdPlusOne;
      void *           nexusClientContext;
   } videoWindow;

   struct {
      int32_t locked;
      bool  destripeComplete;
      NEXUS_VideoDecoderFrameStatus status;
      NEXUS_StripedSurfaceHandle hStripedSurface;
   } videoFrame;

   struct {
      int      sfhdl;
      int32_t  format;
      uint32_t width;
      uint32_t height;
      uint32_t stride;
      uint64_t cUsage;
      uint64_t pUsage;
      int      bpp;
      uint32_t size;
      uint32_t alignment;
      int      wlock;
  } container;

} SHARED_DATA, *PSHARED_DATA;

#ifdef __cplusplus
struct gr1_priv_t : public native_handle {
#else
struct gr1_priv_t {
   struct native_handle nativeHandle;
#endif
   // fd's
   int         pfhdl;
   // int's
   int         magic;
   int         pid;
   uint64_t    descriptor __attribute__((aligned(8)));
   // vc5 cached interface
   int         oglStride;
   int         oglSize;
   uint64_t    nxSurfaceAddress __attribute__((aligned(8)));
   uint64_t    nxSurfacePhysicalAddress __attribute__((aligned(8)));

#ifdef __cplusplus
    static const int sNumFds = 1;
    static inline int sNumInts() {
       return ((sizeof(gr1_priv_t) - sizeof(native_handle_t)) / sizeof(int)) - sNumFds;
    }
    static const int sMagic = 0x4F77656E;

    gr1_priv_t(uint64_t descriptor) :
        pfhdl(-1),
        magic(sMagic),
        pid(getpid()),
        descriptor(descriptor),
        oglStride(0),
        oglSize(0),
        nxSurfaceAddress(0),
        nxSurfacePhysicalAddress(0) {
        version = sizeof(native_handle);
        numInts = sNumInts();
        numFds = sNumFds;
    }

    ~gr1_priv_t() {
        magic = 0;
    }

    static int validate(const native_handle* h) {
        const gr1_priv_t* hnd = (const gr1_priv_t*)h;
        if (!hnd || hnd->version != sizeof(native_handle) ||
            hnd->numInts != sNumInts() || hnd->numFds != sNumFds ||
            hnd->magic != sMagic) {
            return -EINVAL;
        }
        return 0;
    }

#endif
};

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC1_PRIV_H_ */
