/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 *****************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_buffer_tracker"

#include "bomx_utils.h"
#include "bomx_buffer_tracker.h"
#include <inttypes.h>

#define BUFTR_LOGE(msg, ...) ALOGE("[%s] " msg, m_pComponent->GetName(), ##__VA_ARGS__)
#define BUFTR_LOGW(msg, ...) ALOGW("[%s] " msg, m_pComponent->GetName(), ##__VA_ARGS__)
#define BUFTR_LOGI(msg, ...) ALOGI("[%s] " msg, m_pComponent->GetName(), ##__VA_ARGS__)
#define BUFTR_LOGV(msg, ...) ALOGV("[%s] " msg, m_pComponent->GetName(), ##__VA_ARGS__)

#define BUFTR_CLEANUP_THRESHOLD (300)
#define BUFTR_CLEANUP_KEEP (30)

BOMX_BufferTracker::BOMX_BufferTracker(const BOMX_Component *pComponent, unsigned minEntries) :
    m_pComponent(pComponent),
    m_minAllocated(minEntries),
    m_maxAllocated(minEntries),
    m_valid(true)
{
    unsigned i;

    BLST_Q_INIT(&m_allocList);
    BLST_Q_INIT(&m_freeList);

    for ( i = 0; i < minEntries; i++ )
    {
        BOMX_BufferTrackerNode *pNode;
        pNode = new BOMX_BufferTrackerNode;
        if ( NULL == pNode )
        {
            m_valid = false;
            (void)BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
            return;
        }

        BLST_Q_INSERT_TAIL(&m_freeList, pNode, node);
    }
}

