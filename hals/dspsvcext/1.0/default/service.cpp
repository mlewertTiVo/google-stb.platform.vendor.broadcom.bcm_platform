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

#define LOG_TAG "bcm.hardware.dspsvcext@1.0-service"

#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>
#include <hidl/HidlTransportSupport.h>

#include "DspSvcExt.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;
using android::status_t;
using android::OK;
using bcm::hardware::dspsvcext::V1_0::IDspSvcExt;
using bcm::hardware::dspsvcext::V1_0::implementation::DspSvcExt;

int main() {
    configureRpcThreadpool(4, true);

    sp<IDspSvcExt> dspsvcext = new DspSvcExt;
    status_t status = dspsvcext->registerAsService();
    if (status != OK) LOG_ALWAYS_FATAL("Could not register IDspSvcExt");
    dspsvcext->start();

    joinRpcThreadpool();
    return 0;
}
