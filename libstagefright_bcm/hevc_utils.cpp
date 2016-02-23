/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "bcm_hevc_utils"
#include <utils/Log.h>

#include "include/avc_utils.h"
#include "include/hevc_utils.h"

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MAX_SUB_LAYERS                  7      /* H.265 SPS Spec 7.4.3.2 */
#define MAX_SHORT_TERM_RPS_COUNT        64     /* H.265 SPS Spec 7.4.3.2 */
#define MAX_SPATIAL_SEGMENTATION        4096   /* H.265 SPS Spec E.3 */

namespace android {

/* This function is duplicated from frameworks/av/media/libstagefright/avc_utils.cpp */
signed parseSE(ABitReader *br) {
    unsigned codeNum = parseUE(br);

    return (codeNum & 1) ? (codeNum + 1) / 2 : -(codeNum / 2);
}


void ExtractHEVCRbsp(uint8_t *dst, uint8_t *src, int length)
{
    int i, j;
    i = j = 0;

    while (i < (length -2)) {
        if (src[i] == 0 && src[i+1] == 0) {
            if (src[i+2] == 3) {
                dst[j++] = 0;
                dst[j++] = 0;
                i += 3;
            }
            else {
                dst[j++] = src[i++];
            }
        }
        else {
             dst[j++] = src[i++];
        }
    }

    while (i < length) {
        dst[j++] = src[i++];
    }
}

void SkipScalingListData(ABitReader *br) {
    int i, j, k, num_coeffs;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < (i == 3 ? 2 : 6); j++) {
            if (!br->getBits(1)) {
                parseUE(br);
            } else {
                num_coeffs = MIN (64, 1 << (4 + (i << 1)));
                if (i > 1)
                    parseSE(br);
                for (k = 0; k < num_coeffs; k++)
                    parseSE(br);
            }
        }
    }
}

void SkipHRDParameters(ABitReader *br, uint8_t cprms_present_flag, unsigned int max_sub_layers_minus1) {
    unsigned int i;

    uint8_t sub_pic_hrd_params_present_flag = 0;
    uint8_t nal_hrd_parameters_present_flag = 0;
    uint8_t vcl_hrd_parameters_present_flag = 0;

    if (cprms_present_flag) {
        nal_hrd_parameters_present_flag = br->getBits(1);
        vcl_hrd_parameters_present_flag = br->getBits(1);
        if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
            sub_pic_hrd_params_present_flag = br->getBits(1);
            if (sub_pic_hrd_params_present_flag) {
                /*
                * tick_divisor_minus2                          u(8)
                * du_cpb_removal_delay_increment_length_minus1 u(5)
                * sub_pic_cpb_params_in_pic_timing_sei_flag    u(1)
                * dpb_output_delay_du_length_minus1            u(5)
                */
                br->skipBits(19);
            }

            br->skipBits(8); // bit_rate_scale u(4), cpb_size_scale u(4)

            if (sub_pic_hrd_params_present_flag) {
                br->skipBits(4); // cpb_size_du_scale
            }
            /*
            * initial_cpb_removal_delay_length_minus1 u(5)
            * au_cpb_removal_delay_length_minus1      u(5)
            * dpb_output_delay_length_minus1          u(5)
            */
            br->skipBits(15);
        }
    }

    for (i = 0; i <= max_sub_layers_minus1; i++) {
        unsigned int j;
        unsigned int cpb_cnt_minus1 = 0;
        uint8_t low_delay_hrd_flag = 0;
        uint8_t fixed_pic_rate_within_cvs_flag = 0;
        uint8_t fixed_pic_rate_general_flag = br->getBits(1);

        if (!fixed_pic_rate_general_flag) {
            fixed_pic_rate_within_cvs_flag = br->getBits(1);
        }

        if (fixed_pic_rate_within_cvs_flag) {
            parseUE(br); // elemental_duration_in_tc_minus1
        } else {
            low_delay_hrd_flag = br->getBits(1);
        }

        if (!low_delay_hrd_flag) {
            cpb_cnt_minus1 = parseUE(br);
        }

        if (nal_hrd_parameters_present_flag) {
            for (j = 0; j <= cpb_cnt_minus1; j++) {
                parseUE(br); // bit_rate_value_minus1
                parseUE(br); // cpb_size_value_minus1

                if (sub_pic_hrd_params_present_flag) {
                    parseUE(br); // cpb_size_du_value_minus1
                    parseUE(br); // bit_rate_du_value_minus1
                }
                br->skipBits(1); // cbr_flag
            }
        }

        if (vcl_hrd_parameters_present_flag) {
            for (j = 0; j <= cpb_cnt_minus1; j++) {
                parseUE(br); // bit_rate_value_minus1
                parseUE(br); // cpb_size_value_minus1

                if (sub_pic_hrd_params_present_flag) {
                    parseUE(br); // cpb_size_du_value_minus1
                    parseUE(br); // bit_rate_du_value_minus1
                }
                br->skipBits(1); // cbr_flag
            }
        }
    }
}

