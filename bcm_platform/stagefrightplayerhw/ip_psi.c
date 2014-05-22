/******************************************************************************
 *    (c)2008-2012 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: ip_psi.c $
 * $brcm_Revision: 35 $
 * $brcm_Date: 9/6/12 2:21p $
 * 
 * Module Description:
 *  ip psi test file
 * 
 * Revision History:
 * 
 * $brcm_Log: /nexus/lib/playback_ip/apps/ip_psi.c $ 
 * 
 * 35   9/6/12 2:21p ssood
 * SW7346-944: re-enable frontend untune call as firmware now supports it
 * 
 * 34   7/28/12 3:24p ssood
 * SW7346-944: dont skip over programs with ca_pid set (as it seems to be
 *  set even for clear channels) + support cciMode & priority parms for
 *  OFDM tuning
 * 
 * 33   7/26/12 8:33p ssood
 * SW7346-944: add support to stream from OFDM-capable frontend
 * 
 * 32   6/27/12 1:44p ssood
 * SW7435-250: more coverity fixes exposed by new coverity version
 * 
 * 31   6/25/12 6:08p ssood
 * SW7435-250: coverity fixes exposed by new coverity version
 * 
 * 30   6/7/12 2:50p ssood
 * SW7425-3042: fixing compile error for non-frontend platforms
 * 
 * 29   5/29/12 4:55p ssood
 * SW7425-3042: added start/stop callbacks in the psi functions
 * 
 * 28   5/21/12 2:01p ssood
 * SW7425-3042: suppress debug logging by default
 * 
 * 27   5/17/12 6:36p ssood
 * SW7425-3042: simplified build flags & code by using nexus multi process
 *  capabilities
 * 
 * 26   4/17/12 10:05a ssood
 * SW7346-782: fix coverity errors
 * 
 * 25   4/13/12 7:39p ssood
 * SW7346-782: added support for streaming from streamer input
 * 
 * 24   3/12/12 4:41p gkamosa
 * SW7425-2543: Merge support for 3383 SMS platform
 * 
 * Centaurus_Phase_1.0/2   3/6/12 6:03p gkamosa
 * SW7425-2337: Merge branch to tip
 * 
 * 23   1/12/12 3:13p ssood
 * SW7344-250: coverity fix to use dynamic allocation
 * 
 * 22   1/4/12 3:22p ssood
 * SW7231-561: use fast status call to determine frontend tuning status
 * 
 * Centaurus_Phase_1.0/1   2/6/12 6:10p jmiddelk
 * SW7425-2337: Adding support for 93383sms
 * 
 * 21   12/21/11 3:48p ssood
 * SW7231-516: fixed the sharing of same xcode session w/ multiple clients
 *  + modified the qam frontend logic to use fast lock Status function
 * 
 * 20   10/14/11 10:18a ssood
 * SW7425-889: initial check-in for enabling audio in the live xcoded
 *  streams
 * 
 * 19   7/14/11 12:01p ssood
 * SW7346-309: Add support for Streaming from Satellite Source
 * 
 * 18   6/18/11 11:52a ssood
 * SW7425-718: 3128 related changes: use different frontend resources for
 *  each streaming session even though it is same channel
 * 
 * 17   8/12/10 11:24a ssood
 * SW7420-883: added support for streaming same transcoding session to
 *  multiple clients
 * 
 * 16   7/30/10 2:11p garetht
 * SW7420-919: Add CableCARD support to Ip_streamer
 * 
 * 15   3/4/10 12:17p ssood
 * SW7420-561: merge from branch to mainline
 * 
 * SW7420-561/2   2/16/10 10:42p ssood
 * SW7420-561: Changes to support basic RTSP w/o trickmodes
 * 
 * SW7420-561/1   2/16/10 10:11a ssood
 * SW7420-561: initial cut of new APIs to meet Media Browser, DMR, &
 *  Philips RTSP requirements
 * 
 * 14   1/27/10 10:18a ssood
 * SW7420-454: conditionally compile Live Streaming Code using
 *  LIVE_STREAMING_SUPPORT
 * 
 * 13   1/19/10 4:55p ssood
 * SW7420-454: convert verbose output to be available via msg modules
 *  flags
 * 
 * 12   1/14/10 6:41p ssood
 * SW7408-47: Add support to compile IP Streamer App on platforms w/ no
 *  frontend support
 * 
 * 11   12/4/09 8:09p ssood
 * SW7420-454: added support to parse programs w/ either audio or video
 *  streams only
 * 
 * 10   11/24/09 6:05p ssood
 * SW7420-454: reset psi structure before each psi acquisition
 * 
 * 9   10/20/09 6:33p ssood
 * SW7420-340: remove nexus playback, audio, video decoder dependencies
 *  for SMS compilation
 * 
 * 8   10/9/09 12:29p ssood
 * SW7420-340: add support to play live streams on non-Broadcom clients
 * 
 * 7   10/8/09 1:14p ssood
 * SW7420-340: convert PSI data ready event from global to per thread
 *  specific + enabled debug prints
 * 
 * 6   10/2/09 11:35a ssood
 * SW7420-340: wait for qam lock before acquiring PSI info
 * 
 * 5   9/30/09 9:49a ssood
 * SW7420-340: wait for signal lock before starting PSI acquisition
 * 
 * 4   9/25/09 5:15p ssood
 * SW7420-340: Add support for VSB frontend
 * 
 * 3   9/17/09 2:58p ssood
 * SW7420-340: compiling for SMS
 * 
 * 2   9/16/09 12:07p ssood
 * SW7420-340: header changes to include file history
 * 
 ******************************************************************************/
