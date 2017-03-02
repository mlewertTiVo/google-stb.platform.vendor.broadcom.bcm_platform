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

#include <stdint.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc1.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/native_handle.h>
#include <cutils/atomic.h>
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

/* TODO: backward compatibility with 0.x, remove later.
 */
#define GR_NONE                 (1<<0)
#define GR_STANDARD             (1<<1)
#define GR_YV12                 (1<<2)
#define GR_HWTEX                (1<<3)

struct gr1_priv_t;

typedef struct __SHARED_DATA_ {
   struct {
      volatile int32_t windowIdPlusOne;
      uint64_t         nexusClientContext;
   } videoWindow;

   struct {
      int32_t locked;
      bool  destripeComplete;
      NEXUS_VideoDecoderFrameStatus status;
      NEXUS_StripedSurfaceHandle hStripedSurface;
   } videoFrame;

   struct {
      int      sfhdl;
      NEXUS_MemoryBlockHandle block; /* TODO: backward compatibility with 0.x, remove later. */
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
   // TODO: backward compatibility with v-0.x, remove later.
   int         fmt_set;

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
        nxSurfacePhysicalAddress(0),
        fmt_set (0) {
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

    static int get_block_handles(gr1_priv_t *h,
       NEXUS_MemoryBlockHandle *shdl, NEXUS_MemoryBlockHandle *phdl) {
       if (shdl != NULL) {
          *shdl = (NEXUS_MemoryBlockHandle)(intptr_t)h->descriptor;
       } else {
          return -EINVAL;
       }
       /* never used currently, so ignore. */
       if (phdl != NULL) {
          *phdl = 0;
       }
       return 0;
    }

    static int lock_video_frame(gr1_priv_t *hdl, int timeoutMs) {
        PSHARED_DATA pSharedData = NULL;
        NEXUS_Error lrc = NEXUS_SUCCESS;
        NEXUS_MemoryBlockHandle block_handle = NULL;
        int rc = 0;
        void *pMemory;

        get_block_handles(hdl, &block_handle, NULL);
        lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
        if (lrc == BERR_NOT_SUPPORTED) {
           NEXUS_MemoryBlock_Unlock(block_handle);
        }
        pSharedData = (PSHARED_DATA) pMemory;

        struct timespec ts_end, ts_now;
        if (NULL == pSharedData) {
          rc = -EINVAL;
          goto out;
        }
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        ts_end.tv_sec += timeoutMs/1000;
        ts_end.tv_nsec += (timeoutMs%1000)*1000000;
        if (ts_end.tv_nsec >= 1000000000) {
            ts_end.tv_sec++;
            ts_end.tv_nsec -= 1000000000;
        }
        while (android_atomic_acquire_cas(0, 1, &pSharedData->videoFrame.locked)) {
            BKNI_Sleep(1);
            clock_gettime(CLOCK_MONOTONIC, &ts_now);
            if (ts_now.tv_sec < ts_end.tv_sec) {
                continue;
            }
            if (ts_now.tv_sec > ts_end.tv_sec || ts_now.tv_nsec > ts_end.tv_nsec) {
                rc = -EBUSY;
                goto out;
            }
        }

out:
        if (block_handle) {
           if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
        }
        return rc;
    }

    static void unlock_video_frame(gr1_priv_t *hdl) {
        PSHARED_DATA pSharedData = NULL;
        NEXUS_MemoryBlockHandle block_handle = NULL;
        NEXUS_Error lrc = NEXUS_SUCCESS;
        int rc = 0;
        void *pMemory;

        get_block_handles(hdl, &block_handle, NULL);
        lrc = NEXUS_MemoryBlock_Lock(block_handle, &pMemory);
        if (lrc == BERR_NOT_SUPPORTED) {
           NEXUS_MemoryBlock_Unlock(block_handle);
        }
        pSharedData = (PSHARED_DATA) pMemory;

        if (NULL == pSharedData) {
          goto out;
        }
        rc = android_atomic_release_cas(1, 0, &pSharedData->videoFrame.locked);
        assert(rc);

out:
        if (block_handle) {
           if (!lrc) NEXUS_MemoryBlock_Unlock(block_handle);
        }
    }

#endif
};

/* TODO: backward compatibility with 0.x for external to gralloc code.
 */
typedef struct gr1_priv_t private_handle_t;

#endif /* GRALLOC1_PRIV_H_ */
