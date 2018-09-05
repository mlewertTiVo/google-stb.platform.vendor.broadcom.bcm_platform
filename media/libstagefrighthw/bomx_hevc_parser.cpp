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
#define LOG_TAG "bomx_hevc_parser"

#include "bomx_hevc_parser.h"
#include "biobits.h"
#include "bioatom.h"
#include "bmedia_probe_util.h"
#include "bmedia_util.h"
#include <log/log.h>
#include <sys/param.h>

#define B_H265_PARSER_READ_BIT(name, value) value = batom_bitstream_bit(bs); if (value<0) return false

#define B_H265_UNUSED_FIELD(name, bits) batom_bitstream_drop_bits(bs, bits)
#define B_H265_UNUSED_VLC(name) batom_bitstream_exp_golomb(bs)

/* 7.3.3 Profile, tier and level syntax */
static bool BOMX_HEVC_ParseProfileTierLevel(batom_bitstream *bs, unsigned sps_max_sub_layers_minus1)
{
    unsigned i;
    unsigned general_profile_space;
    bool sub_layer_profile_present_flag[8];
    bool sub_layer_level_present_flag[8];

    general_profile_space = batom_bitstream_bits(bs, 2);
    if ( batom_bitstream_eof(bs) || general_profile_space != 0 ) {
        return false;
    }

    /* Skip the rest of general profile (94 bits) */
    B_H265_UNUSED_FIELD(general_profile_*, 31);
    B_H265_UNUSED_FIELD(general_profile_*, 31);
    B_H265_UNUSED_FIELD(general_profile_*, 31);
    B_H265_UNUSED_FIELD(general_profile_*, 1);

    for ( i = 0; i < sps_max_sub_layers_minus1; i++ ) {
        sub_layer_profile_present_flag[i] = batom_bitstream_bit(bs);
        sub_layer_level_present_flag[i] = batom_bitstream_bit(bs);
    }
    if ( sps_max_sub_layers_minus1 > 0 ) {
        for ( i = sps_max_sub_layers_minus1; i < 8; i++ ) {
            unsigned reserved_zero_2bits = batom_bitstream_bits(bs,2);
            if(batom_bitstream_eof(bs) || reserved_zero_2bits != 0) {
                return false;
            }
        }
    }
    for( i = 0; i < sps_max_sub_layers_minus1; i++ ) {
        if ( sub_layer_profile_present_flag[i] ) {
            /* Skip profile (88 bits) */
            B_H265_UNUSED_FIELD(sub_layer_profile_*[i], 31);
            B_H265_UNUSED_FIELD(sub_layer_profile_*[i], 31);
            B_H265_UNUSED_FIELD(sub_layer_profile_*[i], 26);
        }
        if ( sub_layer_level_present_flag[i] ) {
            /* Skip level (8 bits) */
            B_H265_UNUSED_FIELD(sub_layer_level_idc[i], 8);
        }
    }

    if(batom_bitstream_eof(bs)) {
        return false;
    }
    return true;
}

/* 7.3.4 Scaling list data syntax */
static bool BOMX_HEVC_ParseScalingListData(batom_bitstream *bs)
{
    int val;

    for (uint32_t sizeId = 0; sizeId < 4; ++sizeId) {
        for (uint32_t matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1) {
            B_H265_PARSER_READ_BIT(scaling_list_pred_mode_flag, val);
            if ( val == 0 ) {
                B_H265_UNUSED_VLC(scaling_list_pred_matrix_id_delta);
            }
            else {
                uint32_t coefNum = MIN(64, (1 << (4 + (sizeId << 1))));
                if (sizeId > 1) {
                    B_H265_UNUSED_VLC(scaling_list_dc_coef_minus8);
                }
                for (uint32_t i = 0; i < coefNum; ++i) {
                    B_H265_UNUSED_VLC(scaling_list_delta_coef);
                }
            }
        }
    }

    return true;
}