#include "nexus_pid_channel.h"
#include "nexus_playpump.h"
#include "nexus_message.h"
#include "ip_psi.h"
#include "b_playback_ip_lib.h"
#include "b_os_lib.h"
#include "b_os_lib.h"

#include "b_psip_lib.h"
#include "nexus_core_utils.h"

#undef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))

#define ISO936_CODE_LENGTH 3
typedef struct
{
    uint16_t pid;
    uint8_t streamType;
    uint16_t ca_pid;
    unsigned char iso639[ISO936_CODE_LENGTH];
} EPID;

/* struct used to keep track of the si callback we must use, to notify
   the si applib that it's previously requested data is now available */
typedef struct siRequest_t
{
    B_PSIP_DataReadyCallback   callback;
    void                   * callbackParam;
} siRequest_t;

/* we only have one filterHandle (i.e. msg), so we only have to keep track
   of the current request from si (specifically, the si "data ready"
   callback and associated param) */
static siRequest_t g_siRequest;

/* nexus message "data ready" callback - forward notification to si lib */
static void DataReady(void * context, int param)
{
    BSTD_UNUSED(context);
    BSTD_UNUSED(param);

    if (NULL != g_siRequest.callback) {
        /* printf("in DataReady callback - forward notification to psip lib\n");*/
        g_siRequest.callback(g_siRequest.callbackParam);
    }
}

/* message "data ready" callback which is called by si when our requested data
   has been processed and is ready for retrieval */
static void SiDataReady(B_Error status, void * context)
{
    /* printf("in APP DataReady callback - psip lib has finished processing data\n");*/
    if (B_ERROR_SUCCESS == status) {
        BKNI_SetEvent((BKNI_EventHandle)context);
    } else {
        LOGE("problem receiving data from api call - error code:%d\n", status);
        /* set event so our test app can continue... */
        BKNI_SetEvent((BKNI_EventHandle)context);
    }
}

/* utility function to convert si applib message filter to nexus message filter 
   assumes pNexusFilter has already been properly initialized with default values */
