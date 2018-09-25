/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CLEARKEY_SESSION_LIBRARY_H_
#define CLEARKEY_SESSION_LIBRARY_H_

#include <media/cas/CasAPI.h>
#include <media/cas/DescramblerAPI.h>
#include <media/stagefright/foundation/ABase.h>
#include <openssl/aes.h>
#include <utils/KeyedVector.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include "nexus_platform.h"
#include "nexus_dma.h"
#include "nexus_memory.h"
#include "nexus_security.h"
#include "nexus_keyladder.h"
#include "nexus_recpump.h"
#include "nexus_pid_channel.h"
#include "nexus_parser_band.h"
#include "nxclient.h"
#include <nxwrap.h>

namespace android {
struct ABuffer;

namespace dvbcsacas {

class DvbCsaCasSession : public RefBase {
public:
    explicit DvbCsaCasSession(CasPlugin *plugin);

    virtual ~DvbCsaCasSession();

    status_t initSession();

    void unInitSession();

    ssize_t decrypt(
            bool secure,
            const DescramblerPlugin::SubSample *subSamples,
            const void *srcPtr,
            void *dstPtr,
            AString * /* errorDetailMsg */);

private:
    Mutex mKeyLock;
    CasPlugin* mPlugin;
    NEXUS_KeySlotHandle keyslotHandle = NULL;
#if (NEXUS_SECURITY_API_VERSION==1)
    NEXUS_SecurityClearKey NexusKey;
#else
    NEXUS_KeySlotKey slotKey = NULL;
#endif

    NEXUS_DmaHandle dma = NULL;
    BKNI_EventHandle index_event = NULL;
    BKNI_EventHandle last_event = NULL;
    BKNI_EventHandle dma_event = NULL;

    struct Pump {
        NEXUS_PlaypumpHandle playpump;
        NEXUS_RecpumpHandle recpump;
        NEXUS_PlaypumpScatterGatherDescriptor desc;
        uint8_t *copyBuffer;
    };

    Pump indexPump;
    Pump decryptPump;

    friend class DvbCsaSessionLibrary;

    CasPlugin* getPlugin() const { return mPlugin; }
    status_t decryptPayload(
            const char* key, size_t length, size_t offset, char* buffer) const;

    status_t initPump(struct Pump *pump, bool index);
    void unInitPump(struct Pump *pump);
    void parse_itb_buffer(void * dstPtr, const void *buffer, unsigned size, int *ts_header_size);
    size_t create_ts_packet(void * dstPtr, const void *buffer, unsigned size, const void *wrap_buffer, unsigned wrap_size);
    ssize_t pumpDecrypt(uint8_t *dst, uint8_t *src, size_t numBytes, bool index);
    void setDmaJobBlockSettings(NEXUS_DmaJobBlockSettings *blockSettings, void *destination, const void *source, size_t num);
    NEXUS_Error dmaProcessBlocks(NEXUS_DmaJobBlockSettings *blockSettings, int numBlocks);

    DISALLOW_EVIL_CONSTRUCTORS(DvbCsaCasSession);
};

class DvbCsaSessionLibrary {
public:
    static DvbCsaSessionLibrary* get();

    status_t addSession(CasPlugin *plugin, CasSessionId *sessionId);

    std::shared_ptr<DvbCsaCasSession> findSession(const CasSessionId& sessionId);

    void destroySession(const CasSessionId& sessionId);

    void destroyPlugin(CasPlugin *plugin);

private:
    static Mutex sSingletonLock;
    static DvbCsaSessionLibrary* sSingleton;

    Mutex mSessionsLock;
    uint32_t mNextSessionId;
    KeyedVector<CasSessionId, std::shared_ptr<DvbCsaCasSession>> mIDToSessionMap;
    NxWrap *mpNxWrap = NULL;

    DvbCsaSessionLibrary();
    ~DvbCsaSessionLibrary();
    DISALLOW_EVIL_CONSTRUCTORS(DvbCsaSessionLibrary);
};

} // namespace dvbcsacas
} // namespace android

#endif // CLEARKEY_SESSION_LIBRARY_H_
