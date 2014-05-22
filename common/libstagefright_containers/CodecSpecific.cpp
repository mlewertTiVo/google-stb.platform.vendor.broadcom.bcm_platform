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

#define LOG_TAG "ContainersExtractor"
#include <utils/Log.h>

#include "CodecSpecific.h"
#include <ADebug.h>
#include <MediaErrors.h>
#include "containers/containers_codecs.h"

namespace android {

///////////////////////////////////////////////////////////////////////////////
// Vorbis specific functions
///////////////////////////////////////////////////////////////////////////////

static status_t setCodecInfoVorbis(
    const sp<MetaData> &meta,
    const uint8_t*codecPrivate, size_t codecPrivateSize) {

    if (codecPrivateSize < 3)
        return ERROR_MALFORMED;
    if (codecPrivate[0] != 0x02)
        return ERROR_MALFORMED;

    size_t len1 = codecPrivate[1];
    size_t len2 = codecPrivate[2];

    if (codecPrivateSize <= 3 + len1 + len2)
        return ERROR_MALFORMED;
    if (codecPrivate[3] != 0x01)
        return ERROR_MALFORMED;

    meta->setData(kKeyVorbisInfo, 0, &codecPrivate[3], len1);

    if (codecPrivate[len1 + 3] != 0x03)
        return ERROR_MALFORMED;
    if (codecPrivate[len1 + len2 + 3] != 0x05)
        return ERROR_MALFORMED;

    meta->setData(
            kKeyVorbisBooks, 0, &codecPrivate[len1 + len2 + 3],
            codecPrivateSize - len1 - len2 - 3);
    return NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
// MPEG4 video specific functions
///////////////////////////////////////////////////////////////////////////////
static void EncodeSize14(uint8_t **_ptr, size_t size) {
    uint8_t *ptr = *_ptr;
    *ptr++ = 0x80 | (size >> 7);
    *ptr++ = size & 0x7f;
    *_ptr = ptr;
}

static status_t setCodecInfoMPEG4(
    const sp<MetaData> &meta,
    const uint8_t*codecPrivate, size_t codecPrivateSize) {
    uint8_t *ptr, *buffer = ptr = (uint8_t *)malloc(codecPrivateSize + 25);
    if (!ptr)
        return NO_MEMORY;

    *ptr++ = 0x03;
    EncodeSize14(&ptr, 22 + codecPrivateSize);

    *ptr++ = 0x00;  // ES_ID
    *ptr++ = 0x00;

    *ptr++ = 0x00;  // streamDependenceFlag, URL_Flag, OCRstreamFlag

    *ptr++ = 0x04;
    EncodeSize14(&ptr, 16 + codecPrivateSize);

    *ptr++ = 0x40;  // Audio ISO/IEC 14496-3

    for (size_t i = 0; i < 12; ++i) {
        *ptr++ = 0x00;
    }

    *ptr++ = 0x05;
    EncodeSize14(&ptr, codecPrivateSize);

    memcpy(ptr, codecPrivate, codecPrivateSize);
    meta->setData(kKeyESDS, kTypeESDS, buffer, codecPrivateSize + 25);

    free(buffer);
    return NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
// Codec specific class
///////////////////////////////////////////////////////////////////////////////

CodecSpecific::CodecSpecific(VC_CONTAINER_ES_FORMAT_T *format)
    : mFormat(format), mNALLengthSize(0), mLastPTS(0),
    mFramesSinceLastPTS(0), mFrameLength(1024) {
}

void CodecSpecific::setFormat(VC_CONTAINER_ES_FORMAT_T *format) {
    mFormat = format;
}

status_t CodecSpecific::setCodecSpecificHeaders(const sp<MetaData> &meta) {
    status_t status = NO_ERROR;

    // We sanity check the codec config data for some of the
    // codecs to make sure stagefright won't be confused.
    if ((mFormat->codec == VC_CONTAINER_CODEC_H264 &&
         mFormat->codec_variant == VC_FOURCC('a','v','c','C')) ||
        mFormat->codec == VC_CONTAINER_CODEC_VORBIS)
        status = ERROR_MALFORMED;

    if (!mFormat->extradata_size) {
        if (status != NO_ERROR)
            ALOGE("CodecSpecific: codec %4.4s missing config data",
                 (char *)&mFormat->codec);
        return status;
    }

    // Deal with codec config data and format it the way stagefright expects
    switch (mFormat->codec) {
       case VC_CONTAINER_CODEC_H264:
           meta->setData(kKeyAVCC, kTypeAVCC, mFormat->extradata, mFormat->extradata_size);
           if (mFormat->codec_variant == VC_FOURCC('a','v','c','C') &&
               mFormat->extradata_size >= 7) {
               const uint8_t *ptr = (const uint8_t *)mFormat->extradata;
               if (ptr[0] == 1) // configurationVersion == 1
                   mNALLengthSize = 1 + (ptr[4] & 3);
           }
           if (mNALLengthSize > 1)
               status = NO_ERROR;
           break;
       case VC_CONTAINER_CODEC_MP4V:
       case VC_CONTAINER_CODEC_MP4A:
           status = setCodecInfoMPEG4(meta, mFormat->extradata, mFormat->extradata_size);
           break;
       case VC_CONTAINER_CODEC_VORBIS:
           status = setCodecInfoVorbis(meta, mFormat->extradata, mFormat->extradata_size);
           break;
       default:
           meta->setData(kKeyCodecConfig, kTypeGeneric, mFormat->extradata,
                         mFormat->extradata_size);
           status = NO_ERROR;
           break;
    }

    if (status != NO_ERROR)
        ALOGE("CodecSpecific: codec %4.4s has malformed config data", 
             (char *)&mFormat->codec);

    return status;
}

size_t CodecSpecific::getExtraBufferSize() {
    return mNALLengthSize == 2 ? 10 : 0;
}

void CodecSpecific::convert(MediaBuffer *buffer) {
    int64_t timestamp;

    if (!buffer->meta_data()->findInt64(kKeyTime, &timestamp))
        timestamp = VC_CONTAINER_TIME_UNKNOWN;

    // Convert AVC NAL lengths into start codes
    if (mNALLengthSize) {
        uint8_t *data = (uint8_t *)buffer->data() + buffer->range_offset();
        size_t buffer_size = buffer->range_length();

        for (unsigned int size = 0, nalSize = 0; size < buffer_size; size += nalSize) {
            if (size + mNALLengthSize > buffer_size)
                break;

            if (mNALLengthSize == 2) {
                nalSize = (data[0]<<8)|data[1];
                memmove(data + 1, data, buffer_size - size);
                data[0] = 0; data[1] = 0; data[2] = 1;
                buffer_size++;
                nalSize++;
                // FIXME check overrun
            }
            else if (mNALLengthSize == 3) {
                nalSize = (data[0]<<16)|(data[1]<<8)|data[2];
                data[0] = 0; data[1] = 0; data[2] = 1;
            }
            else if (mNALLengthSize == 4) {
                nalSize = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
                data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 1;
            }
            else CHECK(0);

            nalSize += mNALLengthSize;
            data += nalSize;
        }

        buffer->set_range(buffer->range_offset(), buffer_size);
    }

    if (timestamp == VC_CONTAINER_TIME_UNKNOWN &&
        mFormat->es_type == VC_CONTAINER_ES_TYPE_AUDIO) {
        int64_t timestamp = mLastPTS + (mFramesSinceLastPTS * mFrameLength *
            1000000LL / mFormat->type->audio.sample_rate);
        buffer->meta_data()->setInt64(kKeyTime, timestamp);
        ALOGE("audio timestamp missing, inventing one (%lld, %i) -> %lld",
             mLastPTS, (int)mFramesSinceLastPTS, timestamp);
    }

    if (timestamp != VC_CONTAINER_TIME_UNKNOWN) {
        mLastPTS = timestamp;
        mFramesSinceLastPTS = 0;
    }
    mFramesSinceLastPTS++;
}

} // namespace android

