/******************************************************************************
 *    (c)2017 Broadcom Corporation
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
#include "brcm_audio.h"
#include "brcm_audio_nexus_parser.h"

#include "bac3_probe.h"
#include "bmedia_probe.h"
#include "bioatom.h"

using namespace android;

const uint8_t g_nexus_parse_eac3_syncword[2] = { 0x0B, 0x77 };

#define EAC3_HEADER_LEN         (sizeof(g_nexus_parse_eac3_syncword) + 3)

#define EAC3_STRMTYP_MASK       0x03
#define EAC3_STRMTYP_SHIFT      6
#define EAC3_SUBSTREAMID_MASK   0x07
#define EAC3_SUBSTREAMID_SHIFT  3
#define EAC3_FRMSIZ_MASK        0x07
#define EAC3_FRMSIZ_SHIFT       0

#define EAC3_STRMTYP_0 0
#define EAC3_STRMTYP_2 2

const uint8_t *nexus_find_ac3_sync_frame(const uint8_t *data, size_t len, eac3_frame_hdr_info *info)
{
    const uint8_t *syncframe = NULL;

    ALOG_ASSERT(data != NULL && info != NULL);

    syncframe = (const uint8_t *)memmem(data, len, g_nexus_parse_eac3_syncword, sizeof(g_nexus_parse_eac3_syncword));

    if (!syncframe)
        return NULL;

    if (nexus_parse_eac3_frame_hdr(syncframe, len - (syncframe - data), info))
        return syncframe;

    ALOGV("%s: Error parsing AC3 sync frame", __FUNCTION__);
    return NULL;
}

const uint8_t *nexus_find_eac3_independent_frame(const uint8_t *data, size_t len, unsigned substream, eac3_frame_hdr_info *info)
{
    const uint8_t *syncframe = NULL;
    uint8_t strmtyp;
    uint8_t substreamid;
    uint16_t frmsiz;

    ALOG_ASSERT(data != NULL && info != NULL);

    while (len >= EAC3_HEADER_LEN) {
        syncframe = (const uint8_t *)memmem(data, len, g_nexus_parse_eac3_syncword, sizeof(g_nexus_parse_eac3_syncword));
        if (!syncframe)
            return NULL;

        strmtyp = (syncframe[2] >> EAC3_STRMTYP_SHIFT) & EAC3_STRMTYP_MASK;
        substreamid = (syncframe[2] >> EAC3_SUBSTREAMID_SHIFT) & EAC3_SUBSTREAMID_MASK;
        frmsiz = ((uint16_t)((syncframe[2] >> EAC3_FRMSIZ_SHIFT) & EAC3_FRMSIZ_MASK) << 8) + (uint16_t)syncframe[3];

        if (((strmtyp == EAC3_STRMTYP_0) || (strmtyp == EAC3_STRMTYP_2)) &&
            (substreamid == substream)) {
            if (nexus_parse_eac3_frame_hdr(syncframe, len - (syncframe - data), info))
                return syncframe;
            ALOGE("%s: Error parsing EAC3 sync frame", __FUNCTION__);
        }

        // Not found, skip over just the sync word and look for next one
        len -= ((syncframe - data) + sizeof(g_nexus_parse_eac3_syncword));
        data = syncframe + sizeof(g_nexus_parse_eac3_syncword);
    }

    // No more syncwords found
    return NULL;
}


bool nexus_parse_eac3_frame_hdr(const uint8_t *data, size_t len, eac3_frame_hdr_info *info)
{
    batom_vec vec;
    batom_cursor curs, *cursor = &curs;
    bmedia_probe_audio probe;
    uint16_t syncword;
    unsigned num_blocks;
    size_t res;

    ALOG_ASSERT(data != NULL && info != NULL);

    if (len == 0) {
        return false;
    }

    batom_vec_init(&vec, data, len);
    batom_cursor_from_vec(cursor, &vec, 1);

    syncword = batom_cursor_uint16_be(cursor);
    if (syncword != B_AUDIO_PARSER_AC3_SYNCWORD) {
        ALOGW("Incorrect syncword %u", syncword);
        return false;
    }

    res = b_ac3_probe_parse_header(cursor, &probe, &num_blocks);
    if (res == 0) {
        ALOGW("Error parsing AC3 header");
        return false;
    }
    BA_LOG(VERB, "res:%u codec:%u ch:%u sample_size:%u bitrate:%u sample_rate:%u",
        res, probe.codec, probe.channel_count, probe.sample_size, probe.bitrate, probe.sample_rate);

    info->num_audio_blks = num_blocks;
    info->sample_rate = probe.sample_rate;
    info->bitrate = probe.bitrate;
    info->frame_size = res;

    return true;
}
