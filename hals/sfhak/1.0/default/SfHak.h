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

#ifndef BCM_HARDWARE_SFHAK_V1_0_SFHAK_H
#define BCM_HARDWARE_SFHAK_V1_0_SFHAK_H

#include <bcm/hardware/sfhak/1.0/ISfHak.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace bcm {
namespace hardware {
namespace sfhak {
namespace V1_0 {
namespace implementation {

using ::android::hardware::Return;
using ::android::sp;

class SfHak : public ISfHak {
public:
    SfHak();
    ~SfHak();

    // Methods from ::bcm::hardware::sfhak::V1_0::ISfHak follow.
    Return<uint64_t> getVideoPidChannelHandle() override;
    Return<uint64_t> getAudioPidChannelHandle() override;
    Return<::bcm::hardware::sfhak::V1_0::Status> setVideoPidChannelHandle(uint64_t h) override;
    Return<::bcm::hardware::sfhak::V1_0::Status> setAudioPidChannelHandle(uint64_t h) override;

private:
    uint64_t mVideoPidChannelHandle;
    uint64_t mAudioPidChannelHandle;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sfhak
}  // namespace hardware
}  // namespace bcm

#endif  // BCM_HARDWARE_SFHAK_V1_0_SFHAK_H