static void ParseGeneralInfo(ABitReader *br, struct hevc_nal_info *info, unsigned int max_sub_layers_minus1) {

    unsigned int i;
    uint8_t sub_layer_profile_present_flag[MAX_SUB_LAYERS];
    uint8_t sub_layer_level_present_flag[MAX_SUB_LAYERS];

    uint8_t profile_space = br->getBits(2);
    uint8_t tier_flag = br->getBits(1);
    uint8_t profile_idc =  br->getBits(5);
    uint32_t profile_compatibility_flags = br->getBits(32);
    uint64_t constraint_indicator_flags = br->getBits(32) << 16 | ( br->getBits(16) & 0xFFFF );
    uint8_t level_idc = br->getBits(8);

    info->general_profile_space = profile_space;

    if (info->general_tier_flag < tier_flag) {
        info->general_level_idc = level_idc;
    } else {
        info->general_level_idc = MAX(info->general_level_idc, level_idc);
    }

    info->general_tier_flag = MAX(info->general_tier_flag, tier_flag);
    info->general_profile_idc = MAX(info->general_profile_idc, profile_idc);
    info->general_profile_compatibility_flags &= profile_compatibility_flags;
    info->general_constraint_indicator_flags &= constraint_indicator_flags;

    for (i = 0; i < max_sub_layers_minus1; i++) {
        sub_layer_profile_present_flag[i] = br->getBits(1);
        sub_layer_level_present_flag[i] = br->getBits(1);
    }

    if (max_sub_layers_minus1 > 0) {
        for (i = max_sub_layers_minus1; i < 8; i++) {
            br->skipBits(2); // reserved_zero_2bits[i]
        }
    }

    for (i=0; i < max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            /* sub_layer_profile_space   u(2)
             * sub_layer_tier_flag       u(1)
             * sub_layer_profile_idc     u(5)
             * sub_layer_profile_compatibility_flag u(32)
             * sub_layer_progressive_source_flag    u(1)
             * sub_layer_interlaced_source_flag     u(1)
             * sub_layer_non_packed_constraint_flag u(1)
             * sub_layer_frame_only_constraint_flag u(1)
             * sub_layer_reserved_zero_44bits       u(44)
             */
            br->skipBits(8);
            br->skipBits(32);
            br->skipBits(4);
            br->skipBits(44);
        }
        if (sub_layer_level_present_flag[i]) {
            /* sub_layer_level_idc u(8) */
            br->skipBits(8);
        }
    }

    return;
}

