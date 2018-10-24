/*
 * Copyright 2018 The Android Open Source Project
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
#define LOG_TAG "bcm.hardware.casproxy@1.0-service"

#include <hidl/HidlTransportSupport.h>

#include "MediaCasProxyService.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using bcm::hardware::casproxy::V1_0::implementation::MediaCasProxyService;
using bcm::hardware::casproxy::V1_0::IMediaCasProxyService;

int main() {
    ALOGD("bcm.hardware.casproxy@1.0-service starting...");

    configureRpcThreadpool(8, true /* callerWillJoin */);

    // Setup hwbinder service
    android::sp<IMediaCasProxyService> service = new MediaCasProxyService();
    android::status_t status = service->registerAsService();
    LOG_ALWAYS_FATAL_IF(
            status != android::OK,
            "Error while registering casproxy service: %d", status);
    joinRpcThreadpool();
    return 0;
}
