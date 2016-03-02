#include "nxclient.h"
#include "nexus_dma.h"
#include "nexus_platform_client.h"
#include "nexus_base_os.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <string.h>
#include "sage_srai.h"

#define LOG_TAG "testdma"
#include <cutils/log.h>

enum test_mode {
   test_mode_default_heap_to_default_heap,
   test_mode_default_heap_to_crr,
   test_mode_main_heap_to_main_heap,
   test_mode_main_heap_to_crr,
   test_mode_crr_to_crr,
};

static const char *mode_to_string[] = {
   "default_heap_to_default_heap",
   "default_heap_to_crr",
   "main_heap_to_main_heap",
   "main_heap_to_crr",
   "crr_to_crr",
};

static int64_t tick(void)
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static void complete(void *context, int param)
{
    (void)param;
    BKNI_SetEvent(context);
}

int main(int argc, char **argv)
{
    NEXUS_MemoryAllocationSettings allocSettings;
    int rc;
    unsigned bufferSize = 1024*1024*2;
    unsigned halfBufferSize = bufferSize / 2;
    void *buffer, *secureBuffer;
    NEXUS_DmaHandle dma;
    NEXUS_DmaJobHandle job;
    NEXUS_DmaJobSettings jobSettings;
    BKNI_EventHandle event;
    unsigned i;
    const long double nsec_to_msec = 1.0 / 1000000.0;
    int64_t start, dma_op, total, total_mem;
    enum test_mode mode = test_mode_default_heap_to_default_heap;
    unsigned iter = 1;
    NEXUS_MemoryBlockProperties blockProps;
    char bufIn[256], bufOut[256];
    NEXUS_MemoryStatus status;
    unsigned char *writeBuffer = NULL;

    if (argc > 1) mode = (enum test_mode)atoi(argv[1]);

    memset(bufIn, 0, sizeof(bufIn));
    memset(bufOut, 0, sizeof(bufOut));

    rc = NxClient_Join(NULL);
    if (rc) return -1;

    BKNI_CreateEvent(&event);

    NEXUS_Memory_GetDefaultAllocationSettings(&allocSettings);
    if (mode == test_mode_default_heap_to_default_heap) {
       rc = NEXUS_Memory_Allocate(bufferSize, &allocSettings, &buffer);
       BDBG_ASSERT(!rc);
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(buffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufIn, sizeof(bufIn));
          NEXUS_Heap_ToString(&status, bufOut, sizeof(bufOut));
       }
    } else if (mode == test_mode_default_heap_to_crr || mode == test_mode_main_heap_to_crr) {
       if (mode == test_mode_main_heap_to_crr) {
          NEXUS_ClientConfiguration clientConfig;
          NEXUS_Platform_GetClientConfiguration(&clientConfig);
          allocSettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
       }
       rc = NEXUS_Memory_Allocate(halfBufferSize, &allocSettings, &buffer);
       BDBG_ASSERT(!rc);
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(buffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufIn, sizeof(bufIn));
       }
       uint8_t *sec_ptr = SRAI_Memory_Allocate(halfBufferSize, SRAI_MemoryType_SagePrivate);
       BDBG_ASSERT(sec_ptr==NULL);
       secureBuffer = (void *)sec_ptr;
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(secureBuffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufOut, sizeof(bufOut));
       }
    } else if (mode == test_mode_main_heap_to_main_heap) {
       NEXUS_ClientConfiguration clientConfig;
       NEXUS_Platform_GetClientConfiguration(&clientConfig);
       allocSettings.heap = clientConfig.heap[NXCLIENT_FULL_HEAP];
       rc = NEXUS_Memory_Allocate(bufferSize, &allocSettings, &buffer);
       BDBG_ASSERT(!rc);
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(buffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufIn, sizeof(bufIn));
          NEXUS_Heap_ToString(&status, bufOut, sizeof(bufOut));
       }
    } else if (mode == test_mode_crr_to_crr) {
       uint8_t *sec_ptr = SRAI_Memory_Allocate(halfBufferSize, SRAI_MemoryType_SagePrivate);
       BDBG_ASSERT(sec_ptr==NULL);
       buffer = (void *)sec_ptr;
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(buffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufIn, sizeof(bufIn));
       }
       sec_ptr = SRAI_Memory_Allocate(halfBufferSize, SRAI_MemoryType_SagePrivate);
       BDBG_ASSERT(sec_ptr==NULL);
       secureBuffer = (void *)sec_ptr;
       NEXUS_MemoryBlock_GetProperties(NEXUS_MemoryBlock_FromAddress(secureBuffer), &blockProps);
       if (blockProps.heap != NULL) {
          NEXUS_Heap_GetStatus(blockProps.heap, &status);
          NEXUS_Heap_ToString(&status, bufOut, sizeof(bufOut));
       }
    }
    dma = NEXUS_Dma_Open(NEXUS_ANY_ID, NULL);
    BDBG_ASSERT(dma);

    NEXUS_DmaJob_GetDefaultSettings(&jobSettings);
    jobSettings.completionCallback.callback = complete;
    jobSettings.completionCallback.context = event;
    if (mode == test_mode_default_heap_to_crr || mode == test_mode_main_heap_to_crr || mode == test_mode_crr_to_crr) {
       jobSettings.bypassKeySlot = NEXUS_BypassKeySlot_eGR2R;
    }
    job = NEXUS_DmaJob_Create(dma, &jobSettings);
    BDBG_ASSERT(job);

    if (mode == test_mode_default_heap_to_default_heap || mode == test_mode_main_heap_to_main_heap) {
       writeBuffer = (unsigned char *)buffer + halfBufferSize;
    } else if (mode == test_mode_default_heap_to_crr || mode == test_mode_main_heap_to_crr || mode == test_mode_crr_to_crr) {
       writeBuffer = (unsigned char *)&secureBuffer;
    }

    total = 0;
    total_mem = 0;
    for (i=0;i<iter;i++) {
        NEXUS_DmaJobBlockSettings blockSettings;

        if (mode != test_mode_crr_to_crr) {
           unsigned char value = rand();
           ALOGV("test %d: writing %d", i, value);
           memset(buffer, value, halfBufferSize);
           NEXUS_FlushCache(buffer, halfBufferSize);
        }

        start = tick();
        NEXUS_DmaJob_GetDefaultBlockSettings(&blockSettings);
        blockSettings.pSrcAddr = buffer;
        blockSettings.pDestAddr = writeBuffer;
        blockSettings.blockSize = halfBufferSize;
        blockSettings.cached = false;
        rc = NEXUS_DmaJob_ProcessBlocks(job, &blockSettings, 1);
        if (rc == NEXUS_DMA_QUEUED) {
            NEXUS_DmaJobStatus status;
            rc = BKNI_WaitForEvent(event, BKNI_INFINITE);
            BDBG_ASSERT(!rc);
            rc = NEXUS_DmaJob_GetStatus(job, &status);
            BDBG_ASSERT(!rc);
            BDBG_ASSERT(status.currentState == NEXUS_DmaJobState_eComplete);
        }
        else {
            BDBG_ASSERT(!rc);
        }
        dma_op = tick()-start;
        total += dma_op;

        if (mode == test_mode_default_heap_to_default_heap || mode == test_mode_main_heap_to_main_heap) {
           unsigned j;
           NEXUS_FlushCache(writeBuffer, halfBufferSize);
           for (j=0;j<halfBufferSize;j++) {
               BDBG_ASSERT(writeBuffer[j] == value);
           }
        }

        if (mode == test_mode_default_heap_to_default_heap || mode == test_mode_main_heap_to_main_heap) {
           start = tick();
           BKNI_Memcpy(writeBuffer, buffer, halfBufferSize);
           dma_op = tick()-start;
           total_mem += dma_op;
        }
    }

    ALOGI("dma-xfer (%s): %p (%s) -> %p (%s); size: %u; repeat: %u; total time: %.5Lf (avg. %.5Lf)",
          mode_to_string[mode], buffer, bufIn, writeBuffer, bufOut, halfBufferSize, iter, nsec_to_msec * total, (nsec_to_msec * total)/iter);
    if (mode == test_mode_default_heap_to_default_heap || mode == test_mode_main_heap_to_main_heap) {
       ALOGI("memcpy (%s): %p (%s) -> %p (%s); size: %u; repeat: %u; total time: %.5Lf (avg. %.5Lf)",
             mode_to_string[mode], buffer, bufIn, writeBuffer, bufOut, halfBufferSize, iter, nsec_to_msec * total_mem, (nsec_to_msec * total_mem)/iter);
    }

    NEXUS_DmaJob_Destroy(job);
    NEXUS_Dma_Close(dma);
    BKNI_DestroyEvent(event);
    NEXUS_Memory_Free(&buffer);
    if (mode == test_mode_default_heap_to_crr || mode == test_mode_main_heap_to_crr) {
       SRAI_Memory_Free((uint8_t*)secureBuffer);
    }
    if (mode == test_mode_crr_to_crr) {
       SRAI_Memory_Free((uint8_t*)buffer);
    }
    NxClient_Uninit();
    return 0;
}