static int ParseSPSRps(ABitReader *br, unsigned int rps_idx, unsigned int num_rps,
                         unsigned int num_delta_pocs[MAX_SHORT_TERM_RPS_COUNT+1]) {

    unsigned int i, delta_rps_sign, abs_delta_rps_minus1, delta_idx_minus1;
    unsigned int inter_ref_pic_set_prediction_flag = 0;

    if (rps_idx) {
        inter_ref_pic_set_prediction_flag = br->getBits(1);
    }

    if (inter_ref_pic_set_prediction_flag) { // inter_ref_pic_set_prediction_flag

        if (rps_idx >= num_rps)
            return 1;

        delta_rps_sign = br->getBits(1); //delta_rps_sign
        abs_delta_rps_minus1 = parseUE(br); // abs_delta_rps_minus1

        num_delta_pocs[rps_idx] = 0;

        for (i = 0; i < num_delta_pocs[rps_idx - 1 ]; i++) {
            uint8_t use_delta_flag = 0;
            uint8_t used_by_curr_pic_flag = br->getBits(1);

            if (!used_by_curr_pic_flag) {
                use_delta_flag = br->getBits(1);
            }

            if (used_by_curr_pic_flag || use_delta_flag) {
                num_delta_pocs[rps_idx]++;
            }
        }
    } else {
        unsigned int num_negative_pics = parseUE(br);
        unsigned int num_positive_pics = parseUE(br);

        num_delta_pocs[rps_idx] = num_negative_pics + num_positive_pics;

        for (i = 0; i < num_negative_pics; i++) {
            parseUE(br); // delta_poc_s0_minus1[rps_idx]
            br->getBits(1); // used_by_curr_pic_s0_flag[rps_idx]
        }

        for (i = 0; i < num_positive_pics; i++) {
            parseUE(br); // delta_poc_s1_minus1[rps_idx]
            br->getBits(1); // used_by_curr_pic_s1_flag[rps_idx]
        }
    }

    return 0;
}