/* 7.3.7 Short-term reference picture set syntax */
static bool BOMX_HEVC_ParseShortTermRefPicSets(batom_bitstream *bs, unsigned numShortTermRefPicSets)
{
    int val;
    uint32_t numPics = 0;

    for (uint32_t i = 0; i < numShortTermRefPicSets; ++i) {
        val = 0;
        if (i != 0) {
            B_H265_PARSER_READ_BIT(inter_ref_pic_set_prediction_flag, val);
        }

        if (val) {
            if (i == numShortTermRefPicSets) {
                B_H265_UNUSED_VLC(delta_idx_minus1);
            }
            B_H265_UNUSED_FIELD(delta_rps_sign, 1);
            B_H265_UNUSED_VLC(abs_delta_rps_minus1);
            uint32_t nextNumPics = 0;
            for (uint32_t j = 0; j <= numPics; ++j) {
                B_H265_PARSER_READ_BIT(used_by_curr_pic_flag, val);
                if (val) {
                    ++nextNumPics;
                }
                else {
                    B_H265_PARSER_READ_BIT(use_delta_flag, val);
                    if (val) {
                        ++nextNumPics;
                    }
                }
            }
            numPics = nextNumPics;
        } else {
            uint32_t num_negative_pics = batom_bitstream_exp_golomb(bs);
            uint32_t num_positive_pics = batom_bitstream_exp_golomb(bs);
            if (num_negative_pics > UINT32_MAX - num_positive_pics) return false;
            numPics = num_negative_pics + num_positive_pics;
            for (uint32_t j = 0; j < numPics; ++j) {
                B_H265_UNUSED_VLC(delta_poc_s0|1_minus1);
                B_H265_UNUSED_FIELD(used_by_curr_pic_s0|1_flag, 1);
            }
        }
    }

    return true;
}

