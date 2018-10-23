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

#include "bstd.h"
#include "berr.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "nexus_platform.h"
#include "nexus_dma.h"
#include "nexus_memory.h"
#include "nexus_security.h"
#include "nexus_keyladder.h"
#include "nexus_recpump.h"
#include "nexus_pid_channel.h"
#include "nexus_parser_band.h"
#include "nxclient.h"
#include "ClearKeyFetcher.h"
#include "ecm.h"
#include "ClearKeyLicenseFetcher.h"
#include "ClearKeyCasPlugin.h"
#include "ClearKeySessionLibrary.h"
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/Log.h>
#include <cutils/native_handle.h>
#include "bomx_secure_buff.h"

android::CasFactory* createCasFactory() {
    return new android::clearkeycas::ClearKeyCasFactory();
}

android::DescramblerFactory *createDescramblerFactory()
{
    return new android::clearkeycas::ClearKeyDescramblerFactory();
}

namespace android {
namespace clearkeycas {

#define SYSTEM_ID (0xF6D8) /* Google's CA system ID, referenced in the PMT->program_info->CA_descriptor */
#define COPY_BUFFER_SIZE (2 * 1024 * 1024) /* May need to be dynamic... */
#define PARSE_BUFFER_SIZE (4136) /* Smallest size supported by recpump */
#define PAYLOAD_SIZE (184) /* Recpump payload granularity */
#define RECPUMP_ITB_TIMEOUT_MS (10) /* Timeout for recpump ITB data containing PES metadata */
#define KEY_SIZE (16)

bool ClearKeyCasFactory::isSystemIdSupported(int32_t CA_system_id) const {
    return CA_system_id == SYSTEM_ID;
}

status_t ClearKeyCasFactory::queryPlugins(
        std::vector<CasPluginDescriptor> *descriptors) const {
    descriptors->clear();
    descriptors->push_back({SYSTEM_ID, String8("Broadcom Clear Key CAS")});
    return OK;
}

status_t ClearKeyCasFactory::createPlugin(
        int32_t CA_system_id,
        void *appData,
        CasPluginCallback callback,
        CasPlugin **plugin) {
    if (!isSystemIdSupported(CA_system_id)) {
        return BAD_VALUE;
    }

    *plugin = new ClearKeyCasPlugin(appData, callback);
    return OK;
}
///////////////////////////////////////////////////////////////////////////////
bool ClearKeyDescramblerFactory::isSystemIdSupported(
        int32_t CA_system_id) const {
    return CA_system_id == SYSTEM_ID;
}

status_t ClearKeyDescramblerFactory::createPlugin(
        int32_t CA_system_id, DescramblerPlugin** plugin) {
    if (!isSystemIdSupported(CA_system_id)) {
        return BAD_VALUE;
    }

    *plugin = new ClearKeyDescramblerPlugin();
    return OK;
}

///////////////////////////////////////////////////////////////////////////////
ClearKeyCasPlugin::ClearKeyCasPlugin(
        void *appData, CasPluginCallback callback)
    : mCallback(callback), mAppData(appData) {
    ALOGV("CTOR");
}

ClearKeyCasPlugin::~ClearKeyCasPlugin() {
    ALOGV("DTOR");
    ClearKeySessionLibrary::get()->destroyPlugin(this);
}

status_t ClearKeyCasPlugin::setPrivateData(const CasData &/*data*/) {
    ALOGV("setPrivateData");

    return OK;
}

static String8 sessionIdToString(const std::vector<uint8_t> &array) {
    String8 result;
    for (size_t i = 0; i < array.size(); i++) {
        result.appendFormat("%02x ", array[i]);
    }
    if (result.isEmpty()) {
        result.append("(null)");
    }
    return result;
}

status_t ClearKeyCasPlugin::openSession(CasSessionId* sessionId) {
    ALOGV("openSession");

    return ClearKeySessionLibrary::get()->addSession(this, sessionId);
}

status_t ClearKeyCasPlugin::closeSession(const CasSessionId &sessionId) {
    ALOGV("closeSession: sessionId=%s", sessionIdToString(sessionId).string());
    std::shared_ptr<ClearKeyCasSession> session =
            ClearKeySessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    ClearKeySessionLibrary::get()->destroySession(sessionId);
    return OK;
}

status_t ClearKeyCasPlugin::setSessionPrivateData(
        const CasSessionId &sessionId, const CasData & /*data*/) {
    ALOGV("setSessionPrivateData: sessionId=%s",
            sessionIdToString(sessionId).string());
    std::shared_ptr<ClearKeyCasSession> session =
            ClearKeySessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }
    return OK;
}

status_t ClearKeyCasPlugin::processEcm(
        const CasSessionId &sessionId, const CasEcm& ecm) {
    ALOGV("processEcm: sessionId=%s", sessionIdToString(sessionId).string());
    std::shared_ptr<ClearKeyCasSession> session =
            ClearKeySessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    Mutex::Autolock lock(mKeyFetcherLock);

    return session->updateECM(mKeyFetcher.get(), (void*)ecm.data(), ecm.size());
}

status_t ClearKeyCasPlugin::processEmm(const CasEmm& /*emm*/) {
    ALOGV("processEmm");
    Mutex::Autolock lock(mKeyFetcherLock);

    return OK;
}

status_t ClearKeyCasPlugin::sendEvent(
        int32_t event, int32_t arg, const CasData &eventData) {
    ALOGV("sendEvent: event=%d, arg=%d", event, arg);
    // Echo the received event to the callback.
    // Clear key plugin doesn't use any event, echo'ing for testing only.
    if (mCallback != NULL) {
        mCallback((void*)mAppData, event, arg, (uint8_t*)eventData.data(), eventData.size());
    }
    return OK;
}

status_t ClearKeyCasPlugin::provision(const String8 &str) {
    ALOGV("provision: provisionString=%s", str.string());
    Mutex::Autolock lock(mKeyFetcherLock);

    std::unique_ptr<ClearKeyLicenseFetcher> license_fetcher;
    license_fetcher.reset(new ClearKeyLicenseFetcher());
    status_t err = license_fetcher->Init(str.string());
    if (err != OK) {
        ALOGE("provision: failed to init ClearKeyLicenseFetcher (err=%d)", err);
        return err;
    }

    std::unique_ptr<ClearKeyFetcher> key_fetcher;
    key_fetcher.reset(new ClearKeyFetcher(std::move(license_fetcher)));
    err = key_fetcher->Init();
    if (err != OK) {
        ALOGE("provision: failed to init ClearKeyFetcher (err=%d)", err);
        return err;
    }

    ALOGV("provision: using ClearKeyFetcher");
    mKeyFetcher = std::move(key_fetcher);

    return OK;
}

status_t ClearKeyCasPlugin::refreshEntitlements(
        int32_t refreshType, const CasData &/*refreshData*/) {
    ALOGV("refreshEntitlements: refreshType=%d", refreshType);
    Mutex::Autolock lock(mKeyFetcherLock);

    return OK;
}

static void nexus_callback(void *pParam, int iParam)
{
    BKNI_EventHandle event = (BKNI_EventHandle) pParam;
    BSTD_UNUSED(iParam);
    BKNI_SetEvent(event);
}

static void unused_nexus_callback(void *context, int param)
{
    BSTD_UNUSED(context);
    BSTD_UNUSED(param);
}

status_t ClearKeyCasSession::initSession()
{
    NEXUS_RecpumpOpenSettings recpumpOpenSettings;
    NEXUS_PlaypumpOpenPidChannelSettings pidChannelSettings;
    NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_RecpumpSettings recpumpSettings;
    NEXUS_RecpumpAddPidChannelSettings addPidChannelSettings;
    NEXUS_MemoryAllocationSettings memoryAllocationSettings;
    NEXUS_HeapHandle secureHeap;
    NEXUS_Error rc;
    status_t status = ERROR_CAS_SESSION_NOT_OPENED;

    ALOGV("Open session");

#if (NEXUS_SECURITY_API_VERSION==1)
    NEXUS_SecurityKeySlotSettings keySettings;
    NEXUS_SecurityAlgorithmSettings NexusConfig;

    NEXUS_Security_GetDefaultKeySlotSettings(&keySettings);
    keySettings.keySlotEngine = NEXUS_SecurityEngine_eM2m;
    hKeySlot = NEXUS_Security_AllocateKeySlot(&keySettings);
    if (!hKeySlot) {
        ALOGE("NEXUS_Security_AllocateKeySlot failed");
        return status;
    }

    /* Set up decryption key */
    NEXUS_Security_GetDefaultAlgorithmSettings(&NexusConfig);
    NexusConfig.algorithm           = NEXUS_SecurityAlgorithm_eAes128;
    NexusConfig.algorithmVar        = NEXUS_SecurityAlgorithmVariant_eCbc;
    NexusConfig.terminationMode     = NEXUS_SecurityTerminationMode_eCtsCpcm;
    NexusConfig.keyDestEntryType    = NEXUS_SecurityKeyType_eClear;
    NexusConfig.solitarySelect      = NEXUS_SecuritySolitarySelect_eClear;
    NexusConfig.ivMode              = NEXUS_SecurityIVMode_eRegular;
    NexusConfig.aesCounterSize      = NEXUS_SecurityAesCounterSize_e128Bits;
    NexusConfig.operation           = NEXUS_SecurityOperation_eDecrypt;
    if (NEXUS_Security_ConfigAlgorithm( hKeySlot, &NexusConfig) != 0)
    {
        ALOGE("NEXUS_Security_ConfigAlgorithm failed");
        return status;
    }

    NEXUS_Security_GetDefaultClearKey( &NexusKey );
    NexusKey.keyEntryType = NEXUS_SecurityKeyType_eClear;
    NexusKey.keyIVType    = NEXUS_SecurityKeyIVType_eIV;
    NexusKey.keySize = KEY_SIZE;
    if (NEXUS_Security_LoadClearKey( hKeySlot, &NexusKey) != 0) {
        ALOGE("NEXUS_Security_LoadClearKey failed");
        return status;
    }
#else
    NEXUS_KeySlotAllocateSettings keySettings;
    NEXUS_KeySlotSettings keyslotSettings;
    NEXUS_KeySlotEntrySettings keyslotEntrySettings;
    NEXUS_KeySlot_GetDefaultAllocateSettings(&keySettings);
    NEXUS_KeySlotIv slotIv;

    keySettings.useWithDma = true;
    hKeySlot = NEXUS_KeySlot_Allocate(&keySettings);
    if(!hKeySlot) {
        ALOGE("NEXUS_KeySlot_Allocate failed");
        return status;
    }

    NEXUS_KeySlot_GetSettings(hKeySlot, &keyslotSettings);
    rc = NEXUS_KeySlot_SetSettings(hKeySlot, &keyslotSettings);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetSettings failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NEXUS_KeySlot_GetEntrySettings(hKeySlot, NEXUS_KeySlotBlockEntry_eCpsClear, &keyslotEntrySettings);
    keyslotEntrySettings.algorithm           = NEXUS_CryptographicAlgorithm_eAes128;
    keyslotEntrySettings.algorithmMode       = NEXUS_CryptographicAlgorithmMode_eCbc;
    keyslotEntrySettings.terminationMode     = NEXUS_KeySlotTerminationMode_eCtsDvbCpcm;
    keyslotEntrySettings.solitaryMode        = NEXUS_KeySlotTerminationSolitaryMode_eClear;
    keyslotEntrySettings.counterSize         = 128;
    keyslotEntrySettings.rPipeEnable         = true;
    keyslotEntrySettings.gPipeEnable         = true;
    rc = NEXUS_KeySlot_SetEntrySettings(hKeySlot, NEXUS_KeySlotBlockEntry_eCpsClear, &keyslotEntrySettings);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetEntrySettings failed");
        return status;
    }

