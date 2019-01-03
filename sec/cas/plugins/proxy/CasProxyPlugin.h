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

#ifndef CAS_PROXY_PLUGIN_H_
#define CAS_PROXY_PLUGIN_H_

#include <android/hardware/cas/1.0/ICas.h>
#include <android/hardware/cas/native/1.0/IDescrambler.h>
#include <media/cas/CasAPI.h>
#include <media/cas/DescramblerAPI.h>
#include <utils/Mutex.h>
#include <utils/Errors.h>
#include <bcm/hardware/casproxy/1.0/IMediaCasProxyService.h>

extern "C" {
      android::CasFactory *createCasFactory();
      android::DescramblerFactory *createDescramblerFactory();
}

namespace android {

using android::hardware::cas::V1_0::ICas;
using hardware::cas::native::V1_0::IDescrambler;
using bcm::hardware::casproxy::V1_0::IMediaCasProxyService;

class CasProxyHelper {
public:
    static const unsigned SLEEP_S = (1000 * 1000);
    static const unsigned MAX_ATTEMPT_COUNT = 4;

    static const sp<IMediaCasProxyService> getService();
};

class CasProxyFactory : public CasFactory {
public:
    CasProxyFactory();
    virtual ~CasProxyFactory();

    virtual bool isSystemIdSupported(
            int32_t CA_system_id) const override;
    virtual status_t queryPlugins(
            std::vector<CasPluginDescriptor> *descriptors) const override;
    virtual status_t createPlugin(
            int32_t CA_system_id,
            void *appData,
            CasPluginCallback callback,
            CasPlugin **plugin) override;
};

class DescramblerProxyFactory : public DescramblerFactory {
public:
    DescramblerProxyFactory();
    virtual ~DescramblerProxyFactory();

    virtual bool isSystemIdSupported(
            int32_t CA_system_id) const override;
    virtual status_t createPlugin(
            int32_t CA_system_id, DescramblerPlugin **plugin) override;
};

class CasProxyPlugin : public CasPlugin {
public:
    CasProxyPlugin(sp<ICas> cas);
    virtual ~CasProxyPlugin();

    virtual status_t setPrivateData(
            const CasData &data) override;

    virtual status_t openSession(CasSessionId *sessionId) override;

    virtual status_t closeSession(
            const CasSessionId &sessionId) override;

    virtual status_t setSessionPrivateData(
            const CasSessionId &sessionId,
            const CasData &data) override;

    virtual status_t processEcm(
            const CasSessionId &sessionId, const CasEcm &ecm) override;

    virtual status_t processEmm(const CasEmm &emm) override;

    virtual status_t sendEvent(
            int32_t event, int32_t arg, const CasData &eventData) override;

    virtual status_t provision(const String8 &str) override;

    virtual status_t refreshEntitlements(
            int32_t refreshType, const CasData &refreshData) override;

private:
    sp<ICas> mICas;
    Mutex mLock;
};

class DescramblerProxyPlugin : public DescramblerPlugin {
public:
    DescramblerProxyPlugin(sp<IDescrambler> descrambler);
    virtual ~DescramblerProxyPlugin();

    virtual bool requiresSecureDecoderComponent(
            const char *mime) const override;

    virtual status_t setMediaCasSession(
            const CasSessionId &sessionId) override;

    virtual ssize_t descramble(
            bool secure,
            ScramblingControl scramblingControl,
            size_t numSubSamples,
            const SubSample *subSamples,
            const void *srcPtr,
            int32_t srcOffset,
            void *dstPtr,
            int32_t dstOffset,
            AString *errorDetailMsg) override;

private:
    String8 subSamplesToString(
            SubSample const *subSamples,
            size_t numSubSamples) const;

    sp<IDescrambler> mDescrambler;
    Mutex mLock;
};
} // namespace android

#endif // CAS_PROXY_PLUGIN_H_