void ParseHEVCSPS(const sp<ABuffer> &seqParamSet, struct hevc_nal_info *info) {

    sp<ABuffer> cleanSpsBuf = new ABuffer(seqParamSet->size() - 2);

    ExtractHEVCRbsp(cleanSpsBuf->data(), seqParamSet->data() + 2, cleanSpsBuf->size());
    ABitReader br(cleanSpsBuf->data(), cleanSpsBuf->size());

    unsigned int i, min_spatial_segmentation_idc, num_delta_pocs[MAX_SHORT_TERM_RPS_COUNT+1];

    /* 7.3.2.2 Sequence parameter set RBSP syntax */
    br.skipBits(4);  // sps_video_parameter_set_id
    unsigned int sps_max_sub_layers_minus1 = br.getBits(3);

    info->num_temporal_layers = MAX(info->num_temporal_layers, sps_max_sub_layers_minus1 + 1);

    info->temporal_id_nesting_flag = br.getBits(1);

    if (sps_max_sub_layers_minus1 + 1 > MAX_SUB_LAYERS) {
        ALOGE("sps_max_sub_layers_minus1 %u not supported", sps_max_sub_layers_minus1);
        return;
    }

    ParseGeneralInfo(&br, info, sps_max_sub_layers_minus1);

    parseUE(&br); // sps_seq_parameter_set_id

    info->chroma_format = parseUE(&br);

    if (info->chroma_format == 3) {
        br.skipBits(1); //separate_colour_plane_flag
    }

    info->pic_width_in_luma_samples = parseUE(&br);
    info->pic_height_in_luma_samples = parseUE(&br);

    if (br.getBits(1)) { // conformance_window_flag
        parseUE(&br); // conf_win_left_offset
        parseUE(&br); // conf_win_right_offset
        parseUE(&br); // conf_win_top_offset
        parseUE(&br); // conf_win_bottom_offset
    }

    info->bit_depth_luma_minus8 = parseUE(&br);
    info->bit_depth_chroma_minus8 = parseUE(&br);

    parseUE(&br); //log2_max_pic_order_cnt_lsb_minus4

    if (br.getBits(1)) { // sps_sub_layer_ordering_info_present_flag
        for (i = 0; i <= sps_max_sub_layers_minus1; i++) {
            parseUE(&br); // max_dec_pic_buffering_minus1
            parseUE(&br); // max_num_reorder_pics
            parseUE(&br); // max_latency_increase_plus1
        }
    } else {
        for (i = sps_max_sub_layers_minus1; i <= sps_max_sub_layers_minus1; i++) {
            parseUE(&br); // max_dec_pic_buffering_minus1
            parseUE(&br); // max_num_reorder_pics
            parseUE(&br); // max_latency_increase_plus1
        }
    }

    parseUE(&br); // log2_min_luma_coding_block_size_minus3
    parseUE(&br); // log2_diff_max_min_luma_coding_block_size
    parseUE(&br); // log2_min_transform_block_size_minus2
    parseUE(&br); // log2_diff_max_min_transform_block_size
    parseUE(&br); // max_transform_hierarchy_depth_inter
    parseUE(&br); // max_transform_hierarchy_depth_intra

    unsigned int scaling_list_enabled_flag = br.getBits(1);
    if (scaling_list_enabled_flag) {
        if (br.getBits(1)) { // sps_scaling_list_data_present_flag
            SkipScalingListData(&br);
        }
    }

    br.skipBits(1); // amp_enabled_flag
    br.skipBits(1); // sample_adaptive_offset_enabled_flag

    if (br.getBits(1)) { // pcm_enabled_flag
        br.skipBits(4); // pcm_sample_bit_depth_luma_minus1
        br.skipBits(4); // pcm_sample_bit_depth_chroma_minus1
        parseUE(&br); // log2_min_pcm_luma_coding_block_size_minus3
        parseUE(&br); // log2_diff_max_min_pcm_luma_coding_block_size
        br.skipBits(1); // pcm_loop_filter_disabled_flag
    }

    unsigned int num_short_term_ref_pic_sets = parseUE(&br);
    if (num_short_term_ref_pic_sets > MAX_SHORT_TERM_RPS_COUNT) {
        ALOGE("Invalid Data from SPS, num_short_term_ref_pic_sets: %u", num_short_term_ref_pic_sets);
        return;
    }

    for (i = 0 ; i < num_short_term_ref_pic_sets; i++ ) {
        if (ParseSPSRps(&br, i, num_short_term_ref_pic_sets, num_delta_pocs)) {
            ALOGE("Parsing HEVC SPS RPS error");
            return;
        }
    }

    if (br.getBits(1)) { // long_term_ref_pics_present_flag
        for (i = 0; i < parseUE(&br); i ++) { // num_long_term_ref_pics_sps
            parseUE(&br); // lt_ref_pic_poc_lsb_sps[i]
            br.skipBits(1); // used_by_curr_pic_lt_sps_flag[i]
        }
    }

    /* sps_temporal_mvp_enabled_flag u(1)
     * strong_intra_smoothing_enabled_flag u(1) */
    br.skipBits(2);

    if (br.getBits(1)) { // vui_parameters_present_flag
        if (br.getBits(1)) { // aspect_ratio_info_present_flag
            if (br.getBits(8) == 255) // aspect_ratio_idc
                br.skipBits(32); // sar_width u(16), sar_height u(16)
        }

        if (br.getBits(1)) { // overscan_info_present_flag
            br.skipBits(1); // overscan_appropriate_flag
        }

        if (br.getBits(1)) { // video_signal_type_present_flag
            br.skipBits(4); // video_format u(3), video_full_range_flag u(1)

            if (br.getBits(1)) { // colour_description_present_flag
                /* colour_primaries u(8)
                 * transfer_characteristics u(8)
                 * matrix_coeffs u(8) */
                br.skipBits(24);
            }
        }

        if (br.getBits(1)) { // chroma_loc_info_present_flag
            parseUE(&br); // chroma_sample_loc_type_top_field
            parseUE(&br); // chroma_sample_loc_type_bottom_field
        }

        /* neutral_chroma_indication_flag u(1)
         * field_seq_flag                 u(1)
         * frame_field_info_present_flag  u(1) */
        br.skipBits(3);

        if (br.getBits(1)) { // default_display_window_flag
            parseUE(&br); // def_disp_win_left_offset
            parseUE(&br); // def_disp_win_right_offset
            parseUE(&br); // def_disp_win_top_offset
            parseUE(&br); // def_disp_win_bottom_offset
        }

        if (br.getBits(1)) { // vui_timing_info_present_flag

            br.skipBits(32); // num_units_in_tick
            br.skipBits(32); // time_scale

            if (br.getBits(1)) { // poc_proportional_to_timing_flag
                parseUE(&br); // num_ticks_poc_diff_one_minus1
            }

            if (br.getBits(1)) { // vui_hrd_parameters_present_flag
                SkipHRDParameters(&br, 1, sps_max_sub_layers_minus1);
            }
        }

        if (br.getBits(1)) { // bitstream_restriction_flag
            /* tiles_fixed_structure_flag u(1)
             * motion_vectors_over_pic_boundaries_flag u(1)
             * restricted_ref_pic_lists_flag u(1) */
            br.skipBits(3);

            min_spatial_segmentation_idc = parseUE(&br); // min_spatial_segmentation_idc

            info->min_spatial_segmentation_idc = MIN(info->min_spatial_segmentation_idc,
                                                     min_spatial_segmentation_idc);

            parseUE(&br); // max_bytes_per_pic_denom
            parseUE(&br); // max_bits_per_min_cu_denom
            parseUE(&br); // log2_max_mv_length_horizontal
            parseUE(&br); // log2_max_mv_length_vertical
        }
    }

    // the left SPS parsing is not finished yet, and not needed right now
    return;
}

