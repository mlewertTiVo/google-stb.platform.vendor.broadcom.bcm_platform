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

// Uncomment the line below to enable verbose logging
//#define LOG_NDEBUG 0
#define LOG_TAG "CasProxyPlugin"

#include <android/hardware/cas/1.0/ICasListener.h>
#include <binder/IMemory.h>
#include <binder/MemoryDealer.h>
#include <cutils/native_handle.h>
#include <hidlmemory/FrameworkUtils.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/Log.h>
#include <bcm/hardware/casproxy/1.0/IMediaCasProxyService.h>

#include "CasProxyPlugin.h"


android::CasFactory* createCasFactory() {
    return new android::CasProxyFactory();
}

android::DescramblerFactory* createDescramblerFactory() {
    return new android::DescramblerProxyFactory();
}

namespace android {

using android::hardware::cas::V1_0::HidlCasPluginDescriptor;
using android::hardware::cas::V1_0::ICasListener;
using android::hardware::cas::V1_0::IDescramblerBase;
using android::hardware::cas::V1_0::Status;
using android::hardware::cas::native::V1_0::BufferType;
using android::hardware::cas::native::V1_0::DestinationBuffer;
using android::hardware::cas::native::V1_0::IDescrambler;
using android::hardware::fromHeap;
using android::hardware::hidl_handle;
using android::hardware::hidl_string;
using android::hardware::HidlMemory;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;

class MediaCasProxyListener : public ICasListener {
public:
    MediaCasProxyListener(void *appData, CasPluginCallback callback) : mAppData(appData), mCallback(callback) { };
    virtual Return<void> onEvent(int32_t event, int32_t arg,
                                 const hidl_vec<uint8_t>& data) override {
        if (mCallback != NULL) {
            mCallback(mAppData, event, arg, (uint8_t*)data.data(), data.size());
        }
        return Void();
    }

private:
    void *mAppData;
    CasPluginCallback mCallback;
};

const sp<IMediaCasProxyService> CasProxyHelper::getService() {
    static sp<IMediaCasProxyService> casProxy = NULL;
    unsigned cnt = 0;

    while (casProxy == NULL && cnt < CasProxyHelper::MAX_ATTEMPT_COUNT) {
        casProxy = IMediaCasProxyService::getService();
        if (casProxy != NULL) {
            break;
        }
        usleep(CasProxyHelper::SLEEP_S * 0.5);
        cnt++;
    }
    return casProxy;
}

CasProxyFactory::CasProxyFactory() {
    ALOGV("%s: CTOR", __func__);
}

CasProxyFactory::~CasProxyFactory() {
    ALOGV("%s: DTOR", __func__);
}

bool CasProxyFactory::isSystemIdSupported(int32_t CA_system_id) const {
    Return<bool> supported(false);
    sp<IMediaCasProxyService> casProxy = CasProxyHelper::getService();

    ALOGV("CasProxyFactory::isSystemIdSupported: CA ID=0x%04x", CA_system_id);

    if (casProxy == NULL) {
        ALOGE("%s: Could not get CasProxy Service!", __func__);
    }
    else {
        supported = casProxy->isSystemIdSupported(CA_system_id);
    }
    return static_cast<bool>(supported);
}

status_t CasProxyFactory::queryPlugins(
        std::vector<CasPluginDescriptor> *descriptors) const {
    status_t ret = android::OK;

    ALOGV("CasProxyFactory::queryPlugins");

    descriptors->clear();
    sp<IMediaCasProxyService> casProxy = CasProxyHelper::getService();

    if (casProxy == NULL) {
        ALOGE("%s: Could not get CasProxy Service!", __func__);
        ret = android::UNKNOWN_ERROR;
    }
    else {
        casProxy->enumeratePlugins([&descriptors](std::vector<HidlCasPluginDescriptor> results) {
            for (auto it = results.begin(); it != results.end(); it++) {
                descriptors->push_back( CasPluginDescriptor {
                        .CA_system_id = it->caSystemId,
                        .name = String8(it->name.c_str())});
            }
        });
    }
    return ret;
}

status_t CasProxyFactory::createPlugin(
        int32_t CA_system_id,
        void *appData,
        CasPluginCallback callback,
        CasPlugin **plugin) {

    status_t status = android::BAD_VALUE;
    sp<IMediaCasProxyService> casProxy = CasProxyHelper::getService();

    ALOGV("CasProxyFactory::createPlugin: CA ID=0x%04x", CA_system_id);

    if (casProxy == NULL) {
        ALOGE("%s: Could not get CasProxy Service!", __func__);
    }
    else {
        if (isSystemIdSupported(CA_system_id)) {
            sp<MediaCasProxyListener> casListener = new MediaCasProxyListener(appData, callback);
            Return<sp<ICas>> hidlResult = casProxy->createPlugin(CA_system_id, casListener);
            if (hidlResult.isOk()) {
                *plugin = new CasProxyPlugin(sp<ICas>(hidlResult));
                status = android::OK;
            }
        }
    }
    return status;
}

///////////////////////////////////////////////////////////////////////////////

DescramblerProxyFactory::DescramblerProxyFactory() {
    ALOGV("%s: CTOR", __func__);
}

DescramblerProxyFactory::~DescramblerProxyFactory() {
    ALOGV("%s: DTOR", __func__);
}

bool DescramblerProxyFactory::isSystemIdSupported(int32_t CA_system_id) const {
    Return<bool> supported(false);
    sp<IMediaCasProxyService> casProxy = CasProxyHelper::getService();

    ALOGV("DescramblerProxyFactory::isSystemIdSupported: CA ID=0x%04x", CA_system_id);

    if (casProxy == NULL) {
        ALOGE("%s: Could not get CasProxy Service!", __func__);
    }
    else {
        supported = casProxy->isSystemIdSupported(CA_system_id);
    }
    return static_cast<bool>(supported);
}

status_t DescramblerProxyFactory::createPlugin(
        int32_t CA_system_id, DescramblerPlugin** plugin) {
    status_t status = android::BAD_VALUE;
    sp<IMediaCasProxyService> casProxy = CasProxyHelper::getService();

    ALOGV("DescramblerProxyFactory::createPlugin: CA ID=0X%04x", CA_system_id);

    if (casProxy == NULL) {
        ALOGE("%s: Could not get CasProxy Service!", __func__);
    }
    else if (isSystemIdSupported(CA_system_id)) {
        Return<sp<IDescramblerBase>> returnDescrambler = casProxy->createDescrambler(CA_system_id);
        if (!returnDescrambler.isOk()) {
            ALOGE("%s: Could not create descrambler: trans=%s!", __func__,
                    returnDescrambler.description().c_str());
            status = android::NO_INIT;
        }
        else {
            sp<IDescramblerBase> descramblerBase = (sp<IDescramblerBase>) returnDescrambler;
            if (descramblerBase == nullptr) {
                ALOGE("%s: Could not create descrambler (null ptr)!", __func__);
                status = android::NO_INIT;
            }
            else {
                sp<IDescrambler> descrambler = IDescrambler::castFrom(descramblerBase);
                if (descrambler == nullptr) {
                    ALOGE("%s: Could not create descrambler (cast failure)!", __func__);
                    status = android::NO_INIT;
                }
                else {
                    *plugin = new DescramblerProxyPlugin(descrambler);
                    status = android::OK;
                }
            }
        }
    }
    return status;
}

///////////////////////////////////////////////////////////////////////////////

static String8 arrayToString(const std::vector<uint8_t> &array) {
    String8 result;
    for (size_t i = 0; i < array.size(); i++) {
        result.appendFormat("%02x ", array[i]);
    }
    if (result.isEmpty()) {
        result.append("(null)");
    }
    return result;
}

CasProxyPlugin::CasProxyPlugin(sp<ICas> cas) : mICas(cas) {
    ALOGV("%s: CTOR", __func__);
}

CasProxyPlugin::~CasProxyPlugin() {
    ALOGV("%s: DTOR", __func__);
}

status_t CasProxyPlugin::setPrivateData(const CasData &data) {
    status_t ret = android::OK;
    ALOGV("setPrivateData: data=%s", arrayToString(data).string());
    Mutex::Autolock lock(mLock);

    Return<Status> returnStatus = mICas->setPrivateData(data);
    if (!returnStatus.isOk() || returnStatus != Status::OK) {
        ALOGE("Failed to set private data: %s status=%d",
            arrayToString(data).string(), (Status)returnStatus);
        ret = static_cast<status_t>(static_cast<Status>(returnStatus));
    }
    return ret;
}

status_t CasProxyPlugin::openSession(CasSessionId* sessionId) {
    status_t ret = android::OK;
    Status status;

    ALOGV("openSession");
    Mutex::Autolock lock(mLock);

    auto returnVoid = mICas->openSession(
            [&status, &sessionId] (Status _status, const hidl_vec<uint8_t>& _sessionId) {
                status = _status;
                *sessionId = _sessionId;
            });
    if (!returnVoid.isOk() || status != Status::OK) {
        ALOGE("Failed to open session: status=%d", status);
        ret = static_cast<status_t>(status);
    }
    else {
        ALOGV("openSession: sessionId=%s", arrayToString(*sessionId).string());
    }
    return ret;
}

status_t CasProxyPlugin::closeSession(const CasSessionId &sessionId) {
    status_t ret = android::OK;

    ALOGV("closeSession: sessionId=%s", arrayToString(sessionId).string());
    Mutex::Autolock lock(mLock);

    if (sessionId.empty()) {
        ret = android::BAD_VALUE;
    }
    else {
        auto returnStatus = mICas->closeSession(sessionId);
        if (!returnStatus.isOk() || returnStatus != Status::OK) {
            ALOGE("Failed to close sessionId=%s status=%d!", arrayToString(sessionId).string(), (Status)returnStatus);
            ret = static_cast<status_t>(static_cast<Status>(returnStatus));
        }
    }
    return ret;
}

status_t CasProxyPlugin::setSessionPrivateData(
        const CasSessionId &sessionId, const CasData &data) {
    status_t ret = android::OK;

    ALOGV("setSessionPrivateData: sessionId=%s data=%s",
            arrayToString(sessionId).string(), arrayToString(data).string());
    Mutex::Autolock lock(mLock);

    if (sessionId.empty()) {
        ALOGE("%s: Invalid session id!", __func__);
        ret = android::BAD_VALUE;
    }
    else {
        auto returnStatus = mICas->setSessionPrivateData(sessionId, data);
        if (!returnStatus.isOk() || returnStatus != Status::OK) {
            ALOGE("Failed to set sessionId=%s status=%d",
                arrayToString(sessionId).string(), (Status)returnStatus);
            ret = static_cast<status_t>(static_cast<Status>(returnStatus));
        }
    }
    return ret;
}

status_t CasProxyPlugin::processEcm(
        const CasSessionId &sessionId, const CasEcm& ecm) {
    status_t ret = android::OK;

    ALOGV("processEcm: sessionId=%s ECM=%s",
        arrayToString(sessionId).string(),
        arrayToString(ecm).string());
    Mutex::Autolock lock(mLock);

    if (sessionId.empty()) {
        ALOGE("%s: Invalid session id!", __func__);
        ret = android::BAD_VALUE;
    }
    else {
        auto returnStatus = mICas->processEcm(sessionId, ecm);
        if (!returnStatus.isOk() || returnStatus != Status::OK) {
            ALOGE("Failed to process ECM for sessionId=%s status=%d",
                arrayToString(sessionId).string(), (Status)returnStatus);
            ret = static_cast<status_t>(static_cast<Status>(returnStatus));
        }
    }
    return ret;
}

status_t CasProxyPlugin::processEmm(const CasEmm& emm) {
    status_t ret = android::OK;

    ALOGV("processEmm EMM=%s", arrayToString(emm).string());
    Mutex::Autolock lock(mLock);

    auto returnStatus = mICas->processEmm(emm);
    if (!returnStatus.isOk() || returnStatus != Status::OK) {
        ALOGE("Failed to process EMM private data: status=%d", (Status)returnStatus);
        ret = static_cast<status_t>(static_cast<Status>(returnStatus));
    }
    return ret;
}

status_t CasProxyPlugin::sendEvent(
        int32_t event, int arg, const CasData &eventData) {
    status_t ret = android::OK;

    ALOGV("sendEvent: event=%d, arg=%d, data=%s", event, arg, arrayToString(eventData).string());
    Mutex::Autolock lock(mLock);

    auto returnStatus = mICas->sendEvent(event, arg, eventData);
    if (!returnStatus.isOk() || returnStatus != Status::OK) {
        ALOGE("Failed to send event %d status=%d!", event, (Status)returnStatus);
        ret = static_cast<status_t>(static_cast<Status>(returnStatus));
    }
    return ret;
}

status_t CasProxyPlugin::provision(const String8 &str) {
    ALOGV("provision: provisionString=%s", str.string());
    Mutex::Autolock lock(mLock);

    auto returnStatus = mICas->provision(hidl_string(str.string()));
    if (!returnStatus.isOk() || (Status) returnStatus != Status::OK) {
        ALOGE("provision: could not provision CAS plugin! [%d]", (Status)returnStatus);
    }
    return static_cast<status_t>(static_cast<Status>(returnStatus));
}

status_t CasProxyPlugin::refreshEntitlements(
        int32_t refreshType, const CasData &refreshData) {
    status_t ret = android::OK;

    Mutex::Autolock lock(mLock);
    ALOGV("refreshEntitlements: refreshType=%d refreshData=%s", refreshType, arrayToString(refreshData).string());

    auto returnStatus = mICas->refreshEntitlements(refreshType, refreshData);
    if (!returnStatus.isOk() || returnStatus != Status::OK) {
        ALOGE("Failed to refresh entitlements status=%d", (Status)returnStatus);
        ret = static_cast<status_t>(static_cast<Status>(returnStatus));
    }
    return ret;
}

/////////////////////////////////////////////////////////////////

DescramblerProxyPlugin::DescramblerProxyPlugin(sp<IDescrambler> descrambler) : mDescrambler(descrambler) {
    ALOGV("%s: CTOR", __func__);
}

DescramblerProxyPlugin::~DescramblerProxyPlugin() {
    ALOGV("%s: DTOR", __func__);
}

bool DescramblerProxyPlugin::requiresSecureDecoderComponent(
        const char *mime) const {

    ALOGV("DescramblerProxyPlugin::requiresSecureDecoderComponent"
            "(mime=%s)", mime);

    Return<bool> returnStatus = mDescrambler->requiresSecureDecoderComponent(hidl_string(mime));
    if (!returnStatus.isOk()) {
        return false;
    }
    return returnStatus;
}

status_t DescramblerProxyPlugin::setMediaCasSession(
        const CasSessionId &sessionId) {
    status_t ret = android::OK;

    ALOGV("DescramblerProxyPlugin::setMediaCasSession: sessionId=%s", arrayToString(sessionId).string());
    Mutex::Autolock lock(mLock);

    if (sessionId.empty()) {
        ret = android::BAD_VALUE;
    }
    else {
        auto returnStatus = mDescrambler->setMediaCasSession(sessionId);
        if (!returnStatus.isOk() || returnStatus != Status::OK) {
            ALOGE("setMediaCasSession: failed to set session sessionId=%s status=%d!", arrayToString(sessionId).string(), (Status)returnStatus);
            ret = static_cast<status_t>(static_cast<Status>(returnStatus));
        }
    }
    return ret;
}

ssize_t DescramblerProxyPlugin::descramble(
        bool secure,
        ScramblingControl scramblingControl,
        size_t numSubSamples,
        const SubSample *subSamples,
        const void *srcPtr,
        int32_t srcOffset,
        void *dstPtr,
        int32_t dstOffset,
        AString * /* errorDetailMsg*/) {
    ALOGV("DescramblerProxyPlugin::descramble(secure=%d, sctrl=0x%08x,"
          "subSamples=%s, srcPtr=%p, dstPtr=%p, srcOffset=%d, dstOffset=%d)",
          (int)secure, (int)scramblingControl,
          subSamplesToString(subSamples, numSubSamples).string(),
          srcPtr, dstPtr, srcOffset, dstOffset);
    Mutex::Autolock lock(mLock);

    Status status = Status::OK;
    uint32_t bytesWritten = 0;
    size_t actualSize = 0;

    // Calculate how much space is required to transfer the sub-samples
    for (unsigned subSample = 0; subSample < numSubSamples; subSample++) {
        actualSize += subSamples[subSample].mNumBytesOfClearData;
        actualSize += subSamples[subSample].mNumBytesOfEncryptedData;
    }
    if (actualSize > 0) {
        // Ensure Cache line alignment
        size_t alignment = MemoryDealer::getAllocationAlignment();
        size_t requiredSize = (actualSize + (alignment - 1)) & ~(alignment - 1);
        ssize_t offset;
        size_t size;
        hidl_string detailedError;
        hidl_vec<android::hardware::cas::native::V1_0::SubSample> vecSubSamples;

        // Align to multiples of 64K.
        requiredSize = (requiredSize + (64*1024)) & ~(64*1024);

        ALOGV("DescramblerProxyPlugin::descramble: srcPtr=%p, dstPtr=%p, input size=%zu, required size=%zu",
                srcPtr, dstPtr, actualSize, requiredSize);

        sp<MemoryDealer> memDealer = new MemoryDealer(requiredSize, "CasProxyPlugin");

        if (memDealer == NULL) {
            ALOGE("DescramblerProxyPlugin::descramble: ERROR could not obtain memory dealer!");
            return ERROR_CAS_CANNOT_HANDLE;
        }
        sp<IMemory> mem = memDealer->allocate(requiredSize);
        if (mem == NULL) {
            ALOGE("DescramblerProxyPlugin::descramble: ERROR could not allocate memory!");
            return ERROR_CAS_CANNOT_HANDLE;
        }
        sp<IMemoryHeap> heap = mem->getMemory(&offset, &size);

        if (heap == NULL) {
            ALOGE("DescramblerProxyPlugin::descramble: ERROR could not get memory!");
            return ERROR_CAS_CANNOT_HANDLE;
        }

        ALOGV("DescramblerProxyPlugin::descramble: memcpyIn dst=%p, src=%p, srcOffset=%d, size=%zu (input size=%zu)",
               mem->pointer(), (uint8_t*)srcPtr+srcOffset,  srcOffset, mem->size(), actualSize);
        memcpy(mem->pointer(), (uint8_t*)srcPtr+srcOffset, mem->size());

        sp<HidlMemory> hidlMem = fromHeap(heap);
        hardware::cas::native::V1_0::SharedBuffer srcBuffer = {
            .heapBase = *hidlMem,
            .offset = (uint64_t)offset,
            .size = (uint64_t)size
        };

        DestinationBuffer dstBuffer;
        if (secure) {
            dstBuffer.type = BufferType::NATIVE_HANDLE;
            dstBuffer.secureMemory = hidl_handle(static_cast<native_handle_t *>(dstPtr));
        }
        else {
            dstBuffer.type = BufferType::SHARED_MEMORY;
            dstBuffer.nonsecureMemory = srcBuffer;
        }

        vecSubSamples.setToExternal((android::hardware::cas::native::V1_0::SubSample *)subSamples, numSubSamples);
        auto returnVoid = mDescrambler->descramble(
                (android::hardware::cas::native::V1_0::ScramblingControl)scramblingControl,
                vecSubSamples,
                srcBuffer,
                0, //srcOffset,
                dstBuffer,
                0, //dstOffset,
                [&status, &bytesWritten, &detailedError] (
                        Status _status, uint32_t _bytesWritten,
                        const hidl_string& _detailedError) {
                    status = _status;
                    bytesWritten = _bytesWritten;
                    detailedError = _detailedError;
                });

        if (!returnVoid.isOk() || status != Status::OK) {
            ALOGE("DescramblerProxyPlugin::descramble descramble failed, trans=%s",
                    returnVoid.description().c_str());
            return ERROR_CAS_CANNOT_HANDLE;
        }

        // For non-secure memory, copy the destination bytes back to the original dstPtr
        if (!secure && dstPtr != nullptr) {
            ALOGV("DescramblerProxyPlugin::descramble: memcpyOut dst=%p, dstOffset=%d, src=%p, size=%u",
                    (uint8_t *)dstPtr+dstOffset, dstOffset, mem->pointer(), bytesWritten);
            memcpy((uint8_t *)dstPtr+dstOffset, mem->pointer(), bytesWritten);
        }
    }
    return bytesWritten;
}

String8 DescramblerProxyPlugin::subSamplesToString(
        SubSample const *subSamples, size_t numSubSamples) const
{
    String8 result;
    for (size_t i = 0; i < numSubSamples; i++) {
        result.appendFormat("[%zu] {clear:%u, encrypted:%u} ", i,
                            subSamples[i].mNumBytesOfClearData,
                            subSamples[i].mNumBytesOfEncryptedData);
    }
    return result;
}

} // namespace android