    slotIv.size = KEY_SIZE;
    BKNI_Memset(slotIv.iv, 0x00, slotIv.size);
    rc = NEXUS_KeySlot_SetEntryIv(hKeySlot, NEXUS_KeySlotBlockEntry_eCpsClear, &slotIv, NULL);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetEntryIv failed");
        return status;
    }

    slotKey.size = KEY_SIZE;
#endif

    /* Open DMA handle */
    dma = NEXUS_Dma_Open( 0, NULL );
    if (!dma) {
        ALOGE("NEXUS_Dma_Open failed");
        return status;
    }

    NEXUS_Memory_GetDefaultAllocationSettings(&memoryAllocationSettings);
    rc = NEXUS_Memory_Allocate(COPY_BUFFER_SIZE, &memoryAllocationSettings, (void **) &copyBuffer);
    if (rc) {
        ALOGE("NEXUS_Memory_Allocate failed");
        return status;
    }

    secureHeap = NEXUS_Heap_Lookup(NEXUS_HeapLookupType_eCompressedRegion);
    memoryAllocationSettings.heap = secureHeap;
    rc = NEXUS_Memory_Allocate(PARSE_BUFFER_SIZE, &memoryAllocationSettings, (void **) &parseBuffer);
    if (rc) {
        ALOGE("NEXUS_Memory_Allocate failed");
        return status;
    }

    /* Never changes */
    desc.addr = parseBuffer;

    BKNI_CreateEvent(&event);

    /* set up the source */
    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    playpumpOpenSettings.heap = secureHeap;
    playpumpOpenSettings.dataNotCpuAccessible = true;
    playpumpOpenSettings.memory = NEXUS_MemoryBlock_FromAddress(parseBuffer);
    playpumpOpenSettings.fifoSize = PARSE_BUFFER_SIZE;
    playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
    if (!playpump) {
        ALOGE("NEXUS_Memory_Allocate failed");
        return status;
    }

    NEXUS_Playpump_GetSettings(playpump, &playpumpSettings);
    playpumpSettings.dataCallback.callback = unused_nexus_callback;
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    playpumpSettings.allPass = true;
    rc = NEXUS_Playpump_SetSettings(playpump, &playpumpSettings);
    if (rc) {
        ALOGE("NEXUS_Playpump_SetSettings failed");
        return status;
    }

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
    rc = NEXUS_Playpump_GetAllPassPidChannelIndex(playpump, &pidChannelSettings.pidSettings.pidChannelIndex);
    if (rc) {
        ALOGE("NEXUS_Playpump_GetAllPassPidChannelIndex failed");
        return status;
    }

    pidChannel = NEXUS_Playpump_OpenPidChannel(playpump, 0, &pidChannelSettings);
    if (!pidChannel) {
        ALOGE("NEXUS_Playpump_OpenPidChannel failed");
        return status;
    }

    rc = NEXUS_SetPidChannelBypassKeyslot(pidChannel, NEXUS_BypassKeySlot_eGR2R);
    if (rc) {
        ALOGE("NEXUS_Playpump_GetAllPassPidChannelIndex failed");
        return status;
    }

    NEXUS_Recpump_GetDefaultOpenSettings(&recpumpOpenSettings);
    recpumpOpenSettings.data.heap = secureHeap;
    recpumpOpenSettings.data.bufferSize = PARSE_BUFFER_SIZE;
    recpumpOpenSettings.data.dataReadyThreshold = 0;
    recpumpOpenSettings.index.dataReadyThreshold = 0;
    recpump = NEXUS_Recpump_Open(NEXUS_ANY_ID, &recpumpOpenSettings);
    if (!recpump) {
        ALOGE("NEXUS_Recpump_Open failed");
        return status;
    }

    NEXUS_Recpump_GetSettings(recpump, &recpumpSettings);
    recpumpSettings.data.dataReady.callback = unused_nexus_callback;
    recpumpSettings.index.dataReady.callback = nexus_callback;
    recpumpSettings.index.dataReady.context = event;
    recpumpSettings.data.overflow.callback = unused_nexus_callback;
    recpumpSettings.index.overflow.callback = unused_nexus_callback;
    recpumpSettings.bandHold = NEXUS_RecpumpFlowControl_eDisable;
    rc = NEXUS_Recpump_SetSettings(recpump, &recpumpSettings);
    if (rc) {
        ALOGE("NEXUS_Recpump_SetSettings failed");
        return status;
    }

    NEXUS_Recpump_GetDefaultAddPidChannelSettings(&addPidChannelSettings);
    addPidChannelSettings.pidType = NEXUS_PidType_eOther;
    addPidChannelSettings.pidTypeSettings.other.index = true;
    rc = NEXUS_Recpump_AddPidChannel(recpump, pidChannel, &addPidChannelSettings);
    if (rc) {
        ALOGE("NEXUS_Recpump_AddPidChannel failed");
        return status;
    }

    rc = NEXUS_Recpump_Start(recpump);
    if (rc) {
        ALOGE("NEXUS_Recpump_Start failed");
        return status;
    }

    rc = NEXUS_Playpump_Start(playpump);
    if (rc) {
        ALOGE("NEXUS_Playpump_Start failed");
        return status;
    }

    return OK;
}

