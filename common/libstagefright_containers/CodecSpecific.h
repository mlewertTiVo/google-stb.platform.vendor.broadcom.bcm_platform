 /*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef CODEC_SPECIFIC_H_

#define CODEC_SPECIFIC_H_

#include <utils/Errors.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaData.h>
#include "containers/containers.h"

namespace android {

class CodecSpecific {
public:
    CodecSpecific(VC_CONTAINER_ES_FORMAT_T *format = NULL);

    void setFormat(VC_CONTAINER_ES_FORMAT_T *format);
    status_t setCodecSpecificHeaders(const sp<MetaData> &meta);

    size_t getExtraBufferSize();
    void convert(MediaBuffer *buffer);

private:
    VC_CONTAINER_ES_FORMAT_T *mFormat;
    unsigned int mNALLengthSize;
    int64_t mLastPTS;
    int64_t mFramesSinceLastPTS;
    unsigned int mFrameLength;
};

}  // namespace android

#endif  // CODEC_SPECIFIC_H_

