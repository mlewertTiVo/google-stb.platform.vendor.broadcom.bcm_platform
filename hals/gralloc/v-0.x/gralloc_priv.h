/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef GRALLOC_PRIV_H_
#define GRALLOC_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/native_handle.h>
#include <linux/fb.h>
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

#define GR_MGMT_MODE_LOCKED     1
#define GR_MGMT_MODE_UNLOCKED   2

#define HAL_PIXEL_FORMAT_YUV420P 19 /* OMX_COLOR_FormatYUV420Planar */

#define CHECKPOINT_TIMEOUT      (5000)

#define GR_NONE                 (1<<0)
#define GR_STANDARD             (1<<1)
#define GR_YV12                 (1<<2)
#define GR_HWTEX                (1<<3)

/*****************************************************************************/

struct private_module_t;

struct private_module_t {
   gralloc_module_t base;
};

/*****************************************************************************/
/*
 * This shared data is for deferred allocation scheme
 * */

typedef struct __SHARED_DATA_ {
   struct {
      volatile int32_t windowIdPlusOne;
      uint64_t         nexusClientContext;
   } videoWindow;

   //Metadata For Video Buffers
   struct {
      int32_t locked;
      bool  destripeComplete;
      NEXUS_VideoDecoderFrameStatus status;
      NEXUS_StripedSurfaceHandle hStripedSurface;
   } videoFrame;

   struct {
      unsigned format;
      unsigned bpp;
      unsigned width;
      unsigned height;
      NEXUS_MemoryBlockHandle block;
      unsigned size;
      unsigned allocSize;
      unsigned stride;
  } container;

} SHARED_DATA, *PSHARED_DATA;

#ifdef __cplusplus
struct private_handle_int_t : public native_handle {
#else
struct private_handle_int_t {
   struct native_handle nativeHandle;
#endif
   // file descriptors
   int         pdata;
   int         sdata;
   // int's
   int         magic;
   int         flags;
   int         pid;
   int         oglStride;
   int         oglFormat;
   int         oglSize;
   int         usage;
   int         alignment;
   int         mgmt_mode;
   int         fmt_set;
   // v3d/vc5 cached interface
   uint64_t    nxSurfaceAddress __attribute__((aligned(8)));
   uint64_t    nxSurfacePhysicalAddress __attribute__((aligned(8)));

#ifdef __cplusplus
    static const int sNumFds = 2;
    static inline int sNumInts() {
       return ((sizeof(private_handle_int_t) - sizeof(native_handle_t)) / sizeof(int)) - sNumFds;
    }
    static const int sMagic = 0x4F77656E;

    private_handle_int_t(int pdata, int sdata, int flags) :
        pdata(pdata), sdata(sdata), magic(sMagic), flags(flags),
        pid(getpid()), oglStride(0), oglFormat(0), oglSize(0),
        usage(0), alignment(0),
        mgmt_mode(GR_MGMT_MODE_LOCKED), fmt_set(GR_NONE),
        nxSurfaceAddress(0), nxSurfacePhysicalAddress(0)
    {
        version = sizeof(native_handle);
        numInts = sNumInts();
        numFds = sNumFds;
    }

    ~private_handle_int_t()
    {
        magic = 0;
    }

    static int validate(const native_handle* h)
    {
        const private_handle_int_t* hnd = (const private_handle_int_t*)h;
        if (!h || h->version != sizeof(native_handle) ||
            h->numInts != sNumInts() || h->numFds != sNumFds ||
            hnd->magic != sMagic)
        {
            return -EINVAL;
        }
        return 0;
    }

    static int get_block_handles(private_handle_int_t *pHandle, NEXUS_MemoryBlockHandle *sdata, NEXUS_MemoryBlockHandle *pdata)
    {
       int rc;
       struct nx_ashmem_getmem getmem;

       if ((sdata != NULL) && (pHandle->sdata >= 0)) {
          rc = ioctl(pHandle->sdata, NX_ASHMEM_GET_BLK, &getmem);
          if (rc >= 0) {
             *sdata = (NEXUS_MemoryBlockHandle)getmem.hdl;
          } else {
             return rc;
          }
       }

       if ((pdata != NULL) && (pHandle->pdata >= 0)) {
          rc = ioctl(pHandle->pdata, NX_ASHMEM_GET_BLK, &getmem);
          if (rc >= 0) {
             *pdata = (NEXUS_MemoryBlockHandle)getmem.hdl;
          } else {
             return rc;
          }
       }

       return 0;
    }

    static int lock_video_frame(private_handle_int_t *pHandle, int timeoutMs)
    {
        PSHARED_DATA pSharedData = NULL;
        NEXUS_Error lrc = NEXUS_SUCCESS;
        NEXUS_MemoryBlockHandle block_handle = NULL;
        int rc = 0;
        void *pMemory;

        get_block_handles(pHandle, &block_handle, NULL);
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
        // Compute timeout
        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        ts_end.tv_sec += timeoutMs/1000;
        ts_end.tv_nsec += (timeoutMs%1000)*1000000;
        if (ts_end.tv_nsec >= 1000000000) {
            ts_end.tv_sec++;
            ts_end.tv_nsec -= 1000000000;
        }
        // Lock "semaphore" with timeout
        while (android_atomic_acquire_cas(0, 1, &pSharedData->videoFrame.locked)) {
            BKNI_Sleep(1);
            clock_gettime(CLOCK_MONOTONIC, &ts_now);
            /* equivalent to if ( timespec_compare(&ts_now, &ts_end) >= 0 ) return -EBUSY; */
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

    static void unlock_video_frame(private_handle_int_t *pHandle)
    {
        PSHARED_DATA pSharedData = NULL;
        NEXUS_MemoryBlockHandle block_handle = NULL;
        NEXUS_Error lrc = NEXUS_SUCCESS;
        int rc = 0;
        void *pMemory;

        get_block_handles(pHandle, &block_handle, NULL);
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

typedef struct private_handle_int_t private_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_PRIV_H_ */