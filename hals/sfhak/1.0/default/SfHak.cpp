/*
 * Copyright (C) 2018 The Android Open Source Project
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
#define LOG_TAG "bcm.hardware.sfhak@1.0-service"
#include <utils/Log.h>
#include "SfHak.h"

namespace bcm {
namespace hardware {
namespace sfhak {
namespace V1_0 {
namespace implementation {

using ::bcm::hardware::sfhak::V1_0::Status;

SfHak::SfHak() : mVideoPidChannelHandle(0ULL), mAudioPidChannelHandle(0ULL) {
    ALOGV("%s: CTOR", __func__);
}

SfHak::~SfHak() {
    ALOGV("%s: DTOR", __func__);
}

Return<uint64_t> SfHak::getVideoPidChannelHandle() {
    return mVideoPidChannelHandle;
}

Return<uint64_t> SfHak::getAudioPidChannelHandle() {
    return mAudioPidChannelHandle;
}

Return<Status> SfHak::setVideoPidChannelHandle(uint64_t h) {
    if (h == 0) {
        ALOGE("%s: Invalid handle!", __func__);
        return Status::BAD_VALUE;
    }
    mVideoPidChannelHandle = h;
    return Status::OK;
}

Return<Status> SfHak::setAudioPidChannelHandle(uint64_t h) {
    if (h == 0) {
        ALOGE("%s: Invalid handle!", __func__);
        return Status::BAD_VALUE;
    }
    mAudioPidChannelHandle = h;
    return Status::OK;
}


}  // namespace implementation
}  // namespace V1_0
}  // namespace sfhak
}  // namespace hardware
}  // namespace bcm