void ClearKeyCasSession::unInitSession() {
    ALOGV("Close session");

    if (playpump)
        NEXUS_Playpump_Stop(playpump);

    if (recpump) {
        NEXUS_Recpump_Stop(recpump);
        NEXUS_Recpump_RemoveAllPidChannels(recpump);
    }

    if (playpump) {
        if (pidChannel)
            NEXUS_Playpump_ClosePidChannel(playpump, pidChannel);

        NEXUS_Playpump_Close(playpump);
    }

    if (recpump) {
        NEXUS_Recpump_Close(recpump);
    }

    if (event)
        BKNI_DestroyEvent(event);

    if (parseBuffer)
        NEXUS_Memory_Free(parseBuffer);

    if(copyBuffer)
        NEXUS_Memory_Free(copyBuffer);

    if (dma)
        NEXUS_Dma_Close(dma);

    if (hKeySlot) {
#if (NEXUS_SECURITY_API_VERSION==1)
        NEXUS_Security_FreeKeySlot(hKeySlot);
#else
        NEXUS_KeySlot_Invalidate(hKeySlot);
        NEXUS_KeySlot_Free(hKeySlot);
#endif
    }
}

// PES header and ECM stream header layout
//
// processECM() receives the data_byte portion from the transport packet.
// Below is the layout of the first 16 bytes of the ECM PES packet. Here
// we don't parse them, we skip them and go to the ECM container directly.
// The layout is included here only for reference.
//
// 0-2:   0x00 00 01 = start code prefix.
// 3:     0xf0 = stream type (90 = ECM).
// 4-5:   0x00 00 = PES length (filled in later, this is the length of the
//                  PES header (16) plus the length of the ECM container).
// 6-7:   0x00 00 = ECM major version.
// 8-9:   0x00 01 = ECM minor version.
// 10-11: 0x00 00 = Crypto period ID (filled in later).
// 12-13: 0x00 00 = ECM container length (filled in later, either 84 or
// 166).
// 14-15: 0x00 00 = offset = 0.