void ParseHEVCVPS(const sp<ABuffer> &videoParamSet, struct hevc_nal_info *info) {
    sp<ABuffer> cleanSpsBuf = new ABuffer(videoParamSet->size() - 2);

    ExtractHEVCRbsp(cleanSpsBuf->data(), videoParamSet->data() + 2, cleanSpsBuf->size());
    ABitReader br(cleanSpsBuf->data(), cleanSpsBuf->size());

    unsigned int vps_max_sub_layers_minus1;

    /* 7.3.2.1 Video parameter set RBSP syntax */
    /*
     * vps_video_parameter_set_id u(4)
     * vps_reserved_three_2bits   u(2)
     * vps_max_layers_minus1      u(6)
     */
    br.skipBits(12);

    vps_max_sub_layers_minus1 = br.getBits(3);

    info->num_temporal_layers = MAX(info->num_temporal_layers, info->num_temporal_layers + 1);

    /*
     * vps_temporal_id_nesting_flag u(1)
     * vps_reserved_0xffff_16bits   u(16)
     */
    br.skipBits(17);

    if (vps_max_sub_layers_minus1 + 1 > MAX_SUB_LAYERS) {
        ALOGE("vps_max_sub_layers_minus1 %u not supported", vps_max_sub_layers_minus1);
        return;
    }

    ParseGeneralInfo(&br, info, vps_max_sub_layers_minus1);

    /* nothing useful in vps after this point */
    return;
}