BOMX_BufferTracker::~BOMX_BufferTracker()
{
    BOMX_BufferTrackerNode *pNode;
    while ( NULL != (pNode=BLST_Q_FIRST(&m_allocList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_allocList, node);
        delete pNode;
    }
    while ( NULL != (pNode=BLST_Q_FIRST(&m_freeList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_freeList, node);
        delete pNode;
    }
    m_valid = false;
    BUFTR_LOGI("%u min entries %u max entries", m_minAllocated, m_maxAllocated);
}

bool BOMX_BufferTracker::Add(
    const OMX_BUFFERHEADERTYPE *pHeader,
    uint32_t *pPts /* [out] */
    )
{
    ALOG_ASSERT(NULL != pHeader);

    // Get a node
    BOMX_BufferTrackerNode *pNode = BLST_Q_FIRST(&m_freeList);
    if ( pNode )
    {
        BLST_Q_REMOVE_HEAD(&m_freeList, node);
    }
    else
    {
        pNode = new BOMX_BufferTrackerNode;
        if ( NULL == pNode )
        {
            BOMX_BERR_TRACE(BERR_OUT_OF_SYSTEM_MEMORY);
            return false;
        }
        m_maxAllocated++;
    }

    // Populate node
    pNode->ticks = pHeader->nTimeStamp;
    pNode->flags = pHeader->nFlags;
    pNode->pts = BOMX_TickToPts(&pHeader->nTimeStamp);
    BUFTR_LOGV("Adding PTS %#" PRIx32 " for tick %08" PRIx32 " %08" PRIx32 " flags %#" PRIx32, pNode->pts, (int32_t)(pNode->ticks>>(OMX_TICKS)32), (int32_t)pNode->ticks, pNode->flags);

    // Add to list sorted in display order
    BOMX_BufferTrackerNode *pPrev = BLST_Q_LAST(&m_allocList);
    if ( NULL == pPrev || pPrev->pts <= pNode->pts )
    {
        // Typical case, just add to tail
        BLST_Q_INSERT_TAIL(&m_allocList, pNode, node);
    }
    else
    {
        // Check if PTS has wrapped before we scan the whole list
        if ( pNode->pts < BLST_Q_FIRST(&m_allocList)->pts )
        {
            BLST_Q_INSERT_HEAD(&m_allocList, pNode, node);
        }
        else
        {
            // Scan backward until we find a lesser PTS
            do {
                pPrev = BLST_Q_PREV(pPrev, node);
            } while ( NULL != pPrev && pPrev->pts > pNode->pts );
            ALOG_ASSERT(NULL != pPrev); // Should never happen - above checks for head/tail should have caught this
            BLST_Q_INSERT_AFTER(&m_allocList, pPrev, pNode, node);
        }
    }

    if ( NULL != pPts )
    {
        *pPts = pNode->pts;
    }

    return true;
}

bool BOMX_BufferTracker::Remove(
    uint32_t pts,
    OMX_BUFFERHEADERTYPE *pHeader /* [out] - sets flags and ticks value */
    )
{
    ALOG_ASSERT(NULL != pHeader);

    // Find node
    BOMX_BufferTrackerNode *pNode;
    uint32_t miss = 0;
    // First node should ideally be the one we're returning but search for it
    for ( pNode = BLST_Q_FIRST(&m_allocList);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, node) )
    {
        if ( pNode->pts == pts )
            break;

        miss++;
    }

    // See if we found a match
    if ( NULL == pNode )
    {
        BUFTR_LOGW("PTS %" PRIu32 " not in tracker", pts);
        BOMX_PtsToTick(pts, &pHeader->nTimeStamp);
        pHeader->nFlags = 0;

        return false;
    }
    else
    {
        BUFTR_LOGV("Matched PTS %#" PRIx32 " to tick %08" PRIx32 " %08" PRIx32 " flags %#" PRIx32 "", pts, (int)(pNode->ticks>>(OMX_TICKS)32), (int)pNode->ticks, pNode->flags);
        BLST_Q_REMOVE(&m_allocList, pNode, node);
        BLST_Q_INSERT_TAIL(&m_freeList, pNode, node);
        pHeader->nFlags = pNode->flags;
        pHeader->nTimeStamp = pNode->ticks;

        // Clean up unmatched PTS entires to avoid growing the list indefinitely
        if ( miss > BUFTR_CLEANUP_THRESHOLD )
        {
            BOMX_BufferTrackerNode *pNextNode = NULL;
            pNode = BLST_Q_FIRST(&m_allocList);
            while ( NULL != pNode && miss >= BUFTR_CLEANUP_KEEP )
            {
                pNextNode = BLST_Q_NEXT(pNode, node);

                if ( pNode->ticks != 0 && pNode->ticks < pHeader->nTimeStamp )
                {
                    BUFTR_LOGV("Force remove PTS %#" PRIx32 " to tick %08" PRIx32 " %08" PRIx32 " flags %#" PRIx32 "", pNode->pts, (int)(pNode->ticks>>(OMX_TICKS)32), (int)pNode->ticks, pNode->flags);

                    BLST_Q_REMOVE(&m_allocList, pNode, node);
                    BLST_Q_INSERT_TAIL(&m_freeList, pNode, node);

                    miss--;
                }

                pNode = pNextNode;
            }
        }

        return true;
    }
}

void BOMX_BufferTracker::Flush()
{
    BOMX_BufferTrackerNode *pNode;
    BUFTR_LOGV("Flush");
    while ( NULL != (pNode=BLST_Q_FIRST(&m_allocList)) )
    {
        BLST_Q_REMOVE_HEAD(&m_allocList, node);
        BLST_Q_INSERT_TAIL(&m_freeList, pNode, node);
    }
}

bool BOMX_BufferTracker::Last(OMX_TICKS ticks)
{
    BOMX_BufferTrackerNode *pNode;
    for ( pNode = BLST_Q_FIRST(&m_allocList);
          NULL != pNode;
          pNode = BLST_Q_NEXT(pNode, node) )
    {
        if ( pNode->ticks > ticks )
            break;
    }

    return ( NULL == pNode );
}