status_t ClearKeyCasSession::updateECM(
        KeyFetcher *keyFetcher, void *ecm, size_t size) {
    if (keyFetcher == nullptr) {
        return ERROR_CAS_NOT_PROVISIONED;
    }

    if (size < KEY_SIZE) {
        ALOGE("updateECM: invalid ecm size %zu", size);
        return BAD_VALUE;
    }

    Mutex::Autolock _lock(mKeyLock);

    if (mEcmBuffer != NULL && mEcmBuffer->capacity() == size
            && !memcmp(mEcmBuffer->base(), ecm, size)) {
        return OK;
    }

    mEcmBuffer = ABuffer::CreateAsCopy(ecm, size);
    mEcmBuffer->setRange(KEY_SIZE, size - KEY_SIZE);

    uint64_t asset_id;
    std::vector<KeyFetcher::KeyInfo> keys;
    status_t err = keyFetcher->ObtainKey(mEcmBuffer, &asset_id, &keys);
    if (err != OK) {
        ALOGE("updateECM: failed to obtain key (err=%d)", err);
        return err;
    }

    ALOGV("updateECM: %zu key(s) found", keys.size());
    for (size_t keyIndex = 0; keyIndex < keys.size(); keyIndex++) {
        String8 str;
        const sp<ABuffer>& keyBytes = keys[keyIndex].key_bytes;
        CHECK(keyBytes->size() == KEY_SIZE);

        memcpy(mKeyInfo[keyIndex].contentKey, reinterpret_cast<const uint8_t*>(keyBytes->data()), keyBytes->size());
        mKeyInfo[keyIndex].valid = true;
    }
    return OK;
}

