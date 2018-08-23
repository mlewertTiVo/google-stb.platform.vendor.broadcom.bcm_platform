/******************************************************************************
 *    (c)2010-2016 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "nexus_base_mmap.h"
#include "nexus_platform.h"
#include "nxclient.h"
#include "bomx_secure_buff.h"
#include "log/log.h"
#include "sage_srai.h"

#undef LOG_TAG
#define LOG_TAG "bomx_secbuff"

NEXUS_Error BOMX_AllocSecureBuffer(size_t size, bool allocClearBuffer, NEXUS_MemoryBlockHandle *phSecureBuffer)
{
    NEXUS_Error nx_rc;
    NEXUS_ClientConfiguration clientConfig;
    NEXUS_HeapHandle hGlobalHeap;
    NEXUS_MemoryBlockHandle hMemBlock = NULL;
    void *pMemory;
    BOMX_SecBufferSt *pSecBufferSt;
    const size_t align = 16;
    size_t globalBuffSize = sizeof(BOMX_SecBufferSt);

    BDBG_ASSERT(phSecureBuffer != NULL);
    if (allocClearBuffer)
        globalBuffSize += size;
    globalBuffSize = (globalBuffSize + (align -1)) & ~(align - 1);

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    hGlobalHeap = clientConfig.heap[NXCLIENT_FULL_HEAP];
    if (hGlobalHeap == NULL) {
        ALOGE("%s: error accessing main heap", __FUNCTION__);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    hMemBlock = NEXUS_MemoryBlock_Allocate(hGlobalHeap, globalBuffSize, align, NULL);
    if (hMemBlock == NULL) {
        ALOGE("%s: unable to allocate global memory block", __FUNCTION__);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    pMemory = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(hMemBlock, &pMemory);
    if (nx_rc || pMemory == NULL) {
        ALOGE("%s: unable to lock global memory block", __FUNCTION__);
        if (!nx_rc)
            nx_rc = NEXUS_OUT_OF_DEVICE_MEMORY;
        NEXUS_MemoryBlock_Free(hMemBlock);
        return nx_rc;
    }
    pSecBufferSt = (BOMX_SecBufferSt*)pMemory;
    memset(pSecBufferSt, 0, sizeof(*pSecBufferSt));
    // Allocate secure buffer
    uint8_t *secureBuff = SRAI_Memory_Allocate(size, SRAI_MemoryType_SagePrivate);
    if (secureBuff == NULL)
    {
        ALOGE("%s: Failed to allocate secure buffer", __FUNCTION__);
        NEXUS_MemoryBlock_Unlock(hMemBlock);
        NEXUS_MemoryBlock_Free(hMemBlock);
        return NEXUS_OUT_OF_DEVICE_MEMORY;
    }
    pSecBufferSt->pSecureBuff = secureBuff;
    pSecBufferSt->hSecureBuff = NEXUS_MemoryBlock_FromAddress(secureBuff);
    NEXUS_Platform_SetSharedHandle(pSecBufferSt->hSecureBuff, true);
    if (allocClearBuffer)
        pSecBufferSt->clearBuffSize = size;
    NEXUS_Platform_SetSharedHandle(hMemBlock, true);
    *phSecureBuffer = hMemBlock;
    NEXUS_MemoryBlock_Unlock(hMemBlock);
    return NEXUS_SUCCESS;
}

void BOMX_FreeSecureBuffer(NEXUS_MemoryBlockHandle hSecureBuffer)
{
    void *pMemory;
    BOMX_SecBufferSt *pSecureBufferSt;
    NEXUS_Error nx_rc;

    BDBG_ASSERT(hSecureBuffer != NULL);
    NEXUS_Platform_SetSharedHandle(hSecureBuffer, false);
    pMemory = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(hSecureBuffer, &pMemory);
    if (nx_rc || pMemory == NULL) {
        ALOGW("%s: unable to lock secure buffer handle", __FUNCTION__);
        NEXUS_MemoryBlock_Free(hSecureBuffer);
        return;
    }
    pSecureBufferSt = (BOMX_SecBufferSt*)pMemory;
    NEXUS_Platform_SetSharedHandle(pSecureBufferSt->hSecureBuff, false);
    SRAI_Memory_Free(pSecureBufferSt->pSecureBuff);
    NEXUS_MemoryBlock_Unlock(hSecureBuffer);
    NEXUS_MemoryBlock_Free(hSecureBuffer);
}

NEXUS_Error BOMX_LockSecureBuffer(NEXUS_MemoryBlockHandle hSecureBuffer, BOMX_SecBufferSt **pSecureBufferSt)
{
    void *pMemory;
    NEXUS_Error nx_rc;

    BDBG_ASSERT(hSecureBuffer != NULL && pSecureBufferSt != NULL);
    pMemory = NULL;
    nx_rc = NEXUS_MemoryBlock_Lock(hSecureBuffer, &pMemory);
    if (nx_rc || pMemory == NULL) {
        ALOGW("%s: unable to lock secure buffer handle", __FUNCTION__);
        if (!nx_rc)
            nx_rc = NEXUS_OUT_OF_DEVICE_MEMORY;
        return nx_rc;
    }
    *pSecureBufferSt = (BOMX_SecBufferSt*)pMemory;
    return NEXUS_SUCCESS;
}

void BOMX_UnlockSecureBuffer(NEXUS_MemoryBlockHandle hSecureBuffer)
{
    BDBG_ASSERT(hSecureBuffer != NULL);
    NEXUS_MemoryBlock_Unlock(hSecureBuffer);
}

