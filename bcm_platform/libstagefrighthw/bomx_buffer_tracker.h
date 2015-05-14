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
#ifndef BOMX_BUFFER_TRACKER_H__
#define BOMX_BUFFER_TRACKER_H__

#include "bstd.h"
#include "bdbg.h"
#include "bkni.h"
#include "OMX_Core.h"
#include "OMX_Types.h"
#include "blst_queue.h"
#include "bomx_component.h"

struct BOMX_BufferTrackerNode
{
    BLST_Q_ENTRY(BOMX_BufferTrackerNode) node;
    OMX_TICKS ticks;
    OMX_U32 flags;
    uint32_t pts;
};

class BOMX_BufferTracker
{
public:
    BOMX_BufferTracker(const BOMX_Component *pComponent, unsigned minEntries=1024);
    ~BOMX_BufferTracker();

    bool Add(const OMX_BUFFERHEADERTYPE *pHeader, uint32_t *pPts /* [out] */);
    bool Remove(uint32_t pts, OMX_BUFFERHEADERTYPE *pHeader /* [out] - sets flags and ticks value */);
    void Flush();
    bool Last(OMX_TICKS ticks);
    bool Valid() const { return m_valid; }

protected:
    const BOMX_Component *m_pComponent;
    BLST_Q_HEAD(AllocList, BOMX_BufferTrackerNode) m_allocList;
    BLST_Q_HEAD(FreeList, BOMX_BufferTrackerNode) m_freeList;
    unsigned m_minAllocated;
    unsigned m_maxAllocated;
    bool m_valid;
};

#endif /* #ifndef BOMX_BUFFER_TRACKER_H__*/