typedef struct scd
{
/*WORD 0*/
    uint8_t    sc_cont_count:4;
    uint8_t    sc_adapt_ctrl:2;
    uint8_t    sc_scram_ctrl:2;

    uint8_t    sc_tpid_l;  // low 8 bits of tpid

    uint8_t    sc_tpid_h:5;  // high 5 bits of tpid
    bool       sc_pri:1;
    bool       sc_pusi:1;
    bool       sc_tei:1;

    uint8_t    tf_entry_type;

/*WORD 1*/
    uint8_t    sc_timestamp[4];
/*WORD 2*/
    uint8_t    sc_packet_offset;
    uint8_t    sc_contents[2];
    uint8_t    sc_value;
/*WORD 3*/
    uint8_t    sc_record_lsbits[4];
/*WORD 4*/
    uint8_t    sc_es_byte_1[3];
    uint8_t    sc_record_msbits;
/*WORD 5*/
    uint8_t    sc_es_byte_2[3];
    uint8_t    sc_error_word;
} scd;

typedef struct start_code_s {
    uint8_t    sync[3];
    uint8_t    id;
} start_code_t;

typedef struct pes_packet_s {
    /* PES Packet header */
    start_code_t start_code;
    uint8_t    packet_length[2];

    /* Optional PES header */
    uint8_t    original_or_copy:1;
    uint8_t    copyright:1;
    uint8_t    data_allignment_indicator:1;
    uint8_t    priority:1;
    uint8_t    scrambing_control:2;
    uint8_t    marker_bits:2;

    uint8_t    extension_flag:1;
    uint8_t    crc_flag:1;
    uint8_t    additional_copy_info_flag:1;
    uint8_t    dsm_trick_mode_flag:1;
    uint8_t    es_rate_flag:1;
    uint8_t    escr_flag:1;
    uint8_t    pts_dts_indicator:2;

    uint8_t    pes_header_length;
} pes_packet_t;

size_t ClearKeyCasSession::create_pes_packet(void * dstPtr, const void *buffer, unsigned size)
{
    int ts_header_size = 0;
    pes_packet_t *pes_packet = (pes_packet_t *) dstPtr;
    const struct scd *scd;
    uint8_t *pts = (uint8_t *) dstPtr + sizeof (pes_packet_t);
    ALOG_ASSERT(size % sizeof(*scd) == 0); /* always process whole SCT's */

    /* Init PES packet */
    BKNI_Memset((void *)pes_packet, 0x00, sizeof(*pes_packet));
    pes_packet->start_code.sync[2] = 0x01;
    pes_packet->marker_bits = 0x2;

    for (scd = (const struct scd *) buffer; size; size -= sizeof(*scd), scd++) {
        if ((scd->sc_value >= 0xC0) && (scd->sc_value <= 0xEF)) {
            pes_packet->start_code.id = scd->sc_value;
            ts_header_size = scd->sc_packet_offset;
        } else if (scd->sc_value == 0xFE) { /* PTS */
            pts[4] = (scd->sc_es_byte_1[0] << 2) | (0x01);
            pts[3] = (scd->sc_es_byte_1[1] << 2) | (scd->sc_es_byte_1[0] >> 6); // inc , dec
            pts[2] = ((scd->sc_es_byte_1[2] & 0x1f) << 3) | ((scd->sc_es_byte_1[1] & 0xC0) >> 5) | (0x01);
            pts[1] = ((scd->sc_record_msbits & 0x1f) << 3) | ((scd->sc_es_byte_1[2] & 0x07) >> 5);
            pts[0] = ((scd->sc_record_msbits & 0xe0) >> 4) | (0x21);
            pes_packet->pts_dts_indicator |= 0x2;
            pes_packet->pes_header_length = 5;
        } else { /* ES */
            ALOG_ASSERT(ts_header_size);
            pes_packet->pes_header_length = scd->sc_packet_offset - ts_header_size - 9; /* Subtract PES Packet Header (9) */
            break;
        }
    }

    pes_packet->packet_length[0] = 0;
    pes_packet->packet_length[1] = 3 + pes_packet->pes_header_length;

    return (sizeof(pes_packet_t) + pes_packet->pes_header_length);
}

