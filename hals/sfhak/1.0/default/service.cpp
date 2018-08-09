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

#define LOG_TAG "bcm.hardware.sfhak@1.0-service"

#include <bcm/hardware/sfhak/1.0/ISfHak.h>
#include <hidl/HidlTransportSupport.h>

#include "SfHak.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;
using android::status_t;
using android::OK;
using bcm::hardware::sfhak::V1_0::ISfHak;
using bcm::hardware::sfhak::V1_0::implementation::SfHak;

int main() {
    configureRpcThreadpool(4, true);

    sp<ISfHak> sfhak = new SfHak;
    status_t status = sfhak->registerAsService();
    if (status != OK) LOG_ALWAYS_FATAL("Could not register ISfHak");

    joinRpcThreadpool();
    return 0;
}