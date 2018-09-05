 /***************************************************************************
  *  Copyright (C) 2018 Broadcom.
  *  The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
  *
  *  This program is the proprietary software of Broadcom and/or its licensors,
  *  and may only be used, duplicated, modified or distributed pursuant to
  *  the terms and conditions of a separate, written license agreement executed
  *  between you and Broadcom (an "Authorized License").  Except as set forth in
  *  an Authorized License, Broadcom grants no license (express or implied),
  *  right to use, or waiver of any kind with respect to the Software, and
  *  Broadcom expressly reserves all rights in and to the Software and all
  *  intellectual property rights therein. IF YOU HAVE NO AUTHORIZED LICENSE,
  *  THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD
  *  IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
  *
  *  Except as expressly set forth in the Authorized License,
  *
  *  1.     This program, including its structure, sequence and organization,
  *  constitutes the valuable trade secrets of Broadcom, and you shall use all
  *  reasonable efforts to protect the confidentiality thereof, and to use this
  *  information only in connection with your use of Broadcom integrated circuit
  *  products.
  *
  *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
  *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS
  *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
  *  RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL
  *  IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR
  *  A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
  *  ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
  *  THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
  *
  *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
  *  OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
  *  INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
  *  RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
  *  HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
  *  EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
  *  WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
  *  FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
  *
  * Module Description:
  *
  **************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_mpeg2_parser"

#include "bomx_mpeg2_parser.h"
#include "biobits.h"
#include "bioatom.h"
#include "bmedia_util.h"
#include "bmedia_probe_util.h"
#include <log/log.h>
#include <sys/param.h>

#define B_MPEG2_PARSER_READ_BIT(name, value) value = batom_bitstream_bit(bs); if (value<0) return false

#define B_MPEG2_UNUSED_FIELD(name, bits) batom_bitstream_drop_bits(bs, bits)
#define B_MPEG2_UNUSED_VLC(name) batom_bitstream_exp_golomb(bs)

// Code partially borrowed from bmpeg_video_probe.c

typedef struct bmedia_probe_video_mpeg_video *bmpeg_video_probe_t;

struct bmedia_probe_video_mpeg_video {
    uint16_t seq_scode;
    uint8_t last_scode;
};


static bool b_mpeg_video_parse_sequence(batom_cursor *cursor, bool bLog)
{
    uint32_t temp, tempBitrate;
    unsigned frame_rate_code;
    unsigned bitrate_value;
    unsigned width, height, bitrate, framerate;

    static const unsigned  frame_rates[16] = {
        0,      /* 0 */
        23976,  /* 1 */
        24000,  /* 2 */
        25000,  /* 3 */
        29970,  /* 4 */
        30000,  /* 5 */
        50000,  /* 6 */
        59940,  /* 7 */
        60000,  /* 8 */
        0,      /* 9 */
        0,      /* 10 */
        0,      /* 11 */
        0,      /* 12 */
        0,      /* 13 */
        0,      /* 14 */
        0       /* 15 */
    };

    /* ISO/IEC 13818-2 : 2000 (E), Table 6.2.2.1 Sequence header */
    temp = batom_cursor_uint32_be(cursor);
    tempBitrate = batom_cursor_uint32_be(cursor);
    if(BATOM_IS_EOF(cursor)) {return false;}
    width = B_GET_BITS(temp, 31, 20);
    height = B_GET_BITS(temp, 19, 8);
    bitrate_value = B_GET_BITS(tempBitrate, 31, 14);
    if(bitrate_value!=B_GET_BITS(0xFFFFFFFFul, 31, 14)) {
        bitrate = bitrate_value;
    } else {
        bitrate = 0;
    }
    frame_rate_code = B_GET_BITS(temp, 3, 0);
    if(B_GET_BITS(temp, 7, 4)==0 || /* Table 6-3 . aspect_ratio_information */
        frame_rate_code==0    /* Table 6-4 . frame_rate_value */
                        ) {
        return false;
    }
    framerate = frame_rates[frame_rate_code];

    ALOGD_IF(bLog, "width=%u height=%u bitrate=%u framerate=%u", width, height, bitrate, framerate);
    return true;
 }