void ClearKeyCasSession::setDmaJobBlockSettings(NEXUS_DmaJobBlockSettings *blockSettings, uint8_t *destination, uint8_t *source, size_t num, bool scrambled)
{
    NEXUS_DmaJob_GetDefaultBlockSettings(blockSettings);
    blockSettings->resetCrypto               = scrambled;
    blockSettings->scatterGatherCryptoStart  = scrambled;
    blockSettings->scatterGatherCryptoEnd    = scrambled;
    blockSettings->cached                    = false;
    blockSettings->pSrcAddr                  = source;
    blockSettings->pDestAddr                 = destination;
    blockSettings->blockSize                 = num;
}

NEXUS_Error ClearKeyCasSession::dmaProcessBlocks(NEXUS_DmaJobBlockSettings *blockSettings, int numBlocks, bool scrambled)
{
    NEXUS_DmaJobSettings jobSettings;
    NEXUS_DmaJobHandle dmaJob;
    NEXUS_Error rc = NEXUS_SUCCESS;

    NEXUS_DmaJob_GetDefaultSettings(&jobSettings);
    jobSettings.keySlot                     = (scrambled) ? hKeySlot : NULL;
    jobSettings.dataFormat                  = NEXUS_DmaDataFormat_eBlock;
    jobSettings.completionCallback.callback = nexus_callback;
    jobSettings.completionCallback.context  = event;
    jobSettings.numBlocks = numBlocks;
    dmaJob = NEXUS_DmaJob_Create(dma, &jobSettings);
    ALOG_ASSERT(dmaJob);

    rc = NEXUS_DmaJob_ProcessBlocks(dmaJob, blockSettings, numBlocks);
    if (rc == NEXUS_DMA_QUEUED )
    {
        BKNI_WaitForEvent(event, BKNI_INFINITE);
        rc = NEXUS_SUCCESS;
    }

    NEXUS_DmaJob_Destroy(dmaJob);
    return rc;
}