static void cpyFilter(NEXUS_MessageFilter * pNexusFilter, B_PSIP_Filter * pSiFilter)
{
    int i = 0;

    for (i = 0; i < MIN(B_PSIP_FILTER_SIZE, NEXUS_MESSAGE_FILTER_SIZE); i++)
    {
        if (i == 2) {
            /* WARNING: will not filter message byte 2! */
            continue;
        } else
            if (i >= 2) {
                /* adjust nexus index see NEXUS_MessageFilter in nexus_message.h */
                pNexusFilter->mask[i-1]        = pSiFilter->mask[i];
                pNexusFilter->coefficient[i-1] = pSiFilter->coef[i];
                pNexusFilter->exclusion[i-1]   = pSiFilter->excl[i];
            } else {
                pNexusFilter->mask[i]        = pSiFilter->mask[i];
                pNexusFilter->coefficient[i] = pSiFilter->coef[i];
                pNexusFilter->exclusion[i]   = pSiFilter->excl[i];
            }
    }
}

/* gets a pidchannel - nexus handles pid channel reuse automatically */
NEXUS_PidChannelHandle OpenPidChannel(psiCollectionDataType * pCollectionData, uint16_t pid)
{
    int    i = 0;
    NEXUS_PidChannelSettings pidChannelSettings;
    NEXUS_PlaypumpOpenPidChannelSettings playpumpPidChannelSettings;

    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&playpumpPidChannelSettings);

    playpumpPidChannelSettings.pidSettings.requireMessageBuffer = true;
    pidChannelSettings.requireMessageBuffer = true;
    NEXUS_PidChannel_GetDefaultSettings(&pidChannelSettings);

    for (i = 0; i < NUM_PID_CHANNELS; i++)
    {
        if (NULL == pCollectionData->pid[i].channel) {
            LOGD("open pidChannel for pid:0x%04x\n", pid);
            pCollectionData->pid[i].num = pid; 
            pCollectionData->pid[i].channel = 
                NEXUS_Playpump_OpenPidChannel(pCollectionData->playpump, pid, &playpumpPidChannelSettings);
            return pCollectionData->pid[i].channel;
        }
    }

    if (i == NUM_PID_CHANNELS) {
        LOGE("failed to open pid channel:0x%04x - not enough storage space in pCollectionData!\n", pid);
    }

    return NULL;
}

/* closes a previously opened pid channel */
void ClosePidChannel(psiCollectionDataType * pCollectionData, uint16_t pid)
{
    int i = 0;
    bool found=false;

    for (i = 0; i < NUM_PID_CHANNELS; i++)
    {
        /* find pid to close */
        if (( pCollectionData->pid[i].num == pid) &&
            ( pCollectionData->pid[i].channel != NULL)) {
            LOGD("close pidChannel for pid:0x%04x\n", pid);
            NEXUS_Playpump_ClosePidChannel(pCollectionData->playpump, pCollectionData->pid[i].channel);
            pCollectionData->pid[i].channel = NULL;
            pCollectionData->pid[i].num = 0;
            found = true;
            break;
        }
    }
    if (!found)
        LOGE("failure closing pid channel:0x%04x - not found in list\n", pid);
}

void StartMessageFilter(psiCollectionDataType * pCollectionData, B_PSIP_CollectionRequest * pRequest, NEXUS_PidChannelHandle pidChannel)
{
    NEXUS_MessageHandle          msg;
    NEXUS_MessageStartSettings   msgStartSettings;

    BSTD_UNUSED(pCollectionData);
    msg = (NEXUS_MessageHandle)pRequest->filterHandle;
    /* printf("StartMessageFilter\n");*/

    NEXUS_Message_GetDefaultStartSettings(msg, &msgStartSettings);
    msgStartSettings.bufferMode = NEXUS_MessageBufferMode_eOneMessage;
    msgStartSettings.pidChannel = pidChannel;
    cpyFilter(&msgStartSettings.filter, &pRequest->filter);

    NEXUS_Message_Start(msg, &msgStartSettings);
    NEXUS_StartCallbacks(msg);
}