void ParseHEVCPPS(const sp<ABuffer> &picParamSet, struct hevc_nal_info *info) {
    sp<ABuffer> cleanSpsBuf = new ABuffer(picParamSet->size() - 2);

    ExtractHEVCRbsp(cleanSpsBuf->data(), picParamSet->data() + 2, cleanSpsBuf->size());
    ABitReader br(cleanSpsBuf->data(), cleanSpsBuf->size());

    /* 7.3.2.3 Picture parameter set RBSP syntax */
    parseUE(&br); // pps_pic_parameter_set_id
    parseUE(&br); // pps_seq_parameter_set_id

    /* dependent_slice_segments_enabled_flag u(1)
     * output_flag_present_flag              u(1)
     * num_extra_slice_header_bits           u(3)
     * sign_data_hiding_enabled_flag         u(1)
     * cabac_init_present_flag               u(1)
     */
    br.skipBits(7);

    parseUE(&br); // num_ref_idx_l0_default_active_minus1
    parseUE(&br); // num_ref_idx_l1_default_active_minus1
    parseUE(&br); // init_qp_minus26

    /* constrained_intra_pred_flag u(1)
     * transform_skip_enabled_flag u(1)
     */
    br.skipBits(2);

    if (br.getBits(1)) { // cu_qp_delta_enabled_flag
        parseUE(&br); // diff_cu_qp_delta_depth
    }

    parseSE(&br); // pps_cb_qp_offset
    parseSE(&br); // pps_cr_qp_offset

    /* pps_slice_chroma_qp_offsets_present_flag u(1)
     * weighted_pred_flag                       u(1)
     * weighted_bipred_flag                     u(1)
     * transquant_bypass_enabled_flag           u(1)
     */
    br.skipBits(4);

    uint8_t tiles_enabled_flag = br.getBits(1);
    uint8_t entropy_coding_sync_enabled_flag = br.getBits(1);

    if (tiles_enabled_flag && entropy_coding_sync_enabled_flag)  {
        info->parallelism_type = 0; // mixed-type parallel decoding
    } else if (entropy_coding_sync_enabled_flag) {
        info->parallelism_type = 3; // entropy coding synchronization based parallel decoding
    } else if (tiles_enabled_flag) {
        info->parallelism_type = 2; // tile based parallel decoding
    } else {
        info->parallelism_type = 1; // slice based parallel decoding
    }

    /* nothing useful in pps after this point */
    return;
}


status_t getNextNALUnitHEVC(
        const uint8_t **_data, size_t *_size,
        const uint8_t **nalStart, size_t *nalSize,
        bool startCodeFollows) {
    const uint8_t *data = *_data;
    size_t size = *_size;
    *nalStart = NULL;
    *nalSize = 0;

    if (size == 0) {
        return -EAGAIN;
    }

    // Skip any number of leading 0x00.
    size_t offset = 0;
    for (;;) {
        while (offset < size && data[offset] != 0x01) {
            ++offset;
        }

        if (offset == size) {
            if (startCodeFollows) {
                offset = size + 2;
                break;
            }

            return -EAGAIN;
        }

        if (data[offset - 1] == 0x00 && data[offset - 2] == 0x00) {
            break;
        }

        ++offset;
    }

    ++offset;
    size_t startOffset = offset;
    for (;;) {
        while (offset < size && data[offset] != 0x01) {
            ++offset;
        }

        if (offset == size) {
            if (startCodeFollows) {
                offset = size + 2;
                break;
            }

            return -EAGAIN;
        }

        if (data[offset - 1] == 0x00 && data[offset - 2] == 0x00) {
            break;
        }

        ++offset;
    }

    size_t endOffset = offset - 2;
    while (endOffset > startOffset + 1 && data[endOffset - 1] == 0x00) {
        --endOffset;
    }

    *nalStart = &data[startOffset];
    *nalSize = endOffset - startOffset;
    if (offset + 2 < size) {
        *_data = &data[offset - 2];
        *_size = size - offset + 2;
    } else {
        *_data = NULL;
        *_size = 0;
    }

    return OK;
}

static sp<ABuffer> FindNALHEVC(const uint8_t *data, size_t size, unsigned nalType) {
    const uint8_t *nalStart;
    size_t nalSize;
    while (getNextNALUnitHEVC(&data, &size, &nalStart, &nalSize, true) == OK) {
        if (((nalStart[0]>>1) & 0x3f) == nalType) {
            sp<ABuffer> buffer = new ABuffer(nalSize);
            memcpy(buffer->data(), nalStart, nalSize);
            return buffer;
        }
    }

    return NULL;
}