bool BOMX_MPEG2_ParseSeqDispExt(const uint8_t *pData, size_t length, BOMX_MPEG2_SeqInfo *pInfo, bool bLog)
{
    batom_bitstream bitstream, *bs=&bitstream;
    batom_cursor curs, *cursor=&curs;
    batom_vec vec;
    bool conformance_window_flag;
    unsigned i, sps_max_sub_layers_minus1, chroma_format_idc, log2_max_pic_order_cnt_lsb_minus4, width, height;
    int val, sps_video_parameter_set_id, bit_depth_luma, bit_depth_chroma;

    // Code borrowed partially from b_mpeg_video_probe_feed() in bmpeg_video_probe.c
    uint32_t scode;
    bool valid, seqExtOrUsr;
    size_t pos = 0;
    unsigned prevScode = 0;

    batom_vec_init(&vec, pData, length);
    batom_cursor_from_vec(cursor, &vec, 1);

    for(;;) {
        uint32_t scode;
        bool valid;
        unsigned last_scode = prevScode;

        scode = bmedia_video_scan_scode(cursor, 0xFFFFFFFFul);
        pos = batom_cursor_pos(cursor);
        ALOGD_IF(bLog, "scode %#x at %zu", scode, pos);
        if(scode==0) {
            return false;
        }
        scode &= 0xFF;
        prevScode = scode;
        switch(scode) {
        case 0xB7:
            /* Reached the end of sequence */
            return false;
            break;
        case 0xB5:
            if(last_scode == 0xB3) {
                /* Parse the sequence extension */
                uint32_t tempHi = batom_cursor_uint32_be(cursor);
                uint16_t tempLo = batom_cursor_uint16_be(cursor);
                if(BATOM_IS_EOF(cursor)) {
                    return false;
                }

                uint8_t profile_and_level = B_GET_BITS(tempHi, 28, 20);
                uint8_t profile = B_GET_BITS(profile_and_level, 6, 4);
                uint8_t level = B_GET_BITS(profile_and_level, 3, 0);
                uint8_t progressive_sequence = B_GET_BIT(tempHi, 19);
                uint8_t low_delay = B_GET_BIT(tempLo, 7);
                unsigned bitrate = B_GET_BITS(tempHi, 12, 1)<<18;
                ALOGD_IF(bLog, "profile=%u level=%u progressive_sequence=%u low_delay=%u bitrate=%u", profile, level, progressive_sequence, low_delay, bitrate);
                seqExtOrUsr = true;
        }
            else if (seqExtOrUsr) {
                /* extension_and_user_data(0) per section 6.2.2 of ITU-T H.262 */
                uint8_t tempByte = batom_cursor_byte(cursor);
                if (BATOM_IS_EOF(cursor)) {
                    return false;
                }

                if ((tempByte & 0xF0) == 0x20) {
                    /* sequence_display_extension */
                    uint8_t video_format = (tempByte & 0x0F) >> 1;
                    uint8_t colour_description = B_GET_BIT(tempByte, 0);
                    ALOGD_IF(bLog, "video_format=%u colour_description=%u", video_format, colour_description);
                    if (colour_description) {
                        pInfo->colorPrimaries = batom_cursor_byte(cursor);
                        pInfo->transferChar = batom_cursor_byte(cursor);
                        pInfo->matrixCoeff = batom_cursor_byte(cursor);
                        ALOGD_IF(bLog, "colour_primaries=%u transfer_chr=%u matrix_coeff=%u", pInfo->colorPrimaries, pInfo->transferChar, pInfo->matrixCoeff);
                        return true;
                    }
                }
            }
            break;
        case 0xB3:
            valid = b_mpeg_video_parse_sequence(cursor, bLog);
            if(BATOM_IS_EOF(cursor)) {
                return false;
            }
            if(!valid) {
                return false;
            }
            seqExtOrUsr = false;
            break;
        case 0xB2:
            /* No need to update seqExtOrUsr because we are still in the extension_and_user_data while-loop */
            break;
        default:
            seqExtOrUsr = false;
            break;
        }
    }

    return false;
}