void StopMessageFilter(psiCollectionDataType * pCollectionData, B_PSIP_CollectionRequest * pRequest)
{
    NEXUS_MessageHandle          msg;
    BSTD_UNUSED(pCollectionData);

    msg = (NEXUS_MessageHandle)pRequest->filterHandle;
    NEXUS_StopCallbacks(msg);
    NEXUS_Message_Stop(msg);
}

/* callback function called by si applib to collect si data */
static B_Error CollectionFunc(B_PSIP_CollectionRequest * pRequest, void * context)
{

    NEXUS_PidChannelHandle pidChannel = NULL;
    pPsiCollectionDataType  pCollectionData = (pPsiCollectionDataType)context;
    NEXUS_MessageHandle          msg;

    BDBG_ASSERT(NULL != pRequest);
    BDBG_ASSERT(NULL != context);
    msg = (NEXUS_MessageHandle)pRequest->filterHandle;
    if (NULL == msg) {
        LOGE("invalid filterHandle received from SI applib!\n");
        return B_ERROR_INVALID_PARAMETER;
    }

    switch (pRequest->cmd)
    {
        const uint8_t  * buffer;
        size_t             size;

    case B_PSIP_eCollectStart:
        LOGD("-=-=- B_PSIP_eCollectStart -=-=-\n");

        /*
         * Save off pRequest->callback and pRequest->callbackParam.
         * these need to be called when DataReady() is called.  this will
         * notify the si lib that it can begin processing the received data.
         * Si applib only allows us to call one API at a time (per filterHandle),
         * so we only have to keep track of the latest siRequest.callback
         * and siRequest.callbackParam (for our one and only filterHandle).
         */
        g_siRequest.callback      = pRequest->callback;
        g_siRequest.callbackParam = pRequest->callbackParam;

        pidChannel = OpenPidChannel(pCollectionData, pRequest->pid);
        StartMessageFilter(pCollectionData, pRequest, pidChannel);
        break;

    case B_PSIP_eCollectStop:
        LOGD("-=-=- B_PSIP_eCollectStop -=-=-\n");
        StopMessageFilter(pCollectionData, pRequest);
        ClosePidChannel(pCollectionData, pRequest->pid);
        break;

    case B_PSIP_eCollectGetBuffer:
        LOGD("-=-=- B_PSIP_eCollectGetBuffer -=-=-\n");
        /* fall through for now... */

    case B_PSIP_eCollectCopyBuffer:
        /*printf("-=-=- B_PSIP_eCollectCopyBuffer -=-=-\n");*/
        if (NEXUS_SUCCESS == NEXUS_Message_GetBuffer(msg, (const void **)&buffer, &size)) {
            if (0 < size) {
                LOGD("NEXUS_Message_GetBuffer() succeeded! size:%d\n",size);
                memcpy(pRequest->pBuffer, buffer, *(pRequest->pBufferLength)); /* copy valid data to request buffer */
                *(pRequest->pBufferLength) = MIN(*(pRequest->pBufferLength), size);
                NEXUS_Message_ReadComplete(msg, size);
            } else {
                /* NEXUS_Message_GetBuffer can return size==0 (this is normal
                 * operation).  We will simply wait for the next data ready 
                 * notification by returning a B_ERROR_RETRY to the PSIP applib 
                 */
                NEXUS_Message_ReadComplete(msg, size);
                return B_ERROR_PSIP_RETRY;
            }
        }
        else {
            LOGE("NEXUS_Message_GetBuffer() failed\n");

            return B_ERROR_UNKNOWN;
        }
        break;

    default:
        LOGE("-=-=- invalid Command received:%d -=-=-\n", pRequest->cmd);
        return B_ERROR_INVALID_PARAMETER;
        break;
    }

    return B_ERROR_SUCCESS;
}