// Decryption of a set of sub-samples
ssize_t ClearKeyCasSession::decrypt(
        bool secure, DescramblerPlugin::ScramblingControl scramblingControl,
        size_t numSubSamples, const DescramblerPlugin::SubSample *subSamples,
        const void *srcPtr, void *dstPtr, AString * /* errorDetailMsg */) {
    NEXUS_Error rc = NEXUS_SUCCESS;
    NEXUS_DmaJobBlockSettings clearBlockSettings[numSubSamples * 2], scramBlockSettings[numSubSamples];
    NEXUS_MemoryBlockHandle inputBuffHandle;
    BOMX_SecBufferSt *pSecureBuffSt;
    size_t numCopyBytes = 0, numParseBytes, numBytesDecrypted = ERROR_CAS_DECRYPT;
    int numClearBlocks = 0, numScramBlocks = 0;
    bool scrambled = (scramblingControl != DescramblerPlugin::kScrambling_Unscrambled);
    uint8_t *src = (uint8_t*)srcPtr;
    uint8_t *dst = (uint8_t*)dstPtr;
    uint8_t *copyBufferPtr = copyBuffer;
    uint8_t *parseBufferPtr = parseBuffer;

    if (secure) {
        void *pMemory;
        native_handle_t *handle = (native_handle_t *) dstPtr;
        dstPtr = (uint8_t *)(intptr_t)(handle->data[0]);

        // The destination buffer needs to be preprocessed for secure decryption
        inputBuffHandle = (NEXUS_MemoryBlockHandle) dstPtr;
        rc = BOMX_LockSecureBuffer(inputBuffHandle, &pSecureBuffSt);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("BOMX_LockSecureBuffer failed");
            return ERROR_CAS_DECRYPT;
        }

        rc = NEXUS_MemoryBlock_Lock((NEXUS_MemoryBlock *) pSecureBuffSt->hSecureBuff, &pMemory);
        if (rc != NEXUS_SUCCESS || (pMemory == NULL)) {
            ALOGE("NEXUS_MemoryBlock_Lock failed");
            goto errorExit;
        }
        NEXUS_MemoryBlock_Unlock((NEXUS_MemoryBlock *) pSecureBuffSt->hSecureBuff);
        dstPtr = static_cast<uint8_t*> (pMemory);
        dst = (uint8_t*)dstPtr;
        pSecureBuffSt->clearBuffOffset = 0;
    }

    if (scrambled) {
        // Hold lock to get the key only to avoid contention for decryption
        Mutex::Autolock _lock(mKeyLock);

        int32_t keyIndex = (scramblingControl & 1);
        if (!mKeyInfo[keyIndex].valid) {
           ALOGE("decrypt: key %d is invalid", keyIndex);
           goto errorExit;
        }

#if (NEXUS_SECURITY_API_VERSION==1)
        NexusKey.keyIVType = NEXUS_SecurityKeyIVType_eNoIV;
        BKNI_Memcpy(NexusKey.keyData, mKeyInfo[keyIndex].contentKey, NexusKey.keySize);
        rc = NEXUS_Security_LoadClearKey(hKeySlot, &NexusKey);
        ALOG_ASSERT(!rc);
#else
        BKNI_Memcpy(slotKey.key, mKeyInfo[keyIndex].contentKey, slotKey.size);
        rc = NEXUS_KeySlot_SetEntryKey(hKeySlot, NEXUS_KeySlotBlockEntry_eCpsClear, &slotKey);
        ALOG_ASSERT(!rc);
#endif
    }

    for (size_t i = 0; i < numSubSamples; i++) {
        size_t numClear = subSamples[i].mNumBytesOfClearData;
        size_t numEncrypted = subSamples[i].mNumBytesOfEncryptedData;

        numCopyBytes += numClear + numEncrypted;
        if (numCopyBytes > COPY_BUFFER_SIZE) {
            ALOGE("Copy buffer too small, size exceeds %u", COPY_BUFFER_SIZE);
            goto errorExit;
        }

        if (numClear) {
            BKNI_Memcpy(copyBufferPtr, src, numClear);

            if (secure) {
                setDmaJobBlockSettings(&clearBlockSettings[numClearBlocks++], dst, copyBufferPtr, numClear, false);
            } else {
                setDmaJobBlockSettings(&clearBlockSettings[numClearBlocks++], parseBufferPtr, copyBufferPtr, numClear, false);
                parseBufferPtr += numClear;
            }

            dst += numClear;
            src += numClear;
            copyBufferPtr += numClear;
        }

        if (numEncrypted) {
            BKNI_Memcpy(copyBufferPtr, src, numEncrypted);

            if (scrambled && numEncrypted >= AES_BLOCK_SIZE) {
                if (secure) {
                    setDmaJobBlockSettings(&scramBlockSettings[numScramBlocks++], dst, copyBufferPtr, numEncrypted, true);
                } else {
                    setDmaJobBlockSettings(&scramBlockSettings[numScramBlocks++], parseBufferPtr, copyBufferPtr, numEncrypted, true);
                    parseBufferPtr += numEncrypted;
                }
            } else {
                // Don't decrypt if len < AES_BLOCK_SIZE.
                // The last chunk shorter than AES_BLOCK_SIZE is not encrypted.
                if (secure) {
                    setDmaJobBlockSettings(&clearBlockSettings[numClearBlocks++], dst, copyBufferPtr, numEncrypted, false);
                } else {
                    setDmaJobBlockSettings(&clearBlockSettings[numClearBlocks++], parseBufferPtr, copyBufferPtr, numEncrypted, false);
                    parseBufferPtr += numEncrypted;
                }
            }

            dst += numEncrypted;
            src += numEncrypted;
            copyBufferPtr += numEncrypted;
        }
    }

    numParseBytes = parseBufferPtr - parseBuffer;
    if (numParseBytes > (PARSE_BUFFER_SIZE - PAYLOAD_SIZE)) {
        ALOGE("Parsing something too large (%d), we should only need the first TS packet's worth.", numParseBytes);
        ALOGE("Is this plugin being run as insecure mode? (NOT SUPPORTED!)");
        goto errorExit;
    }

    if (numCopyBytes) {
        NEXUS_Memory_FlushCache(copyBuffer, numCopyBytes);
    }

    if (numClearBlocks) {
        rc = dmaProcessBlocks(clearBlockSettings, numClearBlocks, false);
        if (rc) {
            ALOGE("dmaProcessBlocks failed");
            goto errorExit;
        }
    }

    if (numScramBlocks) {
        rc = dmaProcessBlocks(scramBlockSettings, numScramBlocks, true);
        if (rc) {
            ALOGE("dmaProcessBlocks failed");
            goto errorExit;
        }
    }


    if (numParseBytes) {
        const void *data_buffer, *index_buffer;
        size_t data_buffer_size, index_buffer_size, numConsumed;
        size_t dummy_bytes = (PAYLOAD_SIZE - ((parseBufferPtr - parseBuffer) % PAYLOAD_SIZE)) % PAYLOAD_SIZE;

        desc.length = numParseBytes + dummy_bytes;

        rc = NEXUS_Playpump_SubmitScatterGatherDescriptor(playpump, &desc, 1, &numConsumed);
        ALOG_ASSERT(!rc);
        ALOG_ASSERT(numConsumed==1);

        rc = BKNI_WaitForEvent(event, RECPUMP_ITB_TIMEOUT_MS);
        if (rc == BERR_TIMEOUT) {
            ALOGE("No index data captured!");
        } else {
            rc = NEXUS_Recpump_GetIndexBuffer(recpump, &index_buffer, &index_buffer_size);
            ALOG_ASSERT(!rc);
            ALOG_ASSERT(index_buffer_size);
            numBytesDecrypted = create_pes_packet(dstPtr, index_buffer, index_buffer_size);

            rc = NEXUS_Recpump_IndexReadComplete(recpump, index_buffer_size);
            ALOG_ASSERT(!rc);
        }

        rc = NEXUS_Playpump_Flush(playpump);
        ALOG_ASSERT(!rc);

        rc = NEXUS_Recpump_GetDataBuffer(recpump, &data_buffer, &data_buffer_size);
        ALOG_ASSERT(!rc);

        rc = NEXUS_Recpump_DataReadComplete(recpump, data_buffer_size);
        ALOG_ASSERT(!rc);
    } else {
        numBytesDecrypted = dst - (uint8_t *)dstPtr;
    }

    if (secure && !numScramBlocks && (pSecureBuffSt->clearBuffSize > 0)) {
        if (numCopyBytes > pSecureBuffSt->clearBuffSize) {
            ALOGE("pSecureBuffSt->clearBuffSize (%zu) is too small! Required: %zu", pSecureBuffSt->clearBuffSize, numCopyBytes);
            numBytesDecrypted = ERROR_CAS_DECRYPT;
            goto errorExit;
        }
        pSecureBuffSt->clearBuffOffset = sizeof(*pSecureBuffSt);
        pSecureBuffSt->clearBuffSize = numCopyBytes;
        BKNI_Memcpy((uint8_t*)pSecureBuffSt + pSecureBuffSt->clearBuffOffset, srcPtr, numCopyBytes);
    }