sp<MetaData> MakeHEVCCodecSpecificData(const sp<ABuffer> &accessUnit) {
    const uint8_t *data = accessUnit->data();
    size_t size = accessUnit->size();

    uint8_t num_of_array = 0;

    struct hevc_nal_info info;
    memset (&info, 0, sizeof(struct hevc_nal_info));

    /* set min_spatial_segmentation_idc to max + 1, and if it's
     * not changed after parsing SPS, then it's inferred to be equal to 0
     */
    info.min_spatial_segmentation_idc = MAX_SPATIAL_SEGMENTATION + 1;

    /* general_profile_compatibility_flags and general_constraint_indicator_flags
     * require all the same field to be set by both sps and vps
     */
    info.general_profile_compatibility_flags = 0xFFFFFFFF;
    info.general_constraint_indicator_flags = 0xFFFFFFFFFFFF;

    bool isVpsPresent = true;
    sp<ABuffer> videoParamSet = FindNALHEVC(data, size, kHEVCNALUnitVPS);

    if (videoParamSet == NULL) {
        isVpsPresent = false;
    } else {
        num_of_array++;
    }

    sp<ABuffer> seqParamSet = FindNALHEVC(data, size, kHEVCNALUnitSPS);
    if (seqParamSet == NULL) {
        return NULL;
    }
    num_of_array++;

    sp<ABuffer> picParamSet = FindNALHEVC(data, size, kHEVCNALUnitPPS);
    CHECK(picParamSet != NULL);
    num_of_array++;

    /* Start to parse param sets */
    if (isVpsPresent) {
        ParseHEVCVPS(videoParamSet, &info);
    }
    ParseHEVCSPS(seqParamSet, &info);
    ParseHEVCPPS(picParamSet, &info);

    if (info.min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION) {
        info.min_spatial_segmentation_idc = 0;
    }

    // indicates the type of parallelism that is used to meet the restrictions imposed by
    // min_spatial_segmentation_idc when the value of min_spatial_segmentation_idc is greater than 0
    // parallelism_type = 0 indicates that the stream supports mixed types of parallel decoding or that the
    // parallelism type is unknown.
    if (!info.min_spatial_segmentation_idc) {
        info.parallelism_type = 0;
    }

    // Set those field to unspecified values, as it's not clear how to computer them
    info.avg_frame_rate = 0;
    info.constant_frame_rate = 0;

    size_t csdSize =
        1 + 22 +
        (isVpsPresent ? 1 + 2 * 1 + 2 + videoParamSet->size() : 0) +
        1 + 2 * 1 + 2 + seqParamSet->size() +
        1 + 2 * 1 + 2 + picParamSet->size();

#if !LOG_NDEBUG
    ALOGI("###### HEVCDecoderConfigurationRecord info ########");
    ALOGI("- configurationVersion: 0x01");  // this is currently hardcoded
    ALOGI("- general_profile_space: 0x%x", info.general_profile_space);
    ALOGI("- generial_tier_flag: 0x%x", info.general_tier_flag);
    ALOGI("- general_profile_idc: 0x%x", info.general_profile_idc);
    ALOGI("- general_profile_compatibility_flags: 0x%lx", info.general_profile_compatibility_flags);
    ALOGI("- general_constraint_indicator_flags: 0x%llx", info.general_constraint_indicator_flags);
    ALOGI("- general_level_idc: 0x%x", info.general_level_idc);
    ALOGI("- min_spatial_segmentation_idc: 0x%lx", info.min_spatial_segmentation_idc);
    ALOGI("- parallelism_type: 0x%x", info.parallelism_type);
    ALOGI("- chroma_format: 0x%x", info.chroma_format);
    ALOGI("- bit_depth_luma_minus8: 0x%x", info.bit_depth_luma_minus8);
    ALOGI("- bit_depth_chroma_minus8: 0x%x", info.bit_depth_chroma_minus8);
    ALOGI("- avg_frame_rate: 0x%x", info.avg_frame_rate);
    ALOGI("- constant_frame_rate: 0x%x", info.constant_frame_rate);
    ALOGI("- num_temporal_layers: 0x%x", info.num_temporal_layers);
    ALOGI("- temporal_id_nesting_flag: 0x%x", info.temporal_id_nesting_flag);
    ALOGI("- num_of_array: 0x%x", num_of_array);
    ALOGI("- pic_width_luma: %d, pic_heigth_luma: %d", info.pic_width_in_luma_samples, info.pic_height_in_luma_samples);
    ALOGI("##################################################");
#endif

    sp<ABuffer> csd = new ABuffer(csdSize);
    uint8_t *out = csd->data();
    *out++ = 0x01;  // configurationVersion

    *out++ = info.general_profile_space << 6 | info.general_tier_flag << 5 | info.general_profile_idc;

    *out++ = info.general_profile_compatibility_flags >> 24 & 0xff;
    *out++ = info.general_profile_compatibility_flags >> 16 & 0xff;
    *out++ = info.general_profile_compatibility_flags >> 8 & 0xff;
    *out++ = info.general_profile_compatibility_flags & 0xff;

    *out++ = info.general_constraint_indicator_flags >> 40 & 0xff;
    *out++ = info.general_constraint_indicator_flags >> 32 & 0xff;
    *out++ = info.general_constraint_indicator_flags >> 24 & 0xff;
    *out++ = info.general_constraint_indicator_flags >> 16 & 0xff;
    *out++ = info.general_constraint_indicator_flags >> 8 & 0xff;
    *out++ = info.general_constraint_indicator_flags & 0xff;

    *out++ = info.general_level_idc;
    *out++ = (0xf << 4) | (info.min_spatial_segmentation_idc >> 8 & 0x0f);
    *out++ = info.min_spatial_segmentation_idc & 0xff;
    *out++ = (0x3f << 2) | (info.parallelism_type & 0x03);
    *out++ = (0x3f << 2) | (info.chroma_format & 0x03);
    *out++ = (0x1f << 3) | (info.bit_depth_luma_minus8 & 0x07);
    *out++ = (0x1f << 3) | (info.bit_depth_chroma_minus8 & 0x07);

    *out++ = info.avg_frame_rate >> 8;
    *out++ = info.avg_frame_rate & 0xff;
    *out++ = info.constant_frame_rate << 6 | info.num_temporal_layers << 3 | info.temporal_id_nesting_flag << 2 | 3;

    *out++ = num_of_array;

    // VPS
    if (isVpsPresent) {
      *out++ = 0x3F & kHEVCNALUnitVPS;
      *out++ = 0x01 >> 8;
      *out++ = 0x01 & 0xff;
      *out++ = videoParamSet->size() >> 8;
      *out++ = videoParamSet->size() & 0xff;
      memcpy(out, videoParamSet->data(), videoParamSet->size());
      out += videoParamSet->size();
    }

    // SPS
    *out++ = 0x3F & kHEVCNALUnitSPS;
    *out++ = 0x01 >> 8;
    *out++ = 0x01 & 0xff;
    *out++ = seqParamSet->size() >> 8;
    *out++ = seqParamSet->size() & 0xff;
    memcpy(out, seqParamSet->data(), seqParamSet->size());
    out += seqParamSet->size();

    // PPS
    *out++ = 0x3F & kHEVCNALUnitPPS;
    *out++ = 0x01 >> 8;
    *out++ = 0x01 & 0xff;
    *out++ = picParamSet->size() >> 8;
    *out++ = picParamSet->size() & 0xff;
    memcpy(out, picParamSet->data(), picParamSet->size());
    out += picParamSet->size();

    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_HEVC);
    meta->setData(kKeyHVCC, kTypeHVCC, csd->data(), csd->size());

    meta->setInt32(kKeyWidth, info.pic_width_in_luma_samples);
    meta->setInt32(kKeyHeight, info.pic_height_in_luma_samples);

    return meta;
}

}  // namespace android
