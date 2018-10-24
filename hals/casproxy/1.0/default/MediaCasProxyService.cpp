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
#define LOG_TAG "bcm.hardware.casproxy@1.0-MediaCasProxyService"

#include <android/hardware/cas/1.0/ICasListener.h>
#include <media/cas/CasAPI.h>
#include <media/cas/DescramblerAPI.h>
#include <utils/Log.h>

#include "CasImpl.h"
#include "DescramblerImpl.h"
#include "MediaCasProxyService.h"

namespace bcm {
namespace hardware {
namespace casproxy {
namespace V1_0 {
namespace implementation {

using namespace android::hardware::cas::V1_0::implementation;
using android::CasFactory;
using android::CasPlugin;
using android::DescramblerFactory;
using android::DescramblerPlugin;
using android::hardware::cas::V1_0::HidlCasPluginDescriptor;
using android::hardware::cas::native::V1_0::IDescrambler;


MediaCasProxyService::MediaCasProxyService() :
    mCasLoader("createCasProxyFactory"),
    mDescramblerLoader("createDescramblerProxyFactory") {
    ALOGV("%s", __func__);
}

MediaCasProxyService::~MediaCasProxyService() {
    ALOGV("%s", __func__);
}

Return<void> MediaCasProxyService::enumeratePlugins(enumeratePlugins_cb _hidl_cb) {

    ALOGV("%s", __func__);

    vector<HidlCasPluginDescriptor> results;
    mCasLoader.enumeratePlugins(&results);

    _hidl_cb(results);
    return Void();
}

Return<bool> MediaCasProxyService::isSystemIdSupported(int32_t CA_system_id) {
    ALOGV("isSystemIdSupported: CA_system_id=0x%04x", CA_system_id);

    return mCasLoader.findFactoryForScheme(CA_system_id);
}

Return<sp<ICas>> MediaCasProxyService::createPlugin(
        int32_t CA_system_id, const sp<ICasListener>& listener) {

    ALOGV("%s: CA_system_id=0x%04x", __func__, CA_system_id);

    sp<ICas> result;

    CasFactory *factory;
    sp<SharedLibrary> library;
    if (mCasLoader.findFactoryForScheme(CA_system_id, &library, &factory)) {
        CasPlugin *plugin = NULL;
        sp<CasImpl> casImpl = new CasImpl(listener);
        if (factory->createPlugin(CA_system_id, casImpl.get(),
                &CasImpl::OnEvent, &plugin) == android::OK && plugin != NULL) {
            casImpl->init(library, plugin);
            result = casImpl;
        }
    }

    return result;
}

Return<bool> MediaCasProxyService::isDescramblerSupported(int32_t CA_system_id) {
    ALOGV("%s: CA_system_id=%04x", __func__, CA_system_id);

    return mDescramblerLoader.findFactoryForScheme(CA_system_id);
}

Return<sp<IDescramblerBase>> MediaCasProxyService::createDescrambler(int32_t CA_system_id) {

    ALOGV("%s: CA_system_id=%04x", __func__, CA_system_id);

    sp<IDescrambler> result;

    DescramblerFactory *factory;
    sp<SharedLibrary> library;
    if (mDescramblerLoader.findFactoryForScheme(
            CA_system_id, &library, &factory)) {
        DescramblerPlugin *plugin = NULL;
        if (factory->createPlugin(CA_system_id, &plugin) == android::OK
                && plugin != NULL) {
            result = new DescramblerImpl(library, plugin);
        }
    }

    return result;
}

} // namespace implementation
} // namespace V1_0
} // namespace casproxy
} // namespace hardware
} // namespace bcm
