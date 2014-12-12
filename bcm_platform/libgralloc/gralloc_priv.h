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
#include "nexus_video_decoder_types.h"
#include "nexus_striped_surface.h"
#include "nexus_surface.h"

#define BCM_DEBUG_MSG
#define BCM_DEBUG_TRACEMSG      LOGD
#define BCM_DEBUG_ERRMSG        LOGD

#define MAX_NUM_INSTANCES       2

#define DEFAULT_PLANE           0
#define EXTRA_PLANE             1

/*****************************************************************************/

struct private_module_t;
struct private_handle_t;

struct private_module_t {
   gralloc_module_t base;

   struct private_handle_t* framebuffer;
   uint32_t numBuffers;
   uint32_t bufferMask;
   pthread_mutex_t lock;
};

/*****************************************************************************/
/*
 * This shared data is for deferred allocation scheme
 * */

typedef struct __SHARED_DATA_ {
   struct {
      volatile int32_t windowIdPlusOne;
      void *           nexusClientContext;
   } videoWindow;

   //Metadata For Video Buffers
   struct {
      NEXUS_VideoDecoderFrameStatus status;
      NEXUS_StripedSurfaceHandle hStripedSurface;
   } videoFrame;

   struct {
      unsigned format;
      unsigned bpp;
      unsigned width;
      unsigned height;
      unsigned physAddr;
      unsigned size;
  } planes[MAX_NUM_INSTANCES];

  struct {
    int32_t active;
    int layer;
    NEXUS_SurfaceHandle surface;
  } hwc;
} SHARED_DATA, *PSHARED_DATA;

#ifdef __cplusplus
struct private_handle_t : public native_handle {
#else
struct private_handle_t {
                struct native_handle nativeHandle;
#endif

// file-descriptors
/*1.*/        int         fd;    // default data plane
/*2.*/        int         fd2;   // used for the small shared data block (SHARED_DATA)
/*3.*/        int         fd3;   // extra data plane

/*Ints Counter*/
/*1.*/        int         magic;
/*2.*/        int         flags;
/*3.*/        int         pid;
/*4.*/        unsigned    oglStride;
/*5.*/        unsigned    oglFormat;
/*6.*/        unsigned    oglSize;
/*7.*/        unsigned    sharedData;
/*8.*/        int         usage;

// do not use, only for backward compatibility.
/*9.*/        unsigned    nxSurfacePhysicalAddress;

#ifdef __cplusplus
    static const int sNumInts = 9;
    static const int sNumFds = 3;
    static const int sMagic = 0x3141592;

    private_handle_t(int fd, int fd2, int fd3, int flags) :
        fd(fd), fd2(fd2), fd3(fd3), magic(sMagic), flags(flags),
        pid(getpid()), oglStride(0), oglFormat(0), oglSize(0),
        sharedData(0), usage(0)
    {
        version = sizeof(native_handle);
        numInts = sNumInts;
        numFds = sNumFds;
    }

    //
    // Free the shared data in Destructor
    //
    ~private_handle_t()
    {
        magic = 0;
    }

    static int validate(const native_handle* h)
    {
        const private_handle_t* hnd = (const private_handle_t*)h;
        if (!h || h->version != sizeof(native_handle) ||
                h->numInts != sNumInts || h->numFds != sNumFds ||
                hnd->magic != sMagic)
        {
            return -EINVAL;
        }
        return 0;
    }

#endif
};

#ifdef __cplusplus
}
#endif

#endif /* GRALLOC_PRIV_H_ */
