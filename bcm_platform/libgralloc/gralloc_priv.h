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
#include "DisplayFrame.h"

/*****************************************************************************/

struct private_module_t;
struct private_handle_t;

struct private_module_t {
   gralloc_module_t base;

   struct private_handle_t* framebuffer;
   uint32_t numBuffers;
   uint32_t bufferMask;
   pthread_mutex_t lock;
   void *nexSurf;
};

/*****************************************************************************/
/*
 * This shared data is for deferred allocation scheme
 * */

#define PRIV_FLAGS_FRAMEBUFFER 0x00000001

typedef struct __SHARED_DATA_ {
   struct {
      volatile int32_t layerIdPlusOne;
      volatile int32_t windowIdPlusOne;
      bool             windowVisible;
      void *           nexusClientContext;
      int64_t          timestamp;
   } videoWindow;

   //Metadata For Video Buffers
   DISPLAY_FRAME    DisplayFrame;
} SHARED_DATA, *PSHARED_DATA;

#ifdef __cplusplus
struct private_handle_t : public native_handle {
#else
struct private_handle_t {
                struct native_handle nativeHandle;
#endif

// file-descriptors
/*1.*/        int         fd;
/*2.*/        int         fd2;   // used for the small shared data block (SHARED_DATA)

/*Ints Counter*/
/*1.*/        unsigned    nxSurfacePhysicalAddress;
/*2.*/        int         magic;
/*3.*/        int         flags;
/*4.*/        int         size;
/*5.*/        int         pid;
/*6.*/        void *      lockTmp; // used to allocate memory (per process) which is freed in unlock
/*7.*/        void *      lockHnd; // used for HW converter so we can open one per process
/*8.*/        void *      lockEvent; // used for HW converter to trigger completion
/*9.*/        void *      lockHWCLE; // used for the CLE to populate the job
/*10.*/       unsigned    width;
/*11.*/       unsigned    height;
/*12.*/       unsigned    format;
/*13.*/       unsigned    bpp;
/*14.*/       unsigned    oglStride;
/*15.*/       unsigned    oglFormat;
/*16.*/       unsigned    oglSize;
/*17.*/       unsigned    sharedDataPhyAddr;  // physical address of shared Data.

#ifdef __cplusplus
    static const int sNumInts = 17;
    static const int sNumFds = 2;
    static const int sMagic = 0x3141592;

    private_handle_t(int fd, int fd2, int size, int flags) :
        fd(fd), fd2(fd2), nxSurfacePhysicalAddress(0),
        magic(sMagic), flags(flags), size(size),
        pid(getpid()), lockTmp(0), lockHnd(0), lockEvent(0), lockHWCLE(0),
        oglStride(0), oglFormat(0), oglSize(0)
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