/* convertStreamToPsi                            */
/* Fills in B_PlaybackIpPsiInfo based on PMT info */
static void convertStreamToPsi( TS_PMT_stream *stream, B_PlaybackIpPsiInfo *psi)
{
    psi->mpegType = NEXUS_TransportType_eTs;
    switch (stream->stream_type) {
        case TS_PSI_ST_13818_2_Video:
        case TS_PSI_ST_ATSC_Video:
            psi->videoCodec = NEXUS_VideoCodec_eMpeg2;
            psi->videoPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_11172_2_Video:
            psi->videoCodec = NEXUS_VideoCodec_eMpeg1;
            psi->videoPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_14496_10_Video:
            psi->videoCodec = NEXUS_VideoCodec_eH264;
            psi->videoPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case 0x27:
            psi->videoCodec = NEXUS_VideoCodec_eH265;
            psi->videoPid = stream->elementary_PID;
            psi->psiValid = true;
        break;
        case 0x24:
            psi->videoCodec = NEXUS_VideoCodec_eH265;
            psi->videoPid = stream->elementary_PID;
            psi->psiValid = true;
        break;
        case TS_PSI_ST_11172_3_Audio: 
        case TS_PSI_ST_13818_3_Audio:
            psi->audioCodec = NEXUS_AudioCodec_eMpeg;
            psi->audioPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_ATSC_AC3:
            psi->audioCodec = NEXUS_AudioCodec_eAc3;
            psi->audioPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_ATSC_EAC3:
            psi->audioCodec = NEXUS_AudioCodec_eAc3Plus;
            psi->audioPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_13818_7_AAC:
            psi->audioCodec = NEXUS_AudioCodec_eAac;
            psi->audioPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        case TS_PSI_ST_14496_3_Audio:
            psi->audioCodec = NEXUS_AudioCodec_eAacPlus;
            psi->audioPid = stream->elementary_PID; 
            psi->psiValid = true;
            break;
        default:
            LOGE("###### TODO: Unknown stream type: %x #####", stream->stream_type);
    }
}

static void 
tsPsi_procStreamDescriptors( const uint8_t *p_bfr, unsigned bfrSize, int streamNum, EPID *ePidData )
{
    int i;
    TS_PSI_descriptor descriptor;

    for( i = 0, descriptor = TS_PMT_getStreamDescriptor( p_bfr, bfrSize, streamNum, i );
        descriptor != NULL;
        i++, descriptor = TS_PMT_getStreamDescriptor( p_bfr, bfrSize, streamNum, i ) )
    {
        switch (descriptor[0])
        {
        case TS_PSI_DT_CA:
            ePidData->ca_pid = ((descriptor[4] & 0x1F) << 8) + descriptor[5];
            break;

        default:
            break;
        }
    }
}

