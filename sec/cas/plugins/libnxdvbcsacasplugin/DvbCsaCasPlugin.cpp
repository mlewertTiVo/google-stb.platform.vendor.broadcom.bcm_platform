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
#include "DvbCsaCasPlugin.h"
#include "DvbCsaSessionLibrary.h"
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/Log.h>
#include <cutils/native_handle.h>
#include "bomx_secure_buff.h"

android::CasFactory* createCasFactory() {
    return new android::dvbcsacas::DvbCsaCasFactory();
}

android::DescramblerFactory *createDescramblerFactory()
{
    return new android::dvbcsacas::DvbCsaDescramblerFactory();
}

namespace android {
namespace dvbcsacas {

#define SYSTEM_ID (0xFFFF) /* Mock CA system ID */
#define TS_SIZE (188)
#define PUMP_BUFFER_SIZE (TS_SIZE * 1024 * 16)
#define RECPUMP_TIMEOUT_MS (50) /* Timeout for playpump/recpump job */
#define KEY_SIZE (16)

bool DvbCsaCasFactory::isSystemIdSupported(int32_t CA_system_id) const {
    return CA_system_id == SYSTEM_ID;
}

status_t DvbCsaCasFactory::queryPlugins(
        std::vector<CasPluginDescriptor> *descriptors) const {
    descriptors->clear();
    descriptors->push_back({SYSTEM_ID, String8("Broadcom DVBCSA CAS")});
    return OK;
}

status_t DvbCsaCasFactory::createPlugin(
        int32_t CA_system_id,
        void *appData,
        CasPluginCallback callback,
        CasPlugin **plugin) {
    if (!isSystemIdSupported(CA_system_id)) {
        return BAD_VALUE;
    }

    *plugin = new DvbCsaCasPlugin(appData, callback);
    return OK;
}
///////////////////////////////////////////////////////////////////////////////
bool DvbCsaDescramblerFactory::isSystemIdSupported(
        int32_t CA_system_id) const {
    return CA_system_id == SYSTEM_ID;
}

status_t DvbCsaDescramblerFactory::createPlugin(
        int32_t CA_system_id, DescramblerPlugin** plugin) {
    if (!isSystemIdSupported(CA_system_id)) {
        return BAD_VALUE;
    }

    *plugin = new DvbCsaDescramblerPlugin();
    return OK;
}

///////////////////////////////////////////////////////////////////////////////
DvbCsaCasPlugin::DvbCsaCasPlugin(
        void *appData, CasPluginCallback callback)
    : mCallback(callback), mAppData(appData) {
    ALOGV("CTOR");
}

DvbCsaCasPlugin::~DvbCsaCasPlugin() {
    ALOGV("DTOR");
    DvbCsaSessionLibrary::get()->destroyPlugin(this);
}

status_t DvbCsaCasPlugin::setPrivateData(const CasData &/*data*/) {
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

status_t DvbCsaCasPlugin::openSession(CasSessionId* sessionId) {
    ALOGV("openSession");

    return DvbCsaSessionLibrary::get()->addSession(this, sessionId);
}

status_t DvbCsaCasPlugin::closeSession(const CasSessionId &sessionId) {
    ALOGV("closeSession: sessionId=%s", sessionIdToString(sessionId).string());
    std::shared_ptr<DvbCsaCasSession> session =
            DvbCsaSessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    DvbCsaSessionLibrary::get()->destroySession(sessionId);
    return OK;
}

status_t DvbCsaCasPlugin::setSessionPrivateData(
        const CasSessionId &sessionId, const CasData & /*data*/) {
    ALOGV("setSessionPrivateData: sessionId=%s",
            sessionIdToString(sessionId).string());
    std::shared_ptr<DvbCsaCasSession> session =
            DvbCsaSessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }
    return OK;
}

status_t DvbCsaCasPlugin::processEcm(
        const CasSessionId &sessionId, const CasEcm& ecm) {

    BSTD_UNUSED(ecm);

    ALOGV("processEcm: sessionId=%s", sessionIdToString(sessionId).string());
    std::shared_ptr<DvbCsaCasSession> session =
            DvbCsaSessionLibrary::get()->findSession(sessionId);
    if (session.get() == nullptr) {
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    Mutex::Autolock lock(mTaLock);

    return OK;
}

status_t DvbCsaCasPlugin::processEmm(const CasEmm& /*emm*/) {
    ALOGV("processEmm");
    Mutex::Autolock lock(mTaLock);

    return OK;
}

status_t DvbCsaCasPlugin::sendEvent(
        int32_t event, int32_t arg, const CasData &eventData) {
    ALOGV("sendEvent: event=%d, arg=%d", event, arg);
    // Echo the received event to the callback.
    // Clear key plugin doesn't use any event, echo'ing for testing only.
    if (mCallback != NULL) {
        mCallback((void*)mAppData, event, arg, (uint8_t*)eventData.data(), eventData.size());
    }
    return OK;
}

status_t DvbCsaCasPlugin::provision(const String8 &str) {
    ALOGV("provision: provisionString=%s", str.string());
    Mutex::Autolock lock(mTaLock);

    return OK;
}

status_t DvbCsaCasPlugin::refreshEntitlements(
        int32_t refreshType, const CasData &/*refreshData*/) {
    ALOGV("refreshEntitlements: refreshType=%d", refreshType);
    Mutex::Autolock lock(mTaLock);

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

status_t DvbCsaCasSession::initPump(struct Pump *pump, bool index)
{
    NEXUS_RecpumpOpenSettings recpumpOpenSettings;
    NEXUS_PlaypumpOpenPidChannelSettings pidChannelSettings;
    NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_PidChannelHandle pidChannel;
    NEXUS_RecpumpSettings recpumpSettings;
    NEXUS_RecpumpAddPidChannelSettings addPidChannelSettings;
    NEXUS_HeapHandle secureHeap;
    NEXUS_Error rc;
    status_t status = ERROR_CAS_SESSION_NOT_OPENED;

    BKNI_Memset(pump, 0x00, sizeof(pump));

    rc = NEXUS_Memory_Allocate(PUMP_BUFFER_SIZE, NULL, (void **) &pump->copyBuffer);
    if (rc) {
        ALOGE("NEXUS_Memory_Allocate failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    /* Never changes */
    pump->desc.addr = pump->copyBuffer;
    NEXUS_Playpump_GetDefaultWriteCompleteSettings(&pump->desc.descriptorSettings);

    /* set up the source */
    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    playpumpOpenSettings.fifoSize = PUMP_BUFFER_SIZE;
    playpumpOpenSettings.descriptorOptionsEnabled = true;
    playpumpOpenSettings.memory = NEXUS_MemoryBlock_FromAddress(pump->copyBuffer);
    playpumpOpenSettings.dataNotCpuAccessible = true;
    pump->playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
    if (!pump->playpump) {
        ALOGE("NEXUS_Memory_Allocate failed");
        return status;
    }

    NEXUS_Playpump_GetSettings(pump->playpump, &playpumpSettings);
    playpumpSettings.transportType = NEXUS_TransportType_eTs;
    playpumpSettings.allPass = true;
    rc = NEXUS_Playpump_SetSettings(pump->playpump, &playpumpSettings);
    if (rc) {
        ALOGE("NEXUS_Playpump_SetSettings failed");
        return status;
    }

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
    rc = NEXUS_Playpump_GetAllPassPidChannelIndex(pump->playpump, &pidChannelSettings.pidSettings.pidChannelIndex);
    if (rc) {
        ALOGE("NEXUS_Playpump_GetAllPassPidChannelIndex failed");
        return status;
    }

    pidChannel = NEXUS_Playpump_OpenPidChannel(pump->playpump, 0, &pidChannelSettings);
    if (!pidChannel) {
        ALOGE("NEXUS_Playpump_OpenPidChannel failed");
        return status;
    }

    rc = NEXUS_KeySlot_AddPidChannel(keyslotHandle, pidChannel);
    if (rc) {
        ALOGE("NEXUS_KeySlot_AddPidChannel failed");
        return status;
    }

    secureHeap = NEXUS_Heap_Lookup(NEXUS_HeapLookupType_eCompressedRegion);
    NEXUS_Recpump_GetDefaultOpenSettings(&recpumpOpenSettings);
    recpumpOpenSettings.data.heap = secureHeap;
    recpumpOpenSettings.data.bufferSize = PUMP_BUFFER_SIZE;
    recpumpOpenSettings.data.dataReadyThreshold = PUMP_BUFFER_SIZE;
    recpumpOpenSettings.data.atomSize = 0;
    recpumpOpenSettings.index.dataReadyThreshold = 0;
    pump->recpump = NEXUS_Recpump_Open(NEXUS_ANY_ID, &recpumpOpenSettings);
    if (!pump->recpump) {
        ALOGE("NEXUS_Recpump_Open failed");
        return status;
    }

    NEXUS_Recpump_GetSettings(pump->recpump, &recpumpSettings);
    recpumpSettings.data.dataReady.callback = unused_nexus_callback;
    if (index) {
        recpumpSettings.index.dataReady.callback = nexus_callback;
        recpumpSettings.index.dataReady.context = index_event;
    }
    recpumpSettings.lastCmd.callback = nexus_callback;
    recpumpSettings.lastCmd.context = last_event;
    recpumpSettings.dropBtpPackets = true;
    recpumpSettings.outputTransportType = index ? NEXUS_TransportType_eTs : NEXUS_TransportType_eEs;
    recpumpSettings.bandHold = NEXUS_RecpumpFlowControl_eDisable;
    rc = NEXUS_Recpump_SetSettings(pump->recpump, &recpumpSettings);
    if (rc) {
        ALOGE("NEXUS_Recpump_SetSettings failed");
        return status;
    }

    NEXUS_Recpump_GetDefaultAddPidChannelSettings(&addPidChannelSettings);
    addPidChannelSettings.pidType = NEXUS_PidType_eOther;
    addPidChannelSettings.pidTypeSettings.other.index = true; /* Require index for ES output */
    rc = NEXUS_Recpump_AddPidChannel(pump->recpump, pidChannel, &addPidChannelSettings);
    if (rc) {
        ALOGE("NEXUS_Recpump_AddPidChannel failed");
        return status;
    }

    rc = NEXUS_Recpump_Start(pump->recpump);
    if (rc) {
        ALOGE("NEXUS_Recpump_Start failed");
        return status;
    }

    rc = NEXUS_Playpump_Start(pump->playpump);
    if (rc) {
        ALOGE("NEXUS_Playpump_Start failed");
        return status;
    }

    return OK;
}

void DvbCsaCasSession::unInitPump(struct Pump *pump) {
    if (pump->playpump)
        NEXUS_Playpump_Stop(pump->playpump);

    if (pump->recpump) {
        NEXUS_Recpump_Stop(pump->recpump);
        NEXUS_Recpump_RemoveAllPidChannels(pump->recpump);
    }

    if (pump->playpump) {
        NEXUS_Playpump_CloseAllPidChannels(pump->playpump);
        NEXUS_Playpump_Close(pump->playpump);
    }

    if(pump->copyBuffer)
        NEXUS_Memory_Free(pump->copyBuffer);

    if (pump->recpump)
        NEXUS_Recpump_Close(pump->recpump);
}

status_t DvbCsaCasSession::initSession()
{
    NEXUS_MemoryAllocationSettings memoryAllocationSettings;
    NEXUS_Error rc;
    status_t status;

    /* Hard set keys as an example */
    const unsigned char odd[8]  = {0x64, 0x63, 0x6F, 0x6D, 0x54, 0x65, 0x73, 0x74};
    const unsigned char even[8] = {0x42, 0x63, 0x73, 0x64, 0x42, 0x72, 0x6F, 0x61};

    ALOGV("Open session");

#if (NEXUS_SECURITY_API_VERSION==1)
    NEXUS_SecurityKeySlotSettings keySettings;
    NEXUS_SecurityAlgorithmSettings NexusConfig;

    NEXUS_Security_GetDefaultKeySlotSettings(&keySettings);
    keySettings.keySlotEngine = NEXUS_SecurityEngine_eCa;
    keyslotHandle = NEXUS_Security_AllocateKeySlot(&keySettings);
    if (!keyslotHandle) {
        ALOGE("Allocate keyslot failed\n");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    /* Set up decryption key */
    NEXUS_Security_GetDefaultAlgorithmSettings( &NexusConfig );
    NexusConfig.algorithm           = NEXUS_SecurityAlgorithm_eDvbCsa2;
    NexusConfig.dvbScramLevel       = NEXUS_SecurityDvbScrambleLevel_eTs;
    NexusConfig.dest                = NEXUS_SecurityAlgorithmConfigDestination_eCa;
    if( NEXUS_Security_ConfigAlgorithm( keyslotHandle, &NexusConfig ) != 0 )
    {
        ALOGE("NEXUS_Security_ConfigAlgorithm failed\n");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NEXUS_Security_GetDefaultClearKey( &NexusKey );
    NexusKey.keySize = 8;
    NexusKey.keyEntryType = NEXUS_SecurityKeyType_eEven;
    NexusKey.dest = NEXUS_SecurityAlgorithmConfigDestination_eCa;
    BKNI_Memcpy(NexusKey.keyData, even, sizeof(even));
    if( NEXUS_Security_LoadClearKey( keyslotHandle, &NexusKey ) != 0)
    {
        ALOGE("Load encryption key failed\n");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NexusKey.keyEntryType = NEXUS_SecurityKeyType_eOdd;
    memcpy(NexusKey.keyData, odd, sizeof(odd));
    if(NEXUS_Security_LoadClearKey( keyslotHandle, &NexusKey ) != 0)
    {
        ALOGE("Load encryption key failed\n");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }
#else
    NEXUS_KeySlotAllocateSettings keySettings;
    NEXUS_KeySlotEntrySettings keyslotEntrySettings;
    NEXUS_KeySlotSettings keyslotSettings;
    NEXUS_KeySlotKey slotKey;

    NEXUS_KeySlot_GetDefaultAllocateSettings(&keySettings);
    keyslotHandle = NEXUS_KeySlot_Allocate(&keySettings);
    if(!keyslotHandle) {
        ALOGE("NEXUS_KeySlot_Allocate failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NEXUS_KeySlot_GetSettings(keyslotHandle, &keyslotSettings);
    rc = NEXUS_KeySlot_SetSettings(keyslotHandle, &keyslotSettings);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetSettings failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NEXUS_KeySlot_GetEntrySettings(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaEven, &keyslotEntrySettings);
    keyslotEntrySettings.algorithm          = NEXUS_CryptographicAlgorithm_eDvbCsa2;
    keyslotEntrySettings.algorithmMode      = NEXUS_CryptographicAlgorithmMode_eEcb;
    keyslotEntrySettings.terminationMode    = NEXUS_KeySlotTerminationMode_eClear;
    keyslotEntrySettings.solitaryMode       = NEXUS_KeySlotTerminationSolitaryMode_eClear;
    rc = NEXUS_KeySlot_SetEntrySettings(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaEven, &keyslotEntrySettings);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetEntrySettings failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    NEXUS_KeySlot_GetEntrySettings(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaOdd, &keyslotEntrySettings);
    keyslotEntrySettings.algorithm          = NEXUS_CryptographicAlgorithm_eDvbCsa2;
    keyslotEntrySettings.algorithmMode      = NEXUS_CryptographicAlgorithmMode_eEcb;
    keyslotEntrySettings.terminationMode    = NEXUS_KeySlotTerminationMode_eClear;
    keyslotEntrySettings.solitaryMode       = NEXUS_KeySlotTerminationSolitaryMode_eClear;
    rc = NEXUS_KeySlot_SetEntrySettings(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaOdd, &keyslotEntrySettings);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("Allocate keyslot failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    slotKey.size = sizeof(even);
    memcpy(slotKey.key, even, sizeof(even));
    rc = NEXUS_KeySlot_SetEntryKey(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaEven, &slotKey);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetEntryKey failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    slotKey.size = sizeof(odd);
    memcpy(slotKey.key, odd, sizeof(odd));
    rc = NEXUS_KeySlot_SetEntryKey(keyslotHandle, NEXUS_KeySlotBlockEntry_eCaOdd, &slotKey);
    if (rc != NEXUS_SUCCESS) {
        ALOGE("NEXUS_KeySlot_SetEntryKey failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }
#endif

    /* Open DMA handle */
    dma = NEXUS_Dma_Open( 0, NULL );
    if (!dma) {
        ALOGE("NEXUS_Dma_Open failed");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    BKNI_CreateEvent(&last_event);
    BKNI_CreateEvent(&index_event);
    BKNI_CreateEvent(&dma_event);

    status = initPump(&indexPump, true);
    if (status != OK) {
        return status;
    }

    status = initPump(&decryptPump, false);
    if (status != OK) {
        return status;
    }

    return OK;
}

void DvbCsaCasSession::unInitSession() {
    ALOGV("Close session");

    unInitPump(&decryptPump);
    unInitPump(&indexPump);

    if (dma_event)
        BKNI_DestroyEvent(dma_event);

    if (index_event)
        BKNI_DestroyEvent(index_event);

    if (last_event)
        BKNI_DestroyEvent(last_event);

    if (dma)
        NEXUS_Dma_Close(dma);

    if (keyslotHandle) {
#if (NEXUS_SECURITY_API_VERSION==1)
        NEXUS_Security_FreeKeySlot(keyslotHandle);
#else
        NEXUS_KeySlot_Invalidate(keyslotHandle);
        NEXUS_KeySlot_Free(keyslotHandle);
#endif
    }
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

typedef struct pes_payload_s {
    uint8_t    pts[5];
    uint8_t    dts[5];
} pes_payload_t;

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

    pes_payload_t payload;
} pes_packet_t;

typedef struct adapation_field_s {
    uint8_t    adapation_field_length;

    uint8_t    adaptation_field_extension_flag:1;
    uint8_t    transport_private_data_flag:1;
    uint8_t    splicing_point_flag:1;
    uint8_t    opcr_flag:1;
    uint8_t    pcr_flag:1;
    uint8_t    elementary_stream_priority_indicator:1;
    uint8_t    random_access_indicator:1;
    uint8_t    discontinuity_indicator:1;

    uint8_t    transport_private_data[10];
} adapation_field_t;

typedef struct ts_packet_s {
    uint8_t    sync;

    uint8_t    pid_msb:5;
    uint8_t    transport_priority:1;
    uint8_t    payload_unit_start_indicator:1;
    uint8_t    transport_error_indicator:1;

    uint8_t    pid_lsb;

    uint8_t    continuity_counter:4;
    uint8_t    adaptation_field_control:2;
    uint8_t    transport_scrambling_control:2;
} ts_packet_t;

typedef struct btp_ts_s {
    ts_packet_t ts;         /* 4 */
    adapation_field_t af;   /* 16 */
    uint8_t padding[172];   /* 188 */
} btp_ts_t;

typedef struct pts_ts_s {
    ts_packet_t ts;         /* 4 */
    pes_packet_t pes;       /* 23 */
    uint8_t padding[165];   /* 188 */
} pts_ts_t;

static const btp_ts_t btp_packet_template = {
        .ts.sync = 0x47, /* Sync */
        .ts.payload_unit_start_indicator = 0x1,
        .ts.adaptation_field_control = 0x2,
        .af.adapation_field_length = 183,
        .af.transport_private_data_flag = 0x1,
        .af.transport_private_data[0] = 45,
        .af.transport_private_data[2] = 'B',
        .af.transport_private_data[3] = 'R',
        .af.transport_private_data[4] = 'C',
        .af.transport_private_data[5] = 'M',
        .af.transport_private_data[9] = 0x82,
};

static const pts_ts_t ts_packet_template = {
        .ts.sync = 0x47, /* Sync */
        .ts.payload_unit_start_indicator = 0x1,
        .ts.adaptation_field_control = 0x1,
        .pes.start_code.sync[2] = 0x01,
        .pes.marker_bits = 0x2,
};

void DvbCsaCasSession::parse_itb_buffer(void * dstPtr, const void *buffer, unsigned size, int *ts_header_size)
{
    pts_ts_t *ts_packet = (pts_ts_t *) dstPtr;
    const struct scd *scd;

    ALOG_ASSERT(size % sizeof(*scd) == 0); /* always process whole SCT's */

    for (scd = (const struct scd *) buffer; size; size -= sizeof(*scd), scd++) {
        if ((scd->sc_value >= 0xC0) && (scd->sc_value <= 0xEF)) {
            ts_packet->pes.start_code.id = scd->sc_value;
            *ts_header_size = scd->sc_packet_offset;
        } else if (scd->sc_value == 0xFE) { /* PTS */
            /* PTS - assume always present */
            ts_packet->pes.payload.pts[0] = (scd->sc_record_msbits >> 4) | (0x21);
            ts_packet->pes.payload.pts[1] = (scd->sc_record_msbits << 3) | (scd->sc_es_byte_1[2] >> 5);
            ts_packet->pes.payload.pts[2] = (scd->sc_es_byte_1[2] << 3) | (scd->sc_es_byte_1[1] >> 5) | (0x01);
            ts_packet->pes.payload.pts[3] = (scd->sc_es_byte_1[1] << 2) | (scd->sc_es_byte_1[0] >> 6);
            ts_packet->pes.payload.pts[4] = (scd->sc_es_byte_1[0] << 2) | (0x01);
            ts_packet->pes.pts_dts_indicator = 0x2;
            ts_packet->pes.pes_header_length = 5;
            /* DTS */
            if (scd->sc_es_byte_2[0] || scd->sc_es_byte_2[1] || scd->sc_es_byte_2[2] || scd->sc_error_word) {
                ts_packet->pes.payload.dts[0] = (scd->sc_error_word >> 4) | (0x21);
                ts_packet->pes.payload.dts[1] = (scd->sc_error_word << 3) | (scd->sc_es_byte_2[2] >> 5);
                ts_packet->pes.payload.dts[2] = (scd->sc_es_byte_2[2] << 3) | (scd->sc_es_byte_2[1] >> 5) | (0x01);
                ts_packet->pes.payload.dts[3] = (scd->sc_es_byte_2[1] << 2) | (scd->sc_es_byte_2[0] >> 6);
                ts_packet->pes.payload.dts[4] = (scd->sc_es_byte_2[0] << 2) | (0x01);
                ts_packet->pes.pts_dts_indicator = 0x3;
                ts_packet->pes.pes_header_length = 10;
            }
        } else { /* ES */
            ALOG_ASSERT(*ts_header_size);
            ts_packet->pes.pes_header_length = scd->sc_packet_offset - *ts_header_size - 9; /* Subtract PES Packet Header (9) */
            break;
        }
    }
}

size_t DvbCsaCasSession::create_ts_packet(void * dstPtr, const void *buffer, unsigned size, const void *wrap_buffer, unsigned wrap_size)
{
    int ts_header_size = 0;
    pts_ts_t *ts_packet = (pts_ts_t *) dstPtr;

    /* Init TS packet */
    BKNI_Memcpy(dstPtr, &ts_packet_template, sizeof(ts_packet_template));

    parse_itb_buffer(dstPtr, buffer, size, &ts_header_size);
    if (wrap_buffer) {
        parse_itb_buffer(dstPtr, wrap_buffer, wrap_size, &ts_header_size);
    }

    ts_packet->pes.packet_length[0] = 0;
    ts_packet->pes.packet_length[1] = 3 + ts_packet->pes.pes_header_length;

    return sizeof(ts_packet_template);
}

void DvbCsaCasSession::setDmaJobBlockSettings(NEXUS_DmaJobBlockSettings *blockSettings, void *destination, const void *source, size_t num)
{
    NEXUS_DmaJob_GetDefaultBlockSettings(blockSettings);
    blockSettings->cached                    = false;
    blockSettings->pSrcAddr                  = source;
    blockSettings->pDestAddr                 = destination;
    blockSettings->blockSize                 = num;
}

NEXUS_Error DvbCsaCasSession::dmaProcessBlocks(NEXUS_DmaJobBlockSettings *blockSettings, int numBlocks)
{
    NEXUS_DmaJobSettings jobSettings;
    NEXUS_DmaJobHandle dmaJob;
    NEXUS_Error rc = NEXUS_SUCCESS;

    NEXUS_DmaJob_GetDefaultSettings(&jobSettings);
    jobSettings.dataFormat                  = NEXUS_DmaDataFormat_eBlock;
    jobSettings.completionCallback.callback = nexus_callback;
    jobSettings.completionCallback.context  = dma_event;
    jobSettings.numBlocks = numBlocks;
    jobSettings.bypassKeySlot = NEXUS_BypassKeySlot_eGR2R;
    dmaJob = NEXUS_DmaJob_Create(dma, &jobSettings);
    ALOG_ASSERT(dmaJob);

    rc = NEXUS_DmaJob_ProcessBlocks(dmaJob, blockSettings, numBlocks);
    if (rc == NEXUS_DMA_QUEUED )
    {
        BKNI_WaitForEvent(dma_event, BKNI_INFINITE);
        rc = NEXUS_SUCCESS;
    }

    NEXUS_DmaJob_Destroy(dmaJob);
    return rc;
}

ssize_t DvbCsaCasSession::pumpDecrypt(uint8_t *dst, uint8_t *src, size_t numBytes, bool index) {
    size_t numBytesDecrypted = ERROR_CAS_DECRYPT;
    NEXUS_Error rc = NEXUS_SUCCESS;
    const void *data_buffer, *wrap_buffer, *index_buffer, *index_wrap_buffer;
    BKNI_EventHandle event = index ? index_event : last_event;
    Pump *pump = (index) ? &indexPump : &decryptPump;
    size_t data_size, wrap_size, total_size, index_buffer_size, index_wrap_buffer_size, numConsumed;

    if (numBytes > PUMP_BUFFER_SIZE) {
        ALOGE("Copy buffer too small, size exceeds %u", PUMP_BUFFER_SIZE);
        goto errorExit;
    }

    BKNI_Memcpy(pump->desc.addr, src, numBytes);
    pump->desc.length = numBytes;

    if (!index) {
        BKNI_Memcpy(((uint8_t *) (pump->desc.addr)) + numBytes, &btp_packet_template, sizeof(btp_packet_template));
        pump->desc.length += sizeof(btp_packet_template);
    }

    NEXUS_Memory_FlushCache(pump->desc.addr, pump->desc.length);

    rc = NEXUS_Playpump_SubmitScatterGatherDescriptor(pump->playpump, &pump->desc, 1, &numConsumed);
    if (rc || (numConsumed != 1)) {
        ALOGE("NEXUS_Playpump_SubmitScatterGatherDescriptor failed");
        return ERROR_CAS_DECRYPT;
    }

    rc = BKNI_WaitForEvent(event, RECPUMP_TIMEOUT_MS);
    if (rc == BERR_TIMEOUT) {
        ALOGE("%s timeout!\n", index ? "index" : "last_cmd");
        return ERROR_CAS_DECRYPT;
    }

    rc = NEXUS_Recpump_GetDataBufferWithWrap(pump->recpump, &data_buffer, &data_size, &wrap_buffer, &wrap_size);
    if (rc) {
        ALOGE("NEXUS_Recpump_GetDataBuffer failed");
        return ERROR_CAS_DECRYPT;
    }
    total_size = data_size + wrap_size;

    rc = NEXUS_Recpump_GetIndexBufferWithWrap(pump->recpump, &index_buffer, &index_buffer_size, &index_wrap_buffer, &index_wrap_buffer_size);
    if (rc) {
        ALOGE("NEXUS_Recpump_GetIndexBuffer failed");
        return ERROR_CAS_DECRYPT;
    }

    if (index) {
        numBytesDecrypted = create_ts_packet(dst, index_buffer, index_buffer_size, index_wrap_buffer, index_wrap_buffer_size);
    } else {
        NEXUS_DmaJobBlockSettings blockSettings[2];
        int numBlocks;

        setDmaJobBlockSettings(&blockSettings[0], dst, data_buffer, data_size);
        if (wrap_buffer) {
            setDmaJobBlockSettings(&blockSettings[1], dst + data_size, wrap_buffer, wrap_size);
            numBlocks = 2;
        } else {
            numBlocks = 1;
        }
        rc = dmaProcessBlocks(blockSettings, numBlocks);
        if (rc) {
            ALOGE("dmaProcessBlocks failed");
            return ERROR_CAS_DECRYPT;
        }

        numBytesDecrypted = total_size;
    }

    rc = NEXUS_Recpump_DataReadComplete(pump->recpump, total_size);
    if (rc) {
        ALOGE("NEXUS_Recpump_DataReadComplete failed");
        return ERROR_CAS_DECRYPT;
    }

    rc = NEXUS_Recpump_IndexReadComplete(pump->recpump, index_buffer_size + index_wrap_buffer_size);
    if (rc) {
        ALOGE("NEXUS_Recpump_IndexReadComplete failed");
        return ERROR_CAS_DECRYPT;
    }

errorExit:
    return numBytesDecrypted;
}


// Decryption of a set of sub-samples
ssize_t DvbCsaCasSession::decrypt(
        bool secure,
        const DescramblerPlugin::SubSample *subSamples,
        const void *srcPtr, void *dstPtr, AString * /* errorDetailMsg */) {
    NEXUS_Error rc = NEXUS_SUCCESS;
    NEXUS_MemoryBlockHandle inputBuffHandle;
    BOMX_SecBufferSt *pSecureBuffSt;
    size_t numBytesDecrypted = ERROR_CAS_DECRYPT;
    uint8_t *src = (uint8_t*)srcPtr;
    uint8_t *dst = (uint8_t*)dstPtr;
    size_t numClear = subSamples->mNumBytesOfClearData;
    size_t numEncrypted = subSamples->mNumBytesOfEncryptedData;

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

    if (secure) {
        if (numClear) {
            NEXUS_DmaJobBlockSettings blockSettings;
            if (pSecureBuffSt->clearBuffSize > 0) {
                if (numClear > pSecureBuffSt->clearBuffSize) {
                    ALOGE("pSecureBuffSt->clearBuffSize (%zu) is too small! Required: %zu", pSecureBuffSt->clearBuffSize, numClear);
                    goto errorExit;
                }
                pSecureBuffSt->clearBuffOffset = sizeof(*pSecureBuffSt);
                pSecureBuffSt->clearBuffSize = numClear;
                BKNI_Memcpy((uint8_t*)pSecureBuffSt + pSecureBuffSt->clearBuffOffset, srcPtr, numClear);
            }
            if (numClear > PUMP_BUFFER_SIZE) {
                ALOGE("Copy buffer too small, size (%zu) exceeds %u", numClear, PUMP_BUFFER_SIZE);
                goto errorExit;
            }
            BKNI_Memcpy(decryptPump.copyBuffer, src, numClear);
            setDmaJobBlockSettings(&blockSettings, dst, decryptPump.copyBuffer, numClear);
            NEXUS_Memory_FlushCache(decryptPump.copyBuffer, numClear);
            rc = dmaProcessBlocks(&blockSettings, 1);
            if (rc) {
                ALOGE("dmaProcessBlocks failed");
                numBytesDecrypted = ERROR_CAS_DECRYPT;
                goto errorExit;
            }
            numBytesDecrypted = numClear;
        } else {
            numBytesDecrypted = pumpDecrypt(dst, src, numEncrypted, false);
        }
    } else { /* insecure */
        if (numClear) {
            BKNI_Memcpy(dst, src, numClear);
            numBytesDecrypted = numClear;
        } else {
            numBytesDecrypted = pumpDecrypt(dst, src, numEncrypted, true);
        }
    }

errorExit:
    if (secure) {
        BOMX_UnlockSecureBuffer(inputBuffHandle);
    }

    ALOGV("descrambled %d", numBytesDecrypted);

    return numBytesDecrypted;
}

bool DvbCsaDescramblerPlugin::requiresSecureDecoderComponent(
        const char *mime) const {
    ALOGV("requiresSecureDecoderComponent: mime=%s", mime);
    return true;
}

status_t DvbCsaDescramblerPlugin::setMediaCasSession(
        const CasSessionId &sessionId) {
    ALOGV("setMediaCasSession: sessionId=%s", sessionIdToString(sessionId).string());

    std::shared_ptr<DvbCsaCasSession> session =
            DvbCsaSessionLibrary::get()->findSession(sessionId);

    if (session.get() == nullptr) {
        ALOGE("DvbCsaDescramblerPlugin: session not found");
        return ERROR_CAS_SESSION_NOT_OPENED;
    }

    std::atomic_store(&mCASSession, session);
    return OK;
}

ssize_t DvbCsaDescramblerPlugin::descramble(
        bool secure,
        ScramblingControl scramblingControl,
        size_t numSubSamples,
        const SubSample *subSamples,
        const void *srcPtr,
        int32_t srcOffset,
        void *dstPtr,
        int32_t dstOffset,
        AString *errorDetailMsg) {

    BSTD_UNUSED(scramblingControl);

    ALOG_ASSERT(subSamples);
    ALOG_ASSERT(srcPtr);
    ALOG_ASSERT(dstPtr);

    /* If no subsamples provided, or empty subsample, return no error */
    if (numSubSamples == 0 || !(subSamples[0].mNumBytesOfClearData || subSamples[0].mNumBytesOfEncryptedData)) {
        return 0;
    }

    if (numSubSamples > 1 || (subSamples[0].mNumBytesOfClearData && subSamples[0].mNumBytesOfEncryptedData)) {
        ALOGE("DVBCSA: Invalid request");
        return ERROR_CAS_DECRYPT;
    }

    ALOGV("secure %u, clear %zu, scrambled %zu", secure, subSamples[0].mNumBytesOfClearData, subSamples[0].mNumBytesOfEncryptedData);

    std::shared_ptr<DvbCsaCasSession> session = std::atomic_load(&mCASSession);

    if (session.get() == NULL) {
        ALOGE("DvbCsaDescramblerPlugin: Invalid session");
        return ERROR_CAS_DECRYPT_UNIT_NOT_INITIALIZED;
    }

    return session->decrypt(
            secure,
            subSamples,
            (uint8_t*)srcPtr + srcOffset,
            dstPtr == NULL ? NULL : ((uint8_t*)dstPtr + dstOffset),
            errorDetailMsg);
}

} // namespace dvbcsacas
} // namespace android
