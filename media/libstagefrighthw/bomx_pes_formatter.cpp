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
//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_pes_formatter"

#include "bomx_pes_formatter.h"
#include "biobits.h"
#include "nexus_platform.h"
#include <log/log.h>

BOMX_PesFormatter::BOMX_PesFormatter(uint16_t streamId) :
    m_streamId(streamId)
{
    // Nothing to do
}

BOMX_PesFormatter::~BOMX_PesFormatter()
{
    // Nothing to do
}

void BOMX_PesFormatter::SetPayloadLength(uint8_t *pHeaderBuf, uint16_t payloadLen)
{
    ALOG_ASSERT(NULL != pHeaderBuf);
    pHeaderBuf[4] = (payloadLen >> 8) & 0xff;
    pHeaderBuf[5] = payloadLen & 0xff;
}

size_t BOMX_PesFormatter::InitHeader(uint8_t *pHeaderBuf, size_t maxHeaderBytes, bool ptsValid, uint32_t pts)
{
    size_t headerSize = ptsValid ? B_PES_HEADER_LENGTH_WITH_PTS : B_PES_HEADER_LENGTH_WITHOUT_PTS;

    if ( headerSize > maxHeaderBytes )
    {
        ALOGE("Unable to create PES header (insufficient buffer space)");
        return 0;
    }

    pHeaderBuf[0] = 0x00;
    pHeaderBuf[1] = 0x00;
    pHeaderBuf[2] = 0x01;
    pHeaderBuf[3] = m_streamId;
    pHeaderBuf[4] = 0;
    pHeaderBuf[5] = 0;
    pHeaderBuf[6] = 0x81;                   /* Indicate header with 0x10 in the upper bits, original material */
    if ( ptsValid )
    {
        pHeaderBuf[7] = 0x80;
        pHeaderBuf[8] = 0x05;
        pHeaderBuf[9] =
            B_SET_BITS("0010", 0x02, 7, 4) |
            B_SET_BITS("PTS [32..30]", B_GET_BITS(pts,31,29), 3, 1) |
            B_SET_BIT(marker_bit, 1, 0);
        pHeaderBuf[10] = B_GET_BITS(pts,28,21); /* PTS [29..15] -> PTS [29..22] */
        pHeaderBuf[11] =
            B_SET_BITS("PTS [29..15] -> PTS [21..15]", B_GET_BITS(pts,20,14), 7, 1) |
            B_SET_BIT(marker_bit, 1, 0);
        pHeaderBuf[12] = B_GET_BITS(pts,13,6); /* PTS [14..0] -> PTS [14..7]  */
        pHeaderBuf[13] =
            B_SET_BITS("PTS [14..0] -> PTS [6..0]", B_GET_BITS(pts,5,0), 7, 2) |
            B_SET_BIT("PTS[0]", 0, 1) |
            B_SET_BIT(marker_bit, 1, 0);
    }
    else
    {
        pHeaderBuf[7] = 0;
        pHeaderBuf[8] = 0;
    }
    return headerSize;
}

void BOMX_PesFormatter::FormBppPacket(char *pBuffer, uint32_t opcode)
{
    BKNI_Memset(pBuffer, 0, B_BPP_PACKET_LEN);
    /* PES Header */
    pBuffer[0] = 0x00;
    pBuffer[1] = 0x00;
    pBuffer[2] = 0x01;
    pBuffer[3] = m_streamId;
    pBuffer[4] = 0x00;
    pBuffer[5] = 178;
    pBuffer[6] = 0x81;
    pBuffer[7] = 0x01;
    pBuffer[8] = 0x14;
    pBuffer[9] = 0x80;
    pBuffer[10] = 0x42; /* B */
    pBuffer[11] = 0x52; /* R */
    pBuffer[12] = 0x43; /* C */
    pBuffer[13] = 0x4d; /* M */
    /* 14..25 are unused and set to 0 by above memset */
    pBuffer[26] = 0xff;
    pBuffer[27] = 0xff;
    pBuffer[28] = 0xff;
    /* sub-stream id - not used in this config */;
    /* Next 4 are opcode 0000_000ah = inline flush/tpd */
    pBuffer[30] = (opcode>>24) & 0xff;
    pBuffer[31] = (opcode>>16) & 0xff;
    pBuffer[32] = (opcode>>8) & 0xff;
    pBuffer[33] = (opcode>>0) & 0xff;
}