bool BOMX_HEVC_ParseVUI(const uint8_t *pData, size_t length, BOMX_HEVC_VUIInfo *pInfo, bool bLog)
{
    const uint8_t *pSps = NULL, *pCur = pData;
    uint8_t emulTrimBuf[256];
    size_t curLen = length, emulTrimBufLen;
    batom_bitstream bitstream, *bs=&bitstream;
    batom_cursor curs, *cursor=&curs, fullCurs, *fullCursor=&fullCurs;
    batom_vec fullVec, vec;
    bool conformance_window_flag;
    unsigned i, sps_max_sub_layers_minus1, chroma_format_idc, log2_max_pic_order_cnt_lsb_minus4, width, height;
    int val, sps_video_parameter_set_id, bit_depth_luma, bit_depth_chroma;

    while ( pCur != NULL )
    {
        // Try both 0x42 and 0x43 as the nal_unit_type byte overlaps the MSB of nuh_layer_id
        pSps = (const uint8_t *)BKNI_Memchr(pCur, 66, curLen);
        if ( pSps == NULL )
        {
            pSps = (const uint8_t *)BKNI_Memchr(pCur, 67, curLen);
        }

        if ( pSps != NULL )
        {
            size_t pos = (pSps - pData);
            ALOG_ASSERT(pos < length);
            curLen = length - pos;

            if ( pos >= 4 && *(pSps-4) == 0 && *(pSps-3) == 0 && *(pSps-2) == 0 && *(pSps-1) == 1 )
            {
                break;
            }
        }

        pCur = pSps;
    }

    if ( pSps == NULL ) return false; // SPS not found

    pSps += 2; /* Skip nuh_layer_id and nuh_temporal_id_plus1 */
    curLen -= 2;

    batom_vec_init(&fullVec, pSps, curLen);
    batom_cursor_from_vec(fullCursor, &fullVec, 1);

    // Trim emulation prevention byte from bitstream
    BATOM_CLONE(cursor, fullCursor);
    emulTrimBufLen = bmedia_strip_emulation_prevention(cursor, emulTrimBuf, sizeof(emulTrimBuf));

    batom_vec_init(&vec, emulTrimBuf, emulTrimBufLen);
    batom_cursor_from_vec(cursor, &vec, 1);
    batom_bitstream_init(bs, cursor);

    // Code below borrowed partially from b_h265_seq_parameter_set_rbsp() in bmedia_probe_util.c but updated to parse Rec. ITU-T H.265 (02/2018)

    /* 7.3.2.2 Sequence parameter set RBSP syntax */
    sps_video_parameter_set_id = batom_bitstream_bits(bs, 4);
    sps_max_sub_layers_minus1 = batom_bitstream_bits(bs, 3);
    B_H265_UNUSED_FIELD(sps_temporal_id_nesting_flag, 1);

    if(!BOMX_HEVC_ParseProfileTierLevel(bs, sps_max_sub_layers_minus1)) {
        return false;
    }

    B_H265_UNUSED_VLC(sps_seq_parameter_set_id);
    chroma_format_idc = batom_bitstream_exp_golomb(bs);
    if(batom_bitstream_eof(bs)) {
        return false;
    }
    if(chroma_format_idc == 3) {
        B_H265_UNUSED_FIELD(separate_colour_plane_flag, 1);
    }
    width = batom_bitstream_exp_golomb(bs); /* pic_width_in_luma_samples */
    height = batom_bitstream_exp_golomb(bs); /* pic_height_in_luma_samples */
    conformance_window_flag = batom_bitstream_bit(bs);
    if(batom_bitstream_eof(bs)) {
        return false;
    }
    ALOGD_IF(bLog, "width=%u height=%u", width, height);
    if(conformance_window_flag) {
        B_H265_UNUSED_VLC(conf_win_left_offset);
        B_H265_UNUSED_VLC(conf_win_right_offset);
        B_H265_UNUSED_VLC(conf_win_top_offset);
        B_H265_UNUSED_VLC(conf_win_bottom_offset);
    }
    bit_depth_luma = 8 + batom_bitstream_exp_golomb(bs);
    bit_depth_chroma = 8 + batom_bitstream_exp_golomb(bs);
    ALOGD_IF(bLog, "bit_depth_luma=%d bit_depth_chroma=%d", bit_depth_luma, bit_depth_chroma);
    if(batom_bitstream_eof(bs)) {
        return false;
    }

    /* The following codes are added to parse VUI parameters */

    log2_max_pic_order_cnt_lsb_minus4 = batom_bitstream_exp_golomb(bs);
    B_H265_PARSER_READ_BIT(sps_sub_layer_ordering_info_present_flag, val);
    i = (val != 0) ? 0 : sps_max_sub_layers_minus1;
    for ( ; i <= sps_max_sub_layers_minus1; i++ ) {
        B_H265_UNUSED_VLC(sps_max_dec_pic_buffering_minus1);
        B_H265_UNUSED_VLC(sps_max_num_reorder_pics);
        B_H265_UNUSED_VLC(sps_max_latency_increase_plus1);
    }

    B_H265_UNUSED_VLC(log2_min_luma_coding_block_size_minus3);
    B_H265_UNUSED_VLC(log2_diff_max_min_luma_coding_block_size);
    B_H265_UNUSED_VLC(log2_min_luma_transform_block_size_minus2);
    B_H265_UNUSED_VLC(log2_diff_max_min_luma_transform_block_size);
    B_H265_UNUSED_VLC(max_transform_hierarchy_depth_inter);
    B_H265_UNUSED_VLC(max_transform_hierarchy_depth_intra);
    B_H265_PARSER_READ_BIT(scaling_list_enabled_flag, val);
    if ( val ) {
        B_H265_PARSER_READ_BIT(sps_scaling_list_data_present_flag, val);
        if ( val ) {
            if ( !BOMX_HEVC_ParseScalingListData(bs) ) {
                return false;
            }
        }
    }

    B_H265_UNUSED_FIELD(amp_enabled_flag, 1);
    B_H265_UNUSED_FIELD(sample_adaptive_offset_enabled_flag, 1);
    B_H265_PARSER_READ_BIT(pcm_enabled_flag, val);
    if ( val ) {
        B_H265_UNUSED_FIELD(pcm_sample_bit_depth_luma_minus1, 4);
        B_H265_UNUSED_FIELD(pcm_sample_bit_depth_chroma_minus1, 4);
        B_H265_UNUSED_VLC(log2_min_pcm_luma_coding_block_size_minus3);
        B_H265_UNUSED_VLC(log2_diff_max_min_pcm_luma_coding_block_size);
        B_H265_UNUSED_FIELD(pcm_loop_filter_disabled_flag, 1);
    }

    val = batom_bitstream_exp_golomb(bs); /* num_short_term_ref_pic_sets */
    if ( val > 0 ) {
        if ( !BOMX_HEVC_ParseShortTermRefPicSets(bs, val) ) {
            return false;
        }
    }

    B_H265_PARSER_READ_BIT(long_term_ref_pics_present_flag, val);
    if ( val ) {
        val = batom_bitstream_exp_golomb(bs); /* num_long_term_ref_pics_sps */
        for ( int j = 0; j < val; ++j ) {
            B_H265_UNUSED_FIELD(lt_ref_pic_poc_lsb_sps, log2_max_pic_order_cnt_lsb_minus4 + 4);
            B_H265_UNUSED_FIELD(used_by_curr_pic_lt_sps_flag, 1);
        }
    }

    B_H265_UNUSED_FIELD(sps_temporal_mvp_enabled_flag, 1);
    B_H265_UNUSED_FIELD(strong_intra_smoothing_enabled_flag, 1);
    val = batom_bitstream_bit(bs); /* vui_parameters_present_flag */
    if ( val <= 0 ) return false;

    B_H265_PARSER_READ_BIT(aspect_ratio_info_present_flag, val);
    if (val) {
        val = batom_bitstream_bits(bs, 8); /* aspect_ratio_idc */
        ALOGD_IF(bLog, "aspect_ratio_idc=%u", val);
        if (val == 255) { /* Extended_SAR */
            B_H265_UNUSED_FIELD(sar_width, 16);
            B_H265_UNUSED_FIELD(sar_height, 16);
        }
    }

    B_H265_PARSER_READ_BIT(overscan_info_present_flag, val);
    if (val) {
        B_H265_PARSER_READ_BIT(overscan_appropriate_flag, val);
    }

    B_H265_PARSER_READ_BIT(video_signal_type_present_flag, val);
    if (val) {
        unsigned videoFormat;

        videoFormat = batom_bitstream_bits(bs, 3); /* video_format */
        pInfo->bFullRange = (batom_bitstream_bit(bs) != 0); /* video_full_range_flag */
        B_H265_PARSER_READ_BIT(colour_description_present_flag, val);
        if (val) {
            pInfo->colorPrimaries = batom_bitstream_bits(bs, 8); /* colour_primaries */
            pInfo->transferChar = batom_bitstream_bits(bs, 8); /* transfer_characteristics */
            pInfo->matrixCoeff = batom_bitstream_bits(bs, 8); /* matrix_coefficients */
        }
        else {
            /* Mark as unspecified */
            pInfo->colorPrimaries = 2;
            pInfo->transferChar = 2;
            pInfo->matrixCoeff = 2;
        }
        ALOGD_IF(bLog, "video_format=%u video_full_range_flag=%d", videoFormat, pInfo->bFullRange);
        ALOGD_IF(bLog, "colour_primaries=%u transfer_chr=%u matrix_coeff=%u", pInfo->colorPrimaries, pInfo->transferChar, pInfo->matrixCoeff);

        return true;
    }

    return false;
}
