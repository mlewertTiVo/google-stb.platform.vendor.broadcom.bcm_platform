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
#define LOG_TAG "bomx_avc_parser"

#include "bomx_avc_parser.h"
#include "biobits.h"
#include "bioatom.h"
#include "bmedia_probe_util.h"
#include <log/log.h>

bool BOMX_AVC_ParseVUI(const uint8_t *pData, size_t length, BOMX_AVC_VUIInfo *pInfo, bool bLog)
{
    const uint8_t *pSps;
    batom_bitstream bs;
    batom_cursor curs, *cursor=&curs;
    batom_vec vec;
    bmedia_vlc_decode vlc;
    unsigned profile_idc;
    unsigned chroma_format_idc = 1;
    int val, width, height, frame_mbs_only;

    batom_vec_init(&vec, pData, length);
    batom_cursor_from_vec(cursor, &vec, 1);
    batom_bitstream_init(&bs, cursor);

    val = batom_bitstream_bits(&bs,16);
    if ( val != 0 ) { goto err_sps;} // Not SPS
    val = batom_bitstream_bits(&bs,16);
    if ( val != 1 ) { goto err_sps;} // Not SPS
    val = batom_bitstream_bits(&bs,8); // nal_ref_idc and nal_unit_type
    if ( (val & 0x60) == 0 || (val & 0x1F) != 7 ) { goto err_sps;} // Not SPS

    pSps = pData + 5;

    // Code below borrowed partially b_avc_video_parse_sps() from bavc_video_probe.c

    /* ITU-T Rec. H.264 | ISO/IEC 14496-10: 2007, Table 7.3.2.1.1 Sequence parameter set data syntax */
    bmedia_vlc_decode_init(&vlc, pSps, length - 5);
    profile_idc = *pSps;
    ALOGD_IF(bLog, "profile_idc=%d", profile_idc);

    /* skip over profile_idc, constraint_set?_flag's, reserved_zero_4bits and level_idc */
    vlc.index = 3;
    vlc.bit = 7;

    val = bmedia_vlc_decode_read(&vlc); /* seq_parameter_set_id */
    if (val<0) { goto err_sps;}
    ALOGD_IF(bLog, "seq_parameter_set_id=%d", val);

    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
        profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86) {
        chroma_format_idc = bmedia_vlc_decode_read(&vlc); /* chroma_format_idc */
        if (chroma_format_idc == 3) {
            bmedia_vlc_decode_bit(&vlc);
        }
        bmedia_vlc_decode_skip(&vlc); /* bit_depth_luma_minus8 */
        bmedia_vlc_decode_skip(&vlc); /* bit_depth_chroma_minus8 */
        bmedia_vlc_decode_bit(&vlc);  /* qpprime_y_zero_transform_bypass_flag */
        val = bmedia_vlc_decode_bit(&vlc); /* seq_scaling_matrix_present_flag */
        if (val<0) { goto err_sps;}
        if (val) {
            unsigned listIdx,listSize,coeffIdx;
            int lastScale,nextScale;
            for (listIdx=0; listIdx<((chroma_format_idc!=3)?8:12); listIdx++) {
                val = bmedia_vlc_decode_bit(&vlc); /* seq_scaling_list_present_flag */
                if(val<0) { goto err_sps;}
                if (val) {
                    listSize = (listIdx<6)?16:64;
                    lastScale = nextScale = 8;
                    for (coeffIdx=0; coeffIdx<listSize; coeffIdx++) {
                        if (nextScale != 0) {
                            val = bmedia_vlc_decode_read(&vlc); /* delta_scale */
                            if(val<0) { goto err_sps;}
                            val = ((val%2)?1:-1) * ((val+1)>>1); /* convert val from ue(v) to se(v) */
                            nextScale = (lastScale + val + 256) % 256;
                        }
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }
    bmedia_vlc_decode_skip(&vlc);  /* log2_max_frame_num_minus4 */
    val = bmedia_vlc_decode_read(&vlc); /* pic_order_cnt_type */
    if(val<0) { goto err_sps;}
    if (val == 0) {
        bmedia_vlc_decode_skip(&vlc);  /* log2_max_pic_order_cnt_lsb_minus4 */
    } else if (val == 1) {
        int i;
        bmedia_vlc_decode_bit(&vlc); /* delta_pic_order_always_zero_flag */
        bmedia_vlc_decode_skip(&vlc); /* offset_for_non_ref_pic */
        bmedia_vlc_decode_skip(&vlc); /* offset_for_top_to_bottom_field */
        val = bmedia_vlc_decode_read(&vlc); /* num_ref_frames_in_pic_order_cnt_cycle */
        if(val<0) { goto err_sps;}
        for (i = 0; i < val; i++) {
            int is_eof = bmedia_vlc_decode_skip(&vlc); /* offset_for_ref_frame[i] */
            if(is_eof) { goto err_sps;}
        }
    }

    bmedia_vlc_decode_skip(&vlc);  /* num_ref_frames */
    bmedia_vlc_decode_bit(&vlc); /* gaps_in_frame_num_value_allowed_flag */
    val = bmedia_vlc_decode_read(&vlc); /* pic_width_in_mbs_minus1 */
    if(val<0) { goto err_sps;}
    ALOGD_IF(bLog, "pic_width_in_mbs_minus1=%u", val);
    width = (val+1) * 16;
    val = bmedia_vlc_decode_read(&vlc); /* pic_height_in_map_units_minus1 */
    if(val<0) { goto err_sps;}
    ALOGD_IF(bLog, "pic_height_in_map_units_minus1=%u", val);

    frame_mbs_only = bmedia_vlc_decode_bit(&vlc); /* frame_mbs_only_flag */
    if(frame_mbs_only<0) { goto err_sps;}

    ALOGD_IF(bLog, "frame_mbs_only_flag=%u", frame_mbs_only);
    height = 16 * (2 - frame_mbs_only) * (val+1);
    if (!frame_mbs_only) {
        bmedia_vlc_decode_bit(&vlc); /* mb_adaptive_frame_field_flag */
    }
    bmedia_vlc_decode_bit(&vlc); /* direct_8x8_inference_flag */
    val = bmedia_vlc_decode_bit(&vlc); /* frame_cropping_flag */
    if(val<0) { goto err_sps;}
    if (val) {
        unsigned CropUnitX, CropUnitY;
        unsigned right_offset, left_offset;
        unsigned bottom_offset, top_offset;

        if (chroma_format_idc==1) {
          CropUnitX = 2;
          CropUnitY = 2*(2-frame_mbs_only);
        } else if (chroma_format_idc==2) {
          CropUnitX = 2;
          CropUnitY = (2-frame_mbs_only);
        }
        else {
          CropUnitX = 1;
          CropUnitY = (2-frame_mbs_only);
        }

        left_offset   = bmedia_vlc_decode_read(&vlc); /* frame_crop_left_offset */
        right_offset  = bmedia_vlc_decode_read(&vlc); /* frame_crop_right_offset */
        top_offset    = bmedia_vlc_decode_read(&vlc); /* frame_crop_top_offset */
        bottom_offset = bmedia_vlc_decode_read(&vlc); /* frame_crop_bottom_offset */
        width  -= CropUnitX * (right_offset  + left_offset);
        height -= CropUnitY * (bottom_offset + top_offset );
    }

    ALOGD_IF(bLog, "final picture dimensions=%ux%u", width, height);

    /* The following codes are added to parse VUI parameters */
    val = bmedia_vlc_decode_bit(&vlc); /* vui_parameters_present_flag */
    if(val<=0) { goto err_sps;}

    val = bmedia_vlc_decode_bit(&vlc); /* aspect_ratio_info_present_flag */
    if(val<0) { goto err_sps;}
    if (val) {
        val = bmedia_vlc_decode_bits(&vlc, 8); /* aspect_ratio_idc */
        ALOGD_IF(bLog, "aspect_ratio_idc=%u", val);
        if (val == 255) { /* Extended_SAR */
            bmedia_vlc_decode_bits(&vlc, 16); /* sar_width */
            bmedia_vlc_decode_bits(&vlc, 16); /* sar_height */
        }
    }

    val = bmedia_vlc_decode_bit(&vlc); /* overscan_info_present_flag */
    if (val) {
        val = bmedia_vlc_decode_bit(&vlc); /* overscan_appropriate_flag */
    }

    val = bmedia_vlc_decode_bit(&vlc); /* video_signal_type_present_flag */
    if (val) {
        unsigned videoFormat;

        videoFormat = bmedia_vlc_decode_bits(&vlc, 3); /* video_format */
        pInfo->bFullRange = (bmedia_vlc_decode_bit(&vlc) != 0); /* video_full_range_flag */
        val = bmedia_vlc_decode_bit(&vlc); /* colour_description_present_flag */
        if (val) {
            pInfo->colorPrimaries = bmedia_vlc_decode_bits(&vlc, 8); /* colour_primaries */
            pInfo->transferChar = bmedia_vlc_decode_bits(&vlc, 8); /* transfer_characteristics */
            pInfo->matrixCoeff = bmedia_vlc_decode_bits(&vlc, 8); /* matrix_coefficients */
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

err_sps:
    return false;
}
