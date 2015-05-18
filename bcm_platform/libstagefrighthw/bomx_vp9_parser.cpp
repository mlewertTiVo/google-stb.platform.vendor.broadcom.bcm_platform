/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree under external/libvpx/libvpx. An additional intellectual property
 *  rights grant can be found in the file PATENTS.  All contributing project
 *  authors may be found in the AUTHORS file in the same directory.
 */

//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "bomx_video_decoder"

#include <cutils/log.h>

#include "bomx_video_decoder.h"
#include "nexus_platform.h"

// Largely lifted from vp9_dx_iface.c - naming conventions kept intact
void BOMX_Vp9_ParseSuperframe(const uint8_t *data, size_t data_sz, uint32_t sizes[8], unsigned *count)
{
    uint8_t marker;

    ALOG_ASSERT(data_sz);
    marker = data[data_sz - 1];
    *count = 0;

    if ((marker & 0xe0) == 0xc0) {
        const uint32_t frames = (marker & 0x7) + 1;
        const uint32_t mag = ((marker >> 3) & 0x3) + 1;
        const size_t index_sz = 2 + mag * frames;
        size_t total_sz = 0;

        if (data_sz >= index_sz && data[data_sz - index_sz] == marker) {
            ALOGV("Superframe - %u bytes of index for %u frames (%u total)", index_sz, frames, data_sz);
            // found a valid superframe index
            uint32_t i, j;
            const uint8_t *x = &data[data_sz - index_sz + 1];

            for (i = 0; i < frames; i++) {
                uint32_t this_sz = 0;

                for (j = 0; j < mag; j++)
                    this_sz |= (*x++) << (j * 8);
                sizes[i] = this_sz;
                total_sz += this_sz;
                ALOGV("Superframe index %u/%u size %u", i, frames, this_sz);
            }

            if ( total_sz > (data_sz-index_sz) ) {
                ALOGW("VP9 superframe overflow - total of subframes is larger than frame size (%u/%u)", (unsigned)total_sz, (unsigned)data_sz-index_sz);
            } else {
                *count = frames;
            }
        }
    }
}
