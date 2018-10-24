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

#ifndef BCM_HARDWARE_CASPROXY_V1_0_MEDIA_CAS_PROXY_SERVICE_H_
#define BCM_HARDWARE_CASPROXY_V1_0_MEDIA_CAS_PROXY_SERVICE_H_

#include <bcm/hardware/casproxy/1.0/IMediaCasProxyService.h>

#include "FactoryLoader.h"

namespace android {
struct CasFactory;
struct DescramblerFactory;
}

namespace bcm {
namespace hardware {
namespace casproxy {
namespace V1_0 {
namespace implementation {

using android::sp;
using android::hardware::cas::V1_0::ICas;
using android::hardware::cas::V1_0::ICasListener;
using android::hardware::cas::V1_0::IDescramblerBase;
using android::hardware::cas::V1_0::implementation::FactoryLoader;
using android::hardware::Return;
using android::hardware::Void;

class MediaCasProxyService : public IMediaCasProxyService {
public:
    MediaCasProxyService();

    virtual Return<void> enumeratePlugins(
            enumeratePlugins_cb _hidl_cb) override;

    virtual Return<bool> isSystemIdSupported(
            int32_t CA_system_id) override;

    virtual Return<sp<ICas>> createPlugin(
            int32_t CA_system_id, const sp<ICasListener>& listener) override;

    virtual Return<bool> isDescramblerSupported(
            int32_t CA_system_id) override;

    virtual Return<sp<IDescramblerBase>> createDescrambler(
            int32_t CA_system_id) override;

private:
    FactoryLoader<android::CasFactory> mCasLoader;
    FactoryLoader<android::DescramblerFactory> mDescramblerLoader;

    virtual ~MediaCasProxyService();
};

} // namespace implementation
} // namespace V1_0
} // namespace casproxy
} // namespace hardware
} // namespace bcm

#endif // BCM_HARDWARE_CASPROXY_V1_0_MEDIA_CAS_PROXY_SERVICE_H_
