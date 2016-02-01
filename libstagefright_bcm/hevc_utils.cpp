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
#define LOG_TAG "hevc_utils"
#include <utils/Log.h>

#include "include/avc_utils.h"
#include "include/hevc_utils.h"

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>

namespace android {

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

void ParseHEVCSPS(
        const sp<ABuffer> &seqParamSet,
        int32_t *width, int32_t *height) {

    sp<ABuffer> cleanSpsBuf = new ABuffer(seqParamSet->size() - 2);

    ExtractHEVCRbsp(cleanSpsBuf->data(), seqParamSet->data() + 2, cleanSpsBuf->size());
    ABitReader br(cleanSpsBuf->data(), cleanSpsBuf->size());

    // 7.3.2.2 Sequence parameter set RBSP syntax
    unsigned int sps_video_parameter_set_id = br.getBits(4);
    unsigned int sps_max_sub_layers_minus1 = br.getBits(3);
    br.skipBits(1);

    unsigned i;
    unsigned j;
    unsigned data;
    unsigned general_profile_space;
    bool sub_layer_profile_present_flag[8];
    bool sub_layer_level_present_flag[8];

    if (sps_max_sub_layers_minus1 >= 8) {
        ALOGE("sps_max_sub_layers_minus1 %u not supported", sps_max_sub_layers_minus1);
    }
    general_profile_space = br.getBits(2);

    br.getBits(1);
    br.getBits(5);

    for (j=0; j < 32; j++) {
        br.getBits(1);
    }

    br.skipBits(4);
    br.skipBits(44); // general_reserved_zero_44bits
    br.skipBits(8);

    for (i=0; i < sps_max_sub_layers_minus1; i++) {
        sub_layer_profile_present_flag[i] = br.getBits(1);
        sub_layer_level_present_flag[i] = br.getBits(1);
    }

    if (sps_max_sub_layers_minus1 > 0) {
        for (i = sps_max_sub_layers_minus1; i < 8; i++) {
            unsigned reserved_zero_2bits = br.getBits(2);
        }
    }

    for (i = 0; i < sps_max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            br.skipBits(8);
            for (j = 0; j < 32; j++) {
                br.getBits(1);
            }

            br.skipBits(4);
            br.skipBits(44); // general_reserved_zero_44bits
        }

        if(sub_layer_level_present_flag[ i ]) {
            br.skipBits(8);
        }
    }

    parseUE(&br);
    unsigned chroma_format_idc = parseUE(&br);
    if(chroma_format_idc == 3) {
        br.skipBits(1);
    }
    *width= parseUE(&br);
    *height = parseUE(&br);

    // the left SPS parsing is not finished yet, and not needed right now
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

    sp<ABuffer> seqParamSet = FindNALHEVC(data, size, 33);
    if (seqParamSet == NULL) {
        return NULL;
    }

    bool isVpsPresent = true;
    sp<ABuffer> videoParamSet = FindNALHEVC(data, size, 32);
    if (videoParamSet == NULL) {
      isVpsPresent = false;
    }
    int32_t width = 0;
    int32_t height = 0;

    ParseHEVCSPS(seqParamSet, &width, &height);
    size_t stopOffset;
    sp<ABuffer> picParamSet = FindNALHEVC(data, size, 34);
    CHECK(picParamSet != NULL);

    size_t csdSize =
        1 +
        (isVpsPresent ? 1 + 2 * 1 + videoParamSet->size() : 0) +
        1 + 2 * 1 + seqParamSet->size() +
        1 + 2 * 1 + picParamSet->size();
    sp<ABuffer> csd = new ABuffer(csdSize);
    uint8_t *out = csd->data();
    *out++ = 0x01;  // configurationVersion

    // VPS
    if (isVpsPresent) {
      *out++ = 1;
      *out++ = videoParamSet->size() >> 8;
      *out++ = videoParamSet->size() & 0xff;
      memcpy(out, videoParamSet->data(), videoParamSet->size());
      out += videoParamSet->size();
    }

    // SPS
    *out++ = 1;
    *out++ = seqParamSet->size() >> 8;
    *out++ = seqParamSet->size() & 0xff;
    memcpy(out, seqParamSet->data(), seqParamSet->size());
    out += seqParamSet->size();

    // PPS
    *out++ = 1;
    *out++ = picParamSet->size() >> 8;
    *out++ = picParamSet->size() & 0xff;
    memcpy(out, picParamSet->data(), picParamSet->size());
    out += picParamSet->size();
    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_HEVC);
    meta->setData(kKeyHVCC, kTypeHVCC, csd->data(), csd->size());

    meta->setInt32(kKeyWidth, width);
    meta->setInt32(kKeyHeight, height);
    ALOGV("############## pic_width = %d, pic_heigth = %d ", width, height);

    return meta;
}

}  // namespace android
