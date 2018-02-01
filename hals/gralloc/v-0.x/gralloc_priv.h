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
#include <cutils/atomic.h>
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
#include <cutils/log.h>

#define GR_MGMT_MODE_LOCKED     1
#define GR_MGMT_MODE_UNLOCKED   2

#define CHECKPOINT_TIMEOUT      (5000)

#define GR_NONE                 (1<<0)
#define GR_STANDARD             (1<<1)
#define GR_YV12                 (1<<2)
#define GR_HWTEX                (1<<3)
#define GR_BLOB                 (1<<4)
#define GR_FP                   (1<<5)
#define GR_YV12_SW              (1<<6)

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

   struct {
      NEXUS_VideoDecoderFrameStatus status;
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

      NEXUS_StripedSurfaceHandle stripedSurface; // gralloc only.
      unsigned destriped;

      unsigned vDepth;
      unsigned vColor;
      unsigned vsWidth;
      unsigned vsLumaHeight;
      unsigned vsChromaHeight;
      unsigned vBackingUpdated;
      unsigned vImageWidth;
      unsigned vImageHeight;
      unsigned vColorSpace;
      NEXUS_MemoryBlockHandle vLumaBlock;
      NEXUS_MemoryBlockHandle vChromaBlock;
      uint64_t vLumaAddr __attribute__((aligned(8)));
      uint64_t vChromaAddr __attribute__((aligned(8)));
      uint32_t vLumaOffset __attribute__((aligned(4)));
      uint32_t vChromaOffset __attribute__((aligned(4)));
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

       if (sdata != NULL) {
          *sdata = 0;
       }
       if (pdata != NULL) {
          *pdata = 0;
       }

       if (pHandle->magic != sMagic) {
          ALOGE("gbh(%p)::deleted", pHandle);
          return -1;
       }

       if ((sdata != NULL) && (pHandle->sdata >= 0)) {
          rc = ioctl(pHandle->sdata, NX_ASHMEM_GET_BLK, &getmem);
          if (rc >= 0) {
             *sdata = (NEXUS_MemoryBlockHandle)getmem.hdl;
          } else {
             int err = errno;
             ALOGE("sdata:gbh(fd:%d)::fail:%d::errno=%d", pHandle->sdata, rc, err);
             return rc;
          }
       }

       if ((pdata != NULL) && (pHandle->pdata >= 0)) {
          rc = ioctl(pHandle->pdata, NX_ASHMEM_GET_BLK, &getmem);
          if (rc >= 0) {
             *pdata = (NEXUS_MemoryBlockHandle)getmem.hdl;
          } else {
             int err = errno;
             ALOGE("pdata:gbh(fd:%d)::fail:%d:errno=%d", pHandle->pdata, rc, err);
             return rc;
          }
       }

       return 0;
    }
#endif
};

typedef struct private_handle_int_t private_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_PRIV_H_ */