errorExit:
    if (secure) {
        BOMX_UnlockSecureBuffer(inputBuffHandle);
    }

    return numBytesDecrypted;
}

bool ClearKeyDescramblerPlugin::requiresSecureDecoderComponent(
        const char *mime) const {
    ALOGV("requiresSecureDecoderComponent: mime=%s", mime);
    return true;
}

status_t ClearKeyDescramblerPlugin::setMediaCasSession(
        const CasSessionId &sessionId) {
    ALOGV("setMediaCasSession: sessionId=%s", sessionIdToString(sessionId).string());

    std::shared_ptr<ClearKeyCasSession> session =
            ClearKeySessionLibrary::get()->findSession(sessionId);

    if (session.get() == nullptr) {
        ALOGE("ClearKeyDescramblerPlugin: session not found");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    std::atomic_store(&mCASSession, session);
    return OK;
}

ssize_t ClearKeyDescramblerPlugin::descramble(
        bool secure,
        ScramblingControl scramblingControl,
        size_t numSubSamples,
        const SubSample *subSamples,
        const void *srcPtr,
        int32_t srcOffset,
        void *dstPtr,
        int32_t dstOffset,
        AString *errorDetailMsg) {

    if (numSubSamples == 0) {
        return 0;
    }

    ALOG_ASSERT(subSamples);
    ALOG_ASSERT(srcPtr);
    ALOG_ASSERT(dstPtr);

    std::shared_ptr<ClearKeyCasSession> session = std::atomic_load(&mCASSession);

    if (session.get() == NULL) {
        ALOGE("ClearKeyDescramblerPlugin: Invalid session");
        return ERROR_CAS_DECRYPT_UNIT_NOT_INITIALIZED;
    }

    return session->decrypt(
            secure, scramblingControl,
            numSubSamples, subSamples,
            (uint8_t*)srcPtr + srcOffset,
            dstPtr == NULL ? NULL : ((uint8_t*)dstPtr + dstOffset),
            errorDetailMsg);
}

} // namespace clearkeycas
} // namespace android
