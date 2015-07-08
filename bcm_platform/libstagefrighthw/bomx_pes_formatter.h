/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
#ifndef BOMX_PES_FORMATTER_H__
#define BOMX_PES_FORMATTER_H__

#include "nexus_platform.h"

#define B_MAX_PES_PACKET_LENGTH (65535)     // PES packets have a 16-bit length field.  0 indicates unbounded.
#define B_PES_HEADER_START_BYTES (6)        // 00 00 01 [Stream ID] [Length 0] [Length 1] are not reported in the packet length field.
#define B_PES_HEADER_LENGTH_WITH_PTS (14)
#define B_PES_HEADER_LENGTH_WITHOUT_PTS (9)
#define B_BPP_PACKET_LEN (184)

class BOMX_PesFormatter
{
public:
    BOMX_PesFormatter(uint16_t streamId);
    virtual ~BOMX_PesFormatter();

    size_t InitHeader(uint8_t *pHeaderBuf, size_t maxHeaderBytes) { return InitHeader(pHeaderBuf, maxHeaderBytes, false, 0); }
    size_t InitHeader(uint8_t *pHeaderBuf, size_t maxHeaderBytes, uint32_t pts) { return InitHeader(pHeaderBuf, maxHeaderBytes, true, pts); }

    void SetPayloadLength(uint8_t *pHeaderBuf, uint16_t payloadLen);

    void FormBppPacket(char *pBuffer, uint32_t opcode);

protected:
    size_t InitHeader(uint8_t *pHeaderBuf, size_t maxHeaderBytes, bool ptsValid, uint32_t pts);

    uint16_t m_streamId;
};

#endif /* #ifndef BOMX_PES_FORMATTER_H__ */