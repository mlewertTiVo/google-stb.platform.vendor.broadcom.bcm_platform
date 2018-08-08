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

//#define LOG_NDEBUG 0
#define LOG_TAG "bcm-casp"
#include <utils/Log.h>

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>

#include "ClearKeySessionLibrary.h"

#include "bkni.h"

namespace android {
namespace clearkeycas {

Mutex ClearKeySessionLibrary::sSingletonLock;
ClearKeySessionLibrary* ClearKeySessionLibrary::sSingleton = NULL;

ClearKeyCasSession::ClearKeyCasSession(CasPlugin *plugin)
    : mPlugin(plugin) {
#if (NEXUS_SECURITY_API_VERSION==1)
    BKNI_Memset(&NexusKey, 0x00, sizeof(NexusKey));
#endif
    BKNI_Memset(&desc, 0x00, sizeof(desc));
    mKeyInfo[0].valid = mKeyInfo[1].valid = false;
}

ClearKeyCasSession::~ClearKeyCasSession() {
}

ClearKeySessionLibrary* ClearKeySessionLibrary::get() {
    Mutex::Autolock lock(sSingletonLock);

    if (sSingleton == NULL) {
        ALOGV("Instantiating Session Library Singleton.");
        sSingleton = new ClearKeySessionLibrary();
    }

    return sSingleton;
}

ClearKeySessionLibrary::ClearKeySessionLibrary() : mNextSessionId(1) {
    mpNxWrap = new NxWrap("ClearkeyCas");

    if (mpNxWrap == NULL) {
        ALOGE("%s: Could not create Nexux Client!!!", __FUNCTION__);
    }

    mpNxWrap->join();
}
ClearKeySessionLibrary::~ClearKeySessionLibrary() {
    if (mpNxWrap != NULL) {
        mpNxWrap->leave();
       delete mpNxWrap;
       mpNxWrap = NULL;
    }
}

status_t ClearKeySessionLibrary::addSession(
        CasPlugin *plugin, CasSessionId *sessionId) {
    CHECK(sessionId);
    status_t status;

    Mutex::Autolock lock(mSessionsLock);

    std::shared_ptr<ClearKeyCasSession> session(new ClearKeyCasSession(plugin));

    status = session->initSession();
    if (status != OK) {
        session->unInitSession();
        return status;
    }

    uint8_t *byteArray = (uint8_t *) &mNextSessionId;
    sessionId->push_back(byteArray[3]);
    sessionId->push_back(byteArray[2]);
    sessionId->push_back(byteArray[1]);
    sessionId->push_back(byteArray[0]);
    mNextSessionId++;

    mIDToSessionMap.add(*sessionId, session);
    return OK;
}

std::shared_ptr<ClearKeyCasSession> ClearKeySessionLibrary::findSession(
        const CasSessionId& sessionId) {
    Mutex::Autolock lock(mSessionsLock);

    ssize_t index = mIDToSessionMap.indexOfKey(sessionId);
    if (index < 0) {
        return NULL;
    }
    return mIDToSessionMap.valueFor(sessionId);
}

void ClearKeySessionLibrary::destroySession(const CasSessionId& sessionId) {
    Mutex::Autolock lock(mSessionsLock);

    ssize_t index = mIDToSessionMap.indexOfKey(sessionId);
    if (index < 0) {
        return;
    }

    std::shared_ptr<ClearKeyCasSession> session = mIDToSessionMap.valueAt(index);
    session->unInitSession();
    mIDToSessionMap.removeItemsAt(index);
}

void ClearKeySessionLibrary::destroyPlugin(CasPlugin *plugin) {
    Mutex::Autolock lock(mSessionsLock);

    for (ssize_t index = (ssize_t)mIDToSessionMap.size() - 1; index >= 0; index--) {
        std::shared_ptr<ClearKeyCasSession> session = mIDToSessionMap.valueAt(index);
        if (session->getPlugin() == plugin) {
            session->unInitSession();
            mIDToSessionMap.removeItemsAt(index);
        }
    }
}

} // namespace clearkeycas
} // namespace android