/*                                                         */
/* This function returns first stream (video and/or audio) */ 
/* To return multiple streams, remove stream found and     */
/* pass a ptr to array of psi structures.                  */
/*                                                         */
void acquirePsiInfo(pPsiCollectionDataType pCollectionData, B_PlaybackIpPsiInfo *psi, int programsToFind, int *numProgramsFound)
{

    B_Error                    errCode;
    NEXUS_MessageSettings      settings;
    NEXUS_MessageHandle        msg = NULL;
    B_PSIP_Handle si = NULL;
    B_PSIP_Settings si_settings;
    B_PSIP_ApiSettings settingsApi;
    TS_PMT_stream stream;
    TS_PAT_program program;
    uint8_t  i,j; 
    uint8_t  *buf = NULL;
    uint32_t bufLength = 4096;
    uint8_t  *bufPMT = NULL;
    uint32_t bufPMTLength = 4096;
    NEXUS_Error rc;
    B_PlaybackIpSessionStartSettings ipStartSettings;
    B_PlaybackIpSessionStartStatus ipStartStatus;
    BKNI_EventHandle dataReadyEvent = NULL;
    EPID epid;

    buf = BKNI_Malloc(bufLength);
    bufPMT = BKNI_Malloc(bufPMTLength);
    if (buf == NULL || bufPMT == NULL) { LOGE("BKNI_Malloc Failure at %d", __LINE__); goto error;}
    /* Start stream  */
    *numProgramsFound = 0;
    /* start playpump */
    rc = NEXUS_Playpump_Start(pCollectionData->playpump);
    if (rc) {LOGE("PSI - NEXUS Error (%d) at %d \n", rc, __LINE__); goto error;}
    /* let IP Applib go ... */
    memset(&ipStartSettings, 0, sizeof(ipStartSettings));
    /* Since PAT/PMT based PSI discovery is only done for Live UDP/RTP/RTSP sessions, so we only need to set the playpump handle */
    ipStartSettings.nexusHandles.playpump = pCollectionData->playpump;
    ipStartSettings.nexusHandlesValid = true;
    ipStartSettings.u.rtsp.mediaTransportProtocol = B_PlaybackIpProtocol_eRtp;  /* protocol used to carry actual media */
    rc = B_PlaybackIp_SessionStart(pCollectionData->playbackIp, &ipStartSettings, &ipStartStatus);
    while (rc == B_ERROR_IN_PROGRESS) {
        /* app can do some useful work while SessionSetup is in progress and resume when callback sends the completion event */
        LOGD("Session Start call in progress, sleeping for now...");
        BKNI_Sleep(100);
        rc = B_PlaybackIp_SessionStart(pCollectionData->playbackIp, &ipStartSettings, &ipStartStatus);
    }
    if (rc) {LOGE("PSI - NEXUS Error (%d) at %d\n", rc, __LINE__); goto error;}


out:
    if (BKNI_CreateEvent(&dataReadyEvent) != BERR_SUCCESS) { LOGE("Failed to create PSI dataReadyEvent \n"); goto error; }

    NEXUS_Message_GetDefaultSettings(&settings);
    settings.dataReady.callback = DataReady;  
    settings.dataReady.context = NULL;
    msg = NEXUS_Message_Open(&settings);
    if (!msg) { LOGE("PSI - NEXUS_Message_Open failed\n"); goto error; }

    B_PSIP_GetDefaultSettings(&si_settings);
    si_settings.PatTimeout = 800;
    si_settings.PmtTimeout = 800;
    si = B_PSIP_Open(&si_settings, CollectionFunc, pCollectionData);
    if (!si) { LOGE("PSI - Unable to open SI applib\n"); goto error; }

    LOGD("starting PSI gathering\n");
    errCode = B_PSIP_Start(si);
    if ( errCode ) { LOGE("PSI - Unable to start SI data collection, err %d\n", errCode); goto error; }


    B_PSIP_GetDefaultApiSettings(&settingsApi);
    settingsApi.siHandle                 = si;
    settingsApi.filterHandle             = msg;
    settingsApi.dataReadyCallback        = SiDataReady;
    settingsApi.dataReadyCallbackParam   = dataReadyEvent;
    settingsApi.timeout                  = 0; /* Use defaults */

    memset(buf, 0, bufLength);
    BKNI_ResetEvent((BKNI_EventHandle)settingsApi.dataReadyCallbackParam);
    if (B_ERROR_SUCCESS != B_PSIP_GetPAT(&settingsApi, buf, &bufLength)) {
        LOGE("B_PSIP_GetPAT() failed\n");
        goto error;
    }

    /* wait for async response from si - wait on dataReadyEvent */
    if (BKNI_WaitForEvent((BKNI_EventHandle)settingsApi.dataReadyCallbackParam, BKNI_INFINITE)) {
        LOGE("Failed to find PAT table ...\n");
        goto error;
    }
    LOGD("received response from SI, len = %d!\n", bufLength);
#if 0
    printf("\n"); for (i=0;i<bufLength;i++) printf("%02x ",buf[i]); printf("\n");
#endif

    if (0 < bufLength) {
        LOGD("PAT Programs found = %d\n", TS_PAT_getNumPrograms(buf));
        for (i = 0; i < programsToFind && (TS_PAT_getProgram(buf, bufLength, i, &program)==BERR_SUCCESS); i++) 
        {
            memset(psi, 0, sizeof(*psi));
            LOGD("program_number: %d, i %d, PID: 0x%04X, psi %p, sizeof psi %d\n", program.program_number, i, program.PID, psi, sizeof(*psi)); 
            psi->pmtPid = program.PID;

            BKNI_ResetEvent((BKNI_EventHandle)settingsApi.dataReadyCallbackParam);
            memset(bufPMT, 0, bufPMTLength);
            settingsApi.timeout = 0; /* Use defaults */
            if (B_ERROR_SUCCESS != B_PSIP_GetPMT(&settingsApi, program.PID, bufPMT, &bufPMTLength)) {
                LOGE("B_PSIP_GetPMT() failed\n");
                continue;
            }
            /* wait for async response from si - wait on dataReadyEvent */
            if (BKNI_WaitForEvent((BKNI_EventHandle)settingsApi.dataReadyCallbackParam, BKNI_INFINITE)) {
                LOGE("Failed to find PMT table ...\n");
                goto error;
            }
            LOGD("received PMT: size:%d, # of streams in this program %d\n", bufPMTLength, TS_PMT_getNumStreams(bufPMT, bufPMTLength));
            /* find and process Program descriptors */
            if (0 < bufPMTLength) {
                for (j = 0; j < TS_PMT_getNumStreams(bufPMT, bufPMTLength); j++)
                {
                    memset(&stream, 0, sizeof(stream));
                    TS_PMT_getStream(bufPMT, bufPMTLength, j, &stream);
                    LOGD("j %d, stream_type:0x%02X, PID:0x%04X\n", j, stream.stream_type, stream.elementary_PID);
                    convertStreamToPsi( &stream, psi);
                    memset(&epid, 0, sizeof(epid));
                    tsPsi_procStreamDescriptors(bufPMT, bufPMTLength, j, &epid);
                    if (epid.ca_pid) {
                        LOGD("even program 0x%x has ca pid 0x%x, we are not ignoring it", program.PID, epid.ca_pid);
                        break;
                    }
                    psi->pcrPid = TS_PMT_getPcrPid(bufPMT, bufPMTLength);
                    if ((psi->videoPid ) && (psi->audioPid )) {
                        (*numProgramsFound)++;
                        LOGD("Found %d program, vpid %d, vcodec %d, apid %d, acodec %d", *numProgramsFound, psi->videoPid, psi->videoCodec, psi->audioPid, psi->audioCodec);
                        psi += 1;
                        break;
                    }
                    else if (j == (TS_PMT_getNumStreams(bufPMT, bufPMTLength)-1)) {
                        /* last stream in the program */
                        if ((psi->videoPid ) || (psi->audioPid )) {
                            (*numProgramsFound)++;
                            LOGD("Found %d program, vpid %d or apid %d", *numProgramsFound, psi->videoPid, psi->audioPid);
                            psi += 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    LOGD("stopping PSI gathering\n");

    B_PlaybackIp_SessionStop(pCollectionData->playbackIp);
    NEXUS_Playpump_Stop(pCollectionData->playpump);

    /* cleanup */
    B_PSIP_Stop(si);
error:
    if (buf)
        BKNI_Free(buf);
    if (bufPMT)
        BKNI_Free(bufPMT);
    if (si)
        B_PSIP_Close(si);
    if (msg)
        NEXUS_Message_Close(msg);
    if (dataReadyEvent)
        BKNI_DestroyEvent(dataReadyEvent);
}
