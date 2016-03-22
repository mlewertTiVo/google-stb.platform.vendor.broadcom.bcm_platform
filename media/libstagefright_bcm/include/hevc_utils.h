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

#ifndef HEVC_UTILS_H_

#define HEVC_UTILS_H_

#include <media/stagefright/foundation/ABuffer.h>
#include <utils/Errors.h>

namespace android {

enum {
    kHEVCNALUnitVPS = 0x20,
    kHEVCNALUnitSPS = 0x21,
    kHEVCNALUnitPPS = 0x22
};

struct hevc_nal_info {
    uint8_t general_profile_space;
    uint8_t general_tier_flag;
    uint8_t general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t general_level_idc;
    uint32_t min_spatial_segmentation_idc;
    uint8_t parallelism_type;
    uint32_t chroma_format;
    int32_t pic_width_in_luma_samples;
    int32_t pic_height_in_luma_samples;
    uint8_t bit_depth_luma_minus8;
    uint8_t bit_depth_chroma_minus8;
    uint16_t avg_frame_rate;
    uint8_t constant_frame_rate;
    uint8_t num_temporal_layers;
    uint8_t temporal_id_nesting_flag;
};

void ParseHEVCVPS(
        const sp<ABuffer> &videoParamSet,
        struct hevc_sps_info *info);

void ParseHEVCSPS(
        const sp<ABuffer> &seqParamSet,
        struct hevc_sps_info *info);

void ParseHEVCPPS(
        const sp<ABuffer> &picParamSet,
        struct hevc_sps_info *info);

status_t getNextNALUnitHEVC(
        const uint8_t **_data, size_t *_size,
        const uint8_t **nalStart, size_t *nalSize,
        bool startCodeFollows = false);
sp<MetaData> MakeHEVCCodecSpecificData(const sp<ABuffer> &accessUnit);

}  // namespace android

#endif  // HEVC_UTILS_H_
