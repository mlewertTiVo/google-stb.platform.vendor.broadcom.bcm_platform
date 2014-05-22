/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 * $brcm_Workfile: bcmPlayer_ip.cpp $
 * $brcm_Revision: 17 $
 * $brcm_Date: 2/14/13 1:36p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_ip.cpp $
 * 
 * 17   2/14/13 1:36p mnelis
 * SWANDROID-336: Refactor bcmplayer_ip code
 * 
 * 15   1/7/13 1:14p mnelis
 * SWANDROID-293: Don't call B_Os_Uninit twice
 * 
 * 14   11/15/12 10:57a mnelis
 * SWANDROID-165: Set an Androidy user-agent
 * 
 * 13   11/6/12 2:29p mnelis
 * SWANDROID-189: Call B_Os_Uninit at uninit time.
 * 
 * 12   10/17/12 5:52p mnelis
 * SWANDROID-189: Cleanup URL parsing code, avoiding globals to store bits
 *  and pieces.
 * 
 * 11   9/24/12 6:47p mnelis
 * SWANDROID-78: Fix HTML5 streaming behavior
 * 
 * 10   9/19/12 2:02p mnelis
 * SWANDROID-78: General tidyup
 * 
 * 9   9/14/12 1:28p mnelis
 * SWANDROID-78: Use NEXUS_ANY_ID for STC channel and store playbackip in
 *  nexus_handle struct
 * 
 * 8   6/21/12 3:08p franktcc
 * SWANDROID-120: Adding error callback function
 * 
 * 7   6/5/12 2:39p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 6   2/24/12 4:12p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 5   2/8/12 2:54p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 4   12/29/11 6:45p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 3   12/13/11 10:27a franktcc
 * SW7420-1906: Adding capability of setAspectRatio to ICS
 * 
 * 2   12/10/11 7:19p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 2   11/28/11 6:11p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 6   10/19/11 4:08p franktcc
 * SW7425-1540: Correct uninit sequence.
 * 
 * 5   9/22/11 5:00p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 4   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 3   8/22/11 5:35p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 * 1   8/22/11 4:05p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 *
 *****************************************************************************/
// Verbose messages removed
// #define LOG_NDEBUG 0

// comment this out to disable buffer monitoring thread
// #define ENABLE_BUFFER_MONITORING 1

#define LOG_TAG "bcmPlayer_ip"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"
#include "stream_probe.h"

/* include IP lib header files */
#include "b_os_lib.h"
#include "b_playback_ip_lib.h"
#include "ip_psi.h"

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

#define BCMPLAYER_IP_DBG LOGV("bcmPlayer_ip : %s : Enter", __FUNCTION__);

bool ipSetOpenSessionParams(int iPlayerIndex, B_PlaybackIpSessionOpenSettings *sessionOpenSettings);
bool ipSetSetupSessionParams(BcmPlayerIpHandle *ipHandle, B_PlaybackIpSessionSetupSettings *sessionSetupSettings);
bool ipSetupNexusResources(bcmPlayer_base_nexus_handle_t *nexus_handle, int iPlayerIndex);
void ipSetTransportUsage(BcmPlayerIpHandle *ipHandle);
static void preChargeNetworkBuffer (unsigned int preChargeTime /* in sec */, BcmPlayerIpHandle* ipHandle);
static void *bcmPlayerIpBufferMonitoringThread (void *arg);
NEXUS_PidChannelHandle ipGetVideoPidChannel(bcmPlayer_base_nexus_handle_t *nexusHandle);
NEXUS_PidChannelHandle ipGetAudioPidChannel(bcmPlayer_base_nexus_handle_t *nexusHandle);

// Configurables.
/* Data cache size: 30 sec worth for a max bitrate of 3Mbps */
#define BCMPLAYER_IP_MAX_BUFF_SEC 30
#define BCMPLAYER_NETWORK_BUFFER_SIZE (BCMPLAYER_IP_MAX_BUFF_SEC*3*1024*1024)/8;

#define BCMPLAYER_IP_BUFF_THREAD_LOOP_MSEC 500
#define BCMPLAYER_IP_PRECHARGE_TIME_SEC 20
#define BCMPLAYER_IP_PRECHARGE_INIT_TIME_SEC 5

// Work-around for audio issues in SWANDROID-372
#define WA_SWANDROID_372 1

#define B_PLAYBACK_IP_CA_CERT "/system/etc/host.cert"

char userAgent[] = "stagefright/1.2 (Linux;Android 4.4.2)";

pthread_t bcmPlayer_ip_bufferingThreadId[MAX_NEXUS_PLAYER] = {(pthread_t)NULL, (pthread_t)NULL};

static void preChargeNetworkBuffer (unsigned int preChargeTime /* in secs */, BcmPlayerIpHandle* ipHandle)
{
    NEXUS_Error             rc;
    B_PlaybackIpSettings    playbackIpSettings;
    B_PlaybackIpStatus      playbackIpStatus;
    NEXUS_PlaybackPosition  prevBufferDuration = 0;
    int                     noChangeInBufferDepth = 0;
    int                     BufferingTime = 0;

    if (ipHandle == NULL)
    {
        LOGE ("%s: invalid ipHandle", __FUNCTION__);
        return;
    }

    if (ipHandle->endOfStream == true) 
    {
        LOGE("Buffering Aborted due to EOF");
        return;
    }

    /* check how much data is currently buffered: this is from the last seek point */
    BKNI_Sleep (100);    /* sleep little bit to get IP thread finish off any buffering */
    rc = B_PlaybackIp_GetStatus (ipHandle->playbackIp, &playbackIpStatus);
    if (rc) 
    {
        LOGE ("NEXUS Error (%d) at %d, returning...", rc, __LINE__);
        return;
    }

    if (playbackIpStatus.curBufferDuration >= (1000*ipHandle->preChargeTime)) 
    {
        LOGV ("Already buffered %lu milli-sec of data, required %u milli-sec of buffer...",
              playbackIpStatus.curBufferDuration, preChargeTime*1000);
        return;
    }

    if (playbackIpStatus.curBufferDuration >= BCMPLAYER_IP_MAX_BUFF_SEC*1000) 
    {
        LOGV ("Buffer Full %d  / %d ", (int)playbackIpStatus.curBufferDuration, (int)playbackIpStatus.maxBufferDuration);
        return; 
    }   

    /* underflowing in network buffer, tell IP Applib to start buffering */
    LOGV ("Start pre-charging n/w buffer: currently buffered %lu"
         "milli-sec of data, required %u milli-sec of buffer...",
          playbackIpStatus.curBufferDuration, preChargeTime*1000);

    rc = B_PlaybackIp_GetSettings(ipHandle->playbackIp, &playbackIpSettings);
    if (rc) 
    {
        LOGE ("NEXUS Error (%d) at %d, returning...", rc, __LINE__);
        return;
    }

    playbackIpSettings.preChargeBuffer = true;
    rc = B_PlaybackIp_SetSettings(ipHandle->playbackIp, &playbackIpSettings);
    if (rc) 
    {
        LOGE ("NEXUS Error (%d) at %d, returning...", rc, __LINE__);
        return;
    }

    BufferingTime=0;

    /* now monitor the buffer depth until it reaches the desired preChargeTime value */
    do 
    {
        BKNI_Sleep (100);    /* sleep little bit to get IP thread finish off any buffering */

        if (ipHandle->endOfStream == true)
        {
            break;
        }

        rc = B_PlaybackIp_GetStatus(ipHandle->playbackIp, &playbackIpStatus);
        if (rc) 
        {
            LOGE ("NEXUS Error (%d) at %d, returning...", rc, __LINE__);
            return;
        }
        LOGV ("currently buffered %lu milli-sec of data, max duration %d,"
              "required %u milli-sec of buffer...",
              (long unsigned int)playbackIpStatus.curBufferDuration,
              (int)playbackIpStatus.maxBufferDuration, 
              preChargeTime*1000);

        if (playbackIpStatus.curBufferDuration >= playbackIpStatus.maxBufferDuration) 
        {
            LOGV ("currently buffered %lu max duration %d milli-sec worth data, so done buffering",
                  (long unsigned int)playbackIpStatus.curBufferDuration, 
                  (int)playbackIpStatus.maxBufferDuration);
            break;
        }

        if (!prevBufferDuration) 
        {
            prevBufferDuration = playbackIpStatus.curBufferDuration;
        } 
        else 
        {
            if (prevBufferDuration == playbackIpStatus.curBufferDuration) 
            {
                noChangeInBufferDepth++;
            } 
            else  
            {
                noChangeInBufferDepth = 0;
                prevBufferDuration = playbackIpStatus.curBufferDuration;
            }
        }
        BufferingTime++;
        if (noChangeInBufferDepth >= 1000) 
        {
            LOGV ("Warning: can't buffer up to the required buffer depth,"
                  "currently buffered %lu max duration %d milli-sec worth data, so done buffering",
                  (long unsigned int)playbackIpStatus.curBufferDuration,
                  (int)playbackIpStatus.maxBufferDuration);
            break;
        }
        
        if (ipHandle->runMonitoringThread == false)
        {
            LOGV("exiting %s", __FUNCTION__);
            return;
        }
    // keep buffering until we have buffered upto the high water
    // mark or eof/server close events happen
    } while (playbackIpStatus.curBufferDuration < (1000*preChargeTime) &&
            !ipHandle->endOfStream && !playbackIpStatus.serverClosed);

    /* tell IP Applib to stop buffering */
    playbackIpSettings.preChargeBuffer = false;
    rc = B_PlaybackIp_SetSettings (ipHandle->playbackIp, &playbackIpSettings);
    if (rc) 
    {
        LOGE("NEXUS Error (%d)", rc);
        return;
    }

    if (ipHandle->endOfStream) 
    {
        LOGE ("Buffering Aborted due to EOF, buffered %lu milli-sec",
              playbackIpStatus.curBufferDuration);
        return;
    } 
    else if (playbackIpStatus.serverClosed) 
    {
        LOGV ("Can't Buffer anymore due to server closed connection "
              "buffered %lu milli-sec worth of data...",
              playbackIpStatus.curBufferDuration);
        // Note: this is not an error case as server may have closed connection,
        // but we may not have played all this data, so let playback finish
        return;
    } 
    else 
    {
        LOGV ("Buffering Complete (buffered %lu milli-sec worth of data)"
              ", serverClosed %d...",
              playbackIpStatus.curBufferDuration, playbackIpStatus.serverClosed);
        return;
    }
}


static void *bcmPlayerIpBufferMonitoringThread (void *arg)
{
    int                       iPlayerIndex;
    BERR_Code                 rc = B_ERROR_SUCCESS;
    BcmPlayerIpHandle*        ipHandle = (BcmPlayerIpHandle *)arg;
    B_PlaybackIpStatus        playbackIpStatus;
    NEXUS_VideoDecoderStatus  vdecoderStatus;
    NEXUS_AudioDecoderStatus  adecoderStatus;
    NEXUS_PlaybackStatus      pbStatus;
    NEXUS_PlaypumpStatus      ppStatus;
    int                       curBufferDuration;
    NEXUS_Error               nexusRC;

    BCMPLAYER_IP_DBG;

    nexusRC = NEXUS_SUCCESS;
    
    if (ipHandle == NULL)
    {
        LOGE ("invalid ipHandle");
        return NULL;
    }

    if (ipHandle->usePlaypump)
    {
        LOGE ("not using playback");
        return NULL;
    }

    if (nexus_handle[0].ipHandle == ipHandle)
    {
        iPlayerIndex = 0;
    }
    else if (nexus_handle[1].ipHandle == ipHandle)
    {
        iPlayerIndex = 1;
    }
    else
    {
        LOGE("invalid ipHandle %p", ipHandle);
        return NULL;
    }

    playbackIpStatus.serverClosed = false;

    /* monitor playback buffer */
    while (ipHandle->runMonitoringThread == true)
    {
        while (ipHandle->monitoringBuffer == true)
        {
            rc = B_PlaybackIp_GetStatus (ipHandle->playbackIp, &playbackIpStatus);
            if (rc != B_ERROR_SUCCESS) 
            {
                LOGE("%s: B_PlaybackIp_GetStatus returned : %d\n", __FUNCTION__, rc);
                return NULL;
            }
            LOGV("eos %d serverclosed %d", ipHandle->endOfStream, ipHandle->endOfStream);

            while ((ipHandle->endOfStream == false) && (ipHandle->endOfStream == false))
            {
                /* Get status */
                if (nexus_handle[iPlayerIndex].simpleVideoDecoder == NULL)
                {
                  LOGE("Invalid simpleVideoDecoder handle\n");
                  return NULL;
                }
                nexusRC = NEXUS_SimpleVideoDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleVideoDecoder, &vdecoderStatus );
                if (nexusRC != NEXUS_SUCCESS)
                {
                    LOGE("NEXUS_SimpleVideoDecoder_GetStatus error %d", nexusRC);
                }
                else
                {
                    LOGV ("video %.4dx%.4d, pts %#x, (ptsStcDiff %d) fifoSize=%d, fifoDepth=%d, fullness %d%% queueDepth=%d",
                      vdecoderStatus.source.width, vdecoderStatus.source.height, vdecoderStatus.pts, vdecoderStatus.ptsStcDifference,
                      vdecoderStatus.fifoSize, vdecoderStatus.fifoDepth,
                      vdecoderStatus.fifoSize?(vdecoderStatus.fifoDepth*100)/vdecoderStatus.fifoSize:0,
                      vdecoderStatus.queueDepth);
                }
                if (nexus_handle[iPlayerIndex].simpleAudioDecoder == NULL)
                {
                    LOGE("Invalid simpleAudioDecoder handle\n");
                    return NULL;
                }

                nexusRC = NEXUS_SimpleAudioDecoder_GetStatus(nexus_handle[iPlayerIndex].simpleAudioDecoder, &adecoderStatus );
                if (nexusRC != NEXUS_SUCCESS)
                {
                    LOGE("NEXUS_SimpleAudioDecoder_GetStatus error %d", nexusRC);
                }
                else
                {
                    LOGV ("audio %s:[ms=%d] fifoDepth=%d, fifoSize=%d, queuedFrames=%d", __FUNCTION__,
                    (int)adecoderStatus.pts/45, adecoderStatus.fifoDepth, adecoderStatus.fifoSize, adecoderStatus.queuedFrames); 
                }
                rc = B_PlaybackIp_GetStatus(ipHandle->playbackIp, &playbackIpStatus);
                if (rc != B_ERROR_SUCCESS) 
                {
                    LOGE("%s: B_PlaybackIp_GetStatus returned : %d\n", __FUNCTION__, rc);
                    return NULL;
                }

                if (nexus_handle[iPlayerIndex].playpump == NULL)
                {
                    LOGE ("Invalid playpump handle\n");
                    return NULL;
                }
                NEXUS_Playpump_GetStatus(nexus_handle[iPlayerIndex].playpump, &ppStatus);

                if (nexus_handle[iPlayerIndex].playback == NULL)
                {
                    LOGE ("Invalid nexus playback handle\n");
                    return NULL;
                }
                NEXUS_Playback_GetStatus(nexus_handle[iPlayerIndex].playback, &pbStatus);
                LOGV ("playback: ip pos %lu, pb pos %lu, fed %lu, first %lu, last %lu, PB buffer depth %d, size %d, played bytes %lld", 
                      pbStatus.position, pbStatus.readPosition, pbStatus.readPosition - pbStatus.position,
                      pbStatus.first, pbStatus.last, ppStatus.fifoDepth, ppStatus.fifoSize, ppStatus.bytesPlayed);

                if (ipHandle->psi.mpegType == NEXUS_TransportType_eMp4) 
                {
                      curBufferDuration = pbStatus.readPosition - pbStatus.position;
                      LOGV ("buffered %d mill-seconds worth of MP4 content", curBufferDuration);
                }
                else if (ipHandle->psi.mpegType == NEXUS_TransportType_eAsf) 
                {
                    curBufferDuration = (ppStatus.mediaPts -
                    (ipHandle->psi.videoPid?vdecoderStatus.pts:adecoderStatus.pts))/45;
                    LOGV ("buffered %d milli-seconds worth of ASF content", curBufferDuration);
                } 
                else 
                {
                    // we need to use alternate means to determine the amount of buffering in system since
                    // such formats are not processed in sw and thus we don't know the curBufferDepth
                    // instead, we can detect the underflow condition by monitoring the last pts displayed,
                    //once it doesn't change, we are definitely underflowing and thus can precharge
                    // by default, set curBufferDuration to a higher number to avoid precharging
                    curBufferDuration = 99999; // set to some large value
                    if (ipHandle->prevPts) 
                    {
                        if (ipHandle->prevPts == (ipHandle->psi.videoPid?vdecoderStatus.pts:adecoderStatus.pts)) 
                        {
                            /* pts hasn't changed, so we are underflowing, set flag to precharge */
                            LOGV ("pts hasn't changed, so we are underflowing, prev pts,"
                            "%u, cur pts %u, pre charge time %d",
                            ipHandle->prevPts,
                            ipHandle->psi.videoPid?vdecoderStatus.pts:adecoderStatus.pts,
                            ipHandle->preChargeTime);
                            curBufferDuration = 0;
                        }
                    }
                    ipHandle->prevPts = ipHandle->psi.videoPid?vdecoderStatus.pts:adecoderStatus.pts;
                    LOGV ("prev pts, %u, cur pts %u", ipHandle->prevPts,
                    ipHandle->psi.videoPid?vdecoderStatus.pts:adecoderStatus.pts);
                }

                if (ipHandle->preChargeTime && (curBufferDuration < 200) && (curBufferDuration != 0))
                {
                    // we are precharging & current buffer level is below the low water mark, so start 
                    // pre-charging however, sleep quickly to see if underflow is due to EOF. Otherwise,
                    // we will Pause Playback too quickly before we get the EOF callback. Sleep gives 
                    // Nexus Playback a chance to invoke the eof callback.
                    if (ipHandle->endOfStream == true) 
                    {
                        LOGE ("Underflow is due to EOF, breaking out...");
                        break;
                    }
                    LOGV ("Underflowing, so pausing the playback until enough buffering is done...");
                    /* notify app that the ip player is pausing temporarily to buffer more data. */
                    bcmPlayer_bufferingPauseStartCallback (iPlayerIndex);

                    if (NEXUS_Playback_Pause(nexus_handle[iPlayerIndex].playback))
                    {
                        LOGE ("ERROR: Failed to pause Nexus playback");
                        break;
                    }
                    LOGV ("Paused Nexus Playback...");

                    /* Now pre-charge the network buffer */
                    if (ipHandle->firstBuff)
                    {
                        preChargeNetworkBuffer (BCMPLAYER_IP_PRECHARGE_INIT_TIME_SEC, ipHandle);
                        ipHandle->firstBuff = false;
                    }
                    else
                    {
                        preChargeNetworkBuffer (ipHandle->preChargeTime, ipHandle);
                    }

                    if (ipHandle->runMonitoringThread == false)
                    {
                        ipHandle->monitoringBuffer = false;
                        /* need to return quickly from uninit */
                        LOGV ("exiting %s", __FUNCTION__);
                        return NULL;
                    }
 
                    LOGV ("Resuming Playback");
                    /* Notify app that the ip player is resuming playback after buffering. */
                    bcmPlayer_bufferingPauseEndCallback (iPlayerIndex);

                    if (NEXUS_Playback_Play(nexus_handle[iPlayerIndex].playback)) 
                    {
                        LOGE ("ERROR: Failed to play Nexus playback from pause");
                        break;
                    }
                }
                BKNI_Sleep(1000);
            }
            if (ipHandle->endOfStream == true)
            {
                ipHandle->monitoringBuffer = false;
            }
            BKNI_Sleep(100);
        }
        BKNI_Sleep(100);
    }
    return NULL;
}


static void bcmPlayer_uninit_ip (int iPlayerIndex) 
{
    B_PlaybackIpError   rc;
    BcmPlayerIpHandle*  ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    uint32_t            waitCount = 0;

    BCMPLAYER_IP_DBG;

    if (ipHandle)
    {
        /* Make sure buffering is stopped. */
        /* Can't do playback IP pause in B_PlaybackIpState_eBuffering. */
        /* Activity Manager expects reply within 500 ms */

        ipHandle->runMonitoringThread = false;

        rc = B_PlaybackIp_SessionStop(ipHandle->playbackIp);
        if (rc != B_ERROR_SUCCESS) 
        {
            LOGE("%s: B_PlaybackIp_SessionStop returned : %d\n", __FUNCTION__, rc);
        }

        if ((ipHandle->endOfStream == false) && (ipHandle->monitoringBuffer == true))
        {
            ipHandle->endOfStream = true;  
            LOGV("wait for buffering thread to stop\n");
            do 
            {
                BKNI_Sleep (100);
                waitCount++;
                LOGV("%s waiting for buffering thread to loop %d", __FUNCTION__, (int)waitCount);
            } while ((ipHandle->monitoringBuffer == true) && (waitCount < 20));
        }

        if (bcmPlayer_ip_bufferingThreadId[iPlayerIndex] != (pthread_t)NULL)
        {
            LOGV ("ending buffer monitoring thread");
            rc = pthread_join (bcmPlayer_ip_bufferingThreadId[iPlayerIndex], NULL);
            if (rc !=0)
            {
                LOGE ("%s: pthread_join buffering monitoring thread failed %d", __FUNCTION__, rc);
            }
            else
            {
                bcmPlayer_ip_bufferingThreadId[iPlayerIndex] = (pthread_t)NULL;
            }
        }
    }

    bcmPlayer_uninit_base(iPlayerIndex);

    if (ipHandle) {
        if (ipHandle->playbackIp) {
            rc = B_PlaybackIp_SessionStop(ipHandle->playbackIp);
            if (rc != B_ERROR_SUCCESS) {
                LOGD("B_PlaybackIp_SessionStop rc = %d\n", rc);
            }

            rc = B_PlaybackIp_SessionClose(ipHandle->playbackIp);
            if (rc != B_ERROR_SUCCESS) {
                LOGD("B_PlaybackIp_SessionClose rc = %d\n", rc);
            }

            rc = B_PlaybackIp_Close(ipHandle->playbackIp);
            if (rc != B_ERROR_SUCCESS) {
                LOGD("B_PlaybackIp_Close rc = %d\n", rc);
            }

            if (ipHandle->sslCtx) {
                B_PlaybackIp_SslUnInit(ipHandle->sslCtx);
                ipHandle->sslCtx = NULL;
            }
        }

        if (ipHandle->timebase) {
            NEXUS_Timebase_Close(ipHandle->timebase);
        }

        ipHandle->endOfStream = false;

        free(ipHandle);
        ipHandle = NULL;

        B_Os_Uninit();
    }

    if (nexus_handle[iPlayerIndex].stcChannel) {
        NEXUS_StcChannel_Close(nexus_handle[iPlayerIndex].stcChannel);
        nexus_handle[iPlayerIndex].stcChannel = NULL;
    }

    return;
}

static int bcmPlayer_setDataSource_ip(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader) 
{
    int rc;
    BCMPLAYER_IP_DBG;

    if (nexus_handle[iPlayerIndex].ipHandle == NULL) 
    {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return 1;
    }

    rc = bcmPlayer_setDataSource_base(iPlayerIndex, url, videoWidth, videoHeight, extraHeader);
    if (rc != 0) {
        LOGE("[%d]%s: Could not setDataSource_base!!!", iPlayerIndex, __FUNCTION__);
        return 1;
    }

    return 0;
}

static int bcmPlayer_prepare_ip(int iPlayerIndex) {
    B_PlaybackIpSessionOpenSettings sessionOpenSettings;
    B_PlaybackIpSessionOpenStatus sessionOpenStatus;
    B_PlaybackIpSessionSetupSettings sessionSetupSettings;
    B_PlaybackIpSessionSetupStatus sessionSetupStatus;
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpError   rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL) {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return 1;
    }

    if(!ipSetOpenSessionParams(iPlayerIndex, &sessionOpenSettings)) {
        LOGE("%s: Couldn't setOpenSessionParams", __FUNCTION__);
        return 1;
    }

    rc = B_PlaybackIp_SessionOpen(ipHandle->playbackIp, &sessionOpenSettings, &sessionOpenStatus);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_SessionOpen returned: %d", __FUNCTION__, rc);
        return 1;
    }

    if (sessionOpenStatus.u.http.responseHeaders != NULL) {
        LOGV("%s: HTTP Response: %s", __FUNCTION__,
            sessionOpenStatus.u.http.responseHeaders);
        free(sessionOpenStatus.u.http.responseHeaders);
    }

    if (!ipSetSetupSessionParams(ipHandle, &sessionSetupSettings)) {
        LOGE("%s: Couldn't setSetupSessionParams", __FUNCTION__);
        return 1;
    }

    rc = B_PlaybackIp_SessionSetup(ipHandle->playbackIp, &sessionSetupSettings, &sessionSetupStatus);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_SessionSetup returned: %d", __FUNCTION__, rc);
        return 1;
    }

    if (sessionOpenSettings.socketOpenSettings.protocol == B_PlaybackIpProtocol_eRtpNoRtcp
        || sessionOpenSettings.socketOpenSettings.protocol == B_PlaybackIpProtocol_eUdp) {
        psiCollectionDataType  collectionData;
        B_PlaybackIpPsiInfo psiList[1];
        memset(&collectionData, 0, sizeof(psiCollectionDataType));
        collectionData.playpump = nexus_handle[iPlayerIndex].playpump;
        collectionData.playbackIp = ipHandle->playbackIp;
        LOGD(("Acquiring PSI info..."));
        int numPrograms = 0;
        acquirePsiInfo(&collectionData, psiList, 1, &numPrograms);
        if (numPrograms <= 0)
        {
            LOGE("%s: UDP/RTP: Didn't find any programs", __FUNCTION__);
            return 1;
        }
        ipHandle->psi =  psiList[0];
    } else if (!sessionSetupStatus.u.http.psi.psiValid) {
        LOGE("%s: psiValid == false", __FUNCTION__);
        return 1;
    } else {
        ipHandle->psi = sessionSetupStatus.u.http.psi;
    }

    ipSetTransportUsage(ipHandle);

    LOGD("%s: PSI information found : W/H : %dx%d VideoCodec : %d AudioCodec : %d VideoPID : %d AudioPID : %d ",
        __FUNCTION__,
        ipHandle->psi.videoWidth,
        ipHandle->psi.videoHeight,
        ipHandle->psi.videoCodec,
        ipHandle->psi.audioCodec,
        ipHandle->psi.videoPid,
        ipHandle->psi.audioPid);

    if (!ipSetupNexusResources(&nexus_handle[iPlayerIndex], iPlayerIndex)) {
        LOGE("%s: Couldn't setupNexusResources", __FUNCTION__);
        return -1;
    }

    nexus_handle[iPlayerIndex].file = sessionSetupStatus.u.http.file;

    bcmPlayer_widthHeightCallback(iPlayerIndex,
           sessionSetupStatus.u.http.psi.videoWidth,
           sessionSetupStatus.u.http.psi.videoHeight);

    return 0;
}

static int bcmPlayer_init_ip(int iPlayerIndex) 
{
    int                 rc = 0;
    pthread_attr_t      attr;
    struct sched_param  param;

    BCMPLAYER_IP_DBG;

    nexus_handle[iPlayerIndex].ipHandle  = (BcmPlayerIpHandle *) malloc(sizeof(BcmPlayerIpHandle));
    if (nexus_handle[iPlayerIndex].ipHandle == NULL) {
        LOGE("%s: Couldn't allocate IP handle", __FUNCTION__);
        return 1;
    }

    memset(nexus_handle[iPlayerIndex].ipHandle, 0, sizeof(BcmPlayerIpHandle));

    B_Os_Init();

    nexus_handle[iPlayerIndex].ipHandle->playbackIp = B_PlaybackIp_Open(NULL);
    if (nexus_handle[iPlayerIndex].ipHandle->playbackIp == NULL) {
        LOGE("%s: playbackIp == NULL", __FUNCTION__);
        // Can't continue without a playback_ip handle so free our handle
        // to cause set data source to fail.
        free(nexus_handle[iPlayerIndex].ipHandle);
        nexus_handle[iPlayerIndex].ipHandle = NULL;
    }

    rc = bcmPlayer_init_base(iPlayerIndex);

    if (rc == 0) {
        // Now connect the client resources
        b_refsw_client_client_info client_info;
        nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

        b_refsw_client_connect_resource_settings connectSettings;

        BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));
        
        connectSettings.simpleVideoDecoder[0].id = client_info.videoDecoderId;
        connectSettings.simpleVideoDecoder[0].surfaceClientId = client_info.surfaceClientId;
        connectSettings.simpleVideoDecoder[0].windowId = iPlayerIndex; /* Main or PIP Window */
        if (nexus_handle[iPlayerIndex].bSupportsHEVC == true)
        {
            if ((nexus_handle[iPlayerIndex].maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                (nexus_handle[iPlayerIndex].maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz))
            {
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxWidth = 3840;
                connectSettings.simpleVideoDecoder[0].decoderCaps.maxHeight = 2160;
            }
            connectSettings.simpleVideoDecoder[0].decoderCaps.supportedCodecs[NEXUS_VideoCodec_eH265] = NEXUS_VideoCodec_eH265;
        }
        connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;

        if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true) {
            LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
            rc = 1;
        }
        else {
            nexus_handle[iPlayerIndex].ipHandle->preChargeTime    = BCMPLAYER_IP_PRECHARGE_TIME_SEC;
            nexus_handle[iPlayerIndex].ipHandle->firstBuff        = true;
            nexus_handle[iPlayerIndex].ipHandle->monitoringBuffer = false;
            nexus_handle[iPlayerIndex].ipHandle->endOfStream      = false;

#ifdef ENABLE_BUFFER_MONITORING

            pthread_attr_init(&attr); /* get default attributes */
            pthread_attr_setschedpolicy(&attr, SCHED_RR);
            param.sched_priority = 60;    /* check min/max */
            pthread_attr_setschedparam(&attr, &param);
            
            if (bcmPlayer_ip_bufferingThreadId[iPlayerIndex] == (pthread_t)NULL)
            {
                rc = pthread_create (
                  &bcmPlayer_ip_bufferingThreadId[iPlayerIndex],
                  NULL,
                  bcmPlayerIpBufferMonitoringThread,
                  (void *)(nexus_handle[iPlayerIndex].ipHandle)
                  );
                pthread_attr_destroy(&attr);

                if (rc != 0) {
                    LOGE("%s: failed to create bufferingThread %d", __FUNCTION__, rc);
                }
                else {
                    nexus_handle[iPlayerIndex].ipHandle->runMonitoringThread = true;
                    LOGV ("created buffering thread\n");
                }
            }
#endif
        }
    }
    return rc;
}

static int bcmPlayer_start_ip(int iPlayerIndex) 
{
    B_PlaybackIpError   rc;
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    BcmPlayerIpHandle*  ipHandle = nexusHandle->ipHandle;
    B_PlaybackIpSessionStartSettings sessionStartSettings;
    B_PlaybackIpSessionStartStatus sessionStartStatus;
    B_PlaybackIpSettings playbackIpSettings;

    BCMPLAYER_IP_DBG;

    if (ipHandle == NULL || ipHandle->playbackIp == NULL) 
    {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    if (nexusHandle->state != eBcmPlayerStatePaused) {

        if (nexusHandle->simpleVideoDecoder && ipHandle->psi.videoCodec) {
            NEXUS_SimpleVideoDecoderStartSettings videoProgram;

            NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);
            videoProgram.settings.codec = ipHandle->psi.videoCodec;
            videoProgram.settings.stcChannel = nexusHandle->stcChannel;
            videoProgram.settings.pidChannel = nexusHandle->videoPidChannel;
            rc = NEXUS_SimpleVideoDecoder_Start(nexusHandle->simpleVideoDecoder, &videoProgram);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: NEXUS_SimpleVideoDecoder_Start returned : %d", __FUNCTION__, rc);
                return -1;
            }
        }

        if (nexusHandle->simpleAudioDecoder && ipHandle->psi.audioCodec) {
            NEXUS_SimpleAudioDecoderStartSettings audioProgram;

            NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
            audioProgram.primary.codec = ipHandle->psi.audioCodec;
            audioProgram.primary.stcChannel = nexusHandle->stcChannel;
            audioProgram.primary.pidChannel = nexusHandle->audioPidChannel;
            NEXUS_SimpleAudioDecoder_Start(nexusHandle->simpleAudioDecoder, &audioProgram);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: NEXUS_SimpleAudioDecoder_Start returned : %d", __FUNCTION__, rc);
                return -1;
            }
        }

        if (ipHandle->usePlaypump) {
            rc = NEXUS_Playpump_Start(nexusHandle->playpump);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: NEXUS_Playpump_Start returned : %d", __FUNCTION__, rc);
                return -1;
            }
        } else {
            rc = NEXUS_Playback_Start(nexusHandle->playback, nexusHandle->file, NULL);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: NEXUS_Playback_Start returned : %d", __FUNCTION__, rc);
                return -1;
            }
        }

        memset(&sessionStartSettings, 0, sizeof(sessionStartSettings));
        sessionStartSettings.nexusHandles.playpump = nexusHandle->playpump;
        sessionStartSettings.nexusHandles.playback = nexusHandle->playback;
        sessionStartSettings.nexusHandles.simpleVideoDecoder = nexusHandle->simpleVideoDecoder;
        sessionStartSettings.nexusHandles.simpleAudioDecoder = nexusHandle->simpleAudioDecoder;
        sessionStartSettings.nexusHandles.stcChannel = nexusHandle->stcChannel;
        sessionStartSettings.nexusHandlesValid = true;
        sessionStartSettings.mpegType = ipHandle->psi.mpegType;

        rc = B_PlaybackIp_SessionStart(ipHandle->playbackIp, &sessionStartSettings, &sessionStartStatus);
        if (rc != B_ERROR_SUCCESS) {
            LOGE("%s: B_PlaybackIp_SessionStart returned: %d", __FUNCTION__, rc);
            return -1;
        }

    }
    else {
        rc = B_PlaybackIp_Play(ipHandle->playbackIp);
        if (rc != B_ERROR_SUCCESS) {
            LOGE("%s: B_PlaybackIp_Play returned: %d", __FUNCTION__, rc);
            return -1;
        }
    }
    
    if (bcmPlayer_ip_bufferingThreadId[iPlayerIndex] != (pthread_t)NULL)
    {
        ipHandle->monitoringBuffer = true;
        ipHandle->endOfStream      = false;
    }

#if WA_SWANDROID_372
    // set audio rate to normal, in case it was paused earlier
    NEXUS_AudioDecoderTrickState audioTrick;
    NEXUS_SimpleAudioDecoder_GetTrickState(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioTrick);
    if(audioTrick.rate == 0)
    {
        LOGD("%s: Changing audio from pause to normal decode rate",__FUNCTION__);
        audioTrick.rate = NEXUS_NORMAL_DECODE_RATE; 
        NEXUS_SimpleAudioDecoder_SetTrickState(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioTrick);        
    }
#endif 

    return 0;
}

static int bcmPlayer_isPlaying_ip(int iPlayerIndex) {
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpStatus playbackIpStatus;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    rc = B_PlaybackIp_GetStatus(ipHandle->playbackIp, &playbackIpStatus);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_GetStatus returned : %d\n", __FUNCTION__, rc);
        return -1;
    }

    LOGV("[%d]%s: isPlaying=%d", iPlayerIndex, __FUNCTION__, (playbackIpStatus.ipState == B_PlaybackIpState_ePlaying)?1:0);
    return (playbackIpStatus.ipState == B_PlaybackIpState_ePlaying)?1:0;
}

int bcmPlayer_pause_ip(int iPlayerIndex) 
{
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpTrickModesSettings ipTrickModeSettings;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) 
    {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    rc = B_PlaybackIp_GetTrickModeSettings(ipHandle->playbackIp, &ipTrickModeSettings);
    if (rc != B_ERROR_SUCCESS) 
    {
        LOGE("%s: B_PlaybackIp_GetTrickModeSettings returned : %d\n", __FUNCTION__, rc);
        return -1;
    }

    /* Must use connection stalling for HTTPS content */
    ipTrickModeSettings.pauseMethod = B_PlaybackIpPauseMethod_UseConnectionStalling;

    rc = B_PlaybackIp_Pause(ipHandle->playbackIp, &ipTrickModeSettings);
    if (rc != B_ERROR_SUCCESS) 
    {
        LOGE("%s: B_PlaybackIp_Pause returned : %d\n", __FUNCTION__, rc);
        return -1;
    }

#if WA_SWANDROID_372
    /* B_PlaybackIp_Pause() is pausing the playback/playpump, not the decoder. As a result,
       the residual audio stream in CDB/ITB continue to play for substantial time. Hence, pausing 
       the audio decoder here. We set to normal playback rate in bcmPlayer_start)ip(). */
    LOGD("%s: Pausing audio explicitly on playback_ip pause",__FUNCTION__);
    NEXUS_AudioDecoderTrickState audioTrick;
    NEXUS_SimpleAudioDecoder_GetTrickState(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioTrick);
    audioTrick.rate = 0; //pause
    NEXUS_SimpleAudioDecoder_SetTrickState(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioTrick);        
#endif

    return 0;
}

int bcmPlayer_seekTo_ip(int iPlayerIndex, int msec) {
    bcmPlayer_base_nexus_handle_t *nexusHandle = &nexus_handle[iPlayerIndex];
    BcmPlayerIpHandle* ipHandle = nexusHandle->ipHandle;
    B_PlaybackIpTrickModesSettings ipTrickModeSettings;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    /* Don't perform a PlaybackIP Seek until it has started... */
    if (nexus_handle->state != eBcmPlayerStatePrepared) {

#if WA_SWANDROID_372
        if (nexusHandle->simpleAudioDecoder && ipHandle->psi.audioCodec) {
            // flush any residual audio in seek
            LOGD("%s: Flushing residual audio on seek",__FUNCTION__);
            NEXUS_SimpleAudioDecoder_Flush(nexus_handle->simpleAudioDecoder);
        }
#endif 

        rc = B_PlaybackIp_GetTrickModeSettings(ipHandle->playbackIp, &ipTrickModeSettings);
        if (rc != B_ERROR_SUCCESS) {
            LOGE("%s: B_PlaybackIp_GetTrickModeSettings returned : %d\n", __FUNCTION__, rc);
            return -1;
        }

        ipTrickModeSettings.pauseMethod = B_PlaybackIpPauseMethod_UseConnectionStalling;
        ipTrickModeSettings.seekPosition = msec;
        rc = B_PlaybackIp_Seek(ipHandle->playbackIp, &ipTrickModeSettings);
        if (rc != B_ERROR_SUCCESS) {
            LOGE("%s: B_PlaybackIp_Seek returned : %d\n", __FUNCTION__, rc);
            return -1;
        }
    }
    nexus_handle->seekPositionMs = msec;

    return 0;
}

int bcmPlayer_getCurrentPosition_ip(int iPlayerIndex, int *msec) {
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpStatus playbackIpStatus;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    if (msec == NULL) {
        LOGE("%s: msec == NULL", __FUNCTION__);
        return -1;
    }

    rc = B_PlaybackIp_GetStatus(ipHandle->playbackIp, &playbackIpStatus);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_GetStatus returned : %d\n", __FUNCTION__, rc);
        return -1;
    }

    *msec = playbackIpStatus.position;
    // If audio only ES, then bmedia seems to reset the current position after a seek.
    if (!ipHandle->psi.videoCodec) {
        *msec += nexus_handle[iPlayerIndex].seekPositionMs;
    }
    LOGV("[%d]%s: position=%dms", iPlayerIndex, __FUNCTION__, *msec);

    return 0;
}

int bcmPlayer_getDuration_ip(int iPlayerIndex, int *msec) {
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    if (msec == NULL) {
        LOGE("%s: msec == NULL", __FUNCTION__);
        return -1;
    }

    *msec = ipHandle->psi.duration;

    LOGV("[%d]%s: duration=%dms", iPlayerIndex, __FUNCTION__, *msec);

    return 0;
}

static int bcmPlayer_stop_ip (int iPlayerIndex) 
{
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    B_PlaybackIpError rc;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    rc = B_PlaybackIp_SessionStop(ipHandle->playbackIp);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_SessionStop returned : %d\n", __FUNCTION__, rc);
        return -1;
    }

    ipHandle->monitoringBuffer = false;
    nexus_handle[iPlayerIndex].seekPositionMs = 0;

    return 0;
}

int bcmPlayer_getMediaExtractorFlags_ip(int iPlayerIndex, unsigned *flags)
{
    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    char *url = nexus_handle[iPlayerIndex].url;

    BCMPLAYER_IP_DBG;

    if(ipHandle == NULL || ipHandle->playbackIp == NULL) {
        LOGE("%s: ipHandle == NULL || ipHandle->playbackIp == NULL", __FUNCTION__);
        return -1;
    }

    if (flags == NULL) {
        LOGE("%s: flags == NULL", __FUNCTION__);
        return -1;
    }

    if ((strncmp(url, "http://", 7) == 0) || (strncmp(url, "https://", 8) == 0)) {
        /* If we are an HLS unbounded live stream then, we cannot enable trick-play operations */
        if (!((ipHandle->psi.hlsSessionEnabled == true) && (ipHandle->psi.duration == 9999999))) {
            *flags = ( CAN_SEEK_BACKWARD |
                       CAN_SEEK_FORWARD  |
                       CAN_PAUSE         |
                       CAN_SEEK );
        }
    }
    else {
        *flags = 0;
    }

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

void bcmPlayerIp_endOfStreamCallback (void *context, int param)
{
    B_PlaybackIpError   rc = 0;
    BcmPlayerIpHandle*  ipHandle = nexus_handle[param].ipHandle;
    B_PlaybackIpStatus  playbackIpStatus;
    uint32_t            waitCount = 0;

    BCMPLAYER_IP_DBG;

    if (ipHandle != NULL)
    {
        /* Make sure buffering is stopped. */
        /* Can't do playback IP pause in B_PlaybackIpState_eBuffering. */
        if (ipHandle->endOfStream == false)
        {
            ipHandle->endOfStream = true;  
            LOGV("wait for buffering thread to loop\n");
            do 
            {
                BKNI_Sleep (100);
                waitCount++;
                LOGV("%s waiting for buffering thread to loop %d", __FUNCTION__, (int)waitCount);
            } while ((ipHandle->monitoringBuffer == true) && (waitCount < 20));
        }
    }
    bcmPlayer_endOfStreamCallback (context, param);
    return;
}

void player_reg_ip(bcmPlayer_func_t *pFuncs){
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_ip;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_ip;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_ip;
    pFuncs->bcmPlayer_prepare_func = bcmPlayer_prepare_ip;
    pFuncs->bcmPlayer_start_func = bcmPlayer_start_ip;
    pFuncs->bcmPlayer_stop_func = bcmPlayer_stop_ip;
    pFuncs->bcmPlayer_pause_func = bcmPlayer_pause_ip;
    pFuncs->bcmPlayer_isPlaying_func = bcmPlayer_isPlaying_ip;
    pFuncs->bcmPlayer_seekTo_func = bcmPlayer_seekTo_ip;
    pFuncs->bcmPlayer_getCurrentPosition_func = bcmPlayer_getCurrentPosition_ip;
    pFuncs->bcmPlayer_getDuration_func = bcmPlayer_getDuration_ip;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_ip;
}

void ipPlaybackIpEventCallback (void * appCtx, B_PlaybackIpEventIds eventId) {

    BCMPLAYER_IP_DBG;

    int iPlayerIndex = (int)appCtx;
    int durationMs;
    int positionMs;
    BcmPlayerIpHandle *ipHandle = nexus_handle[iPlayerIndex].ipHandle;

    LOGV("%s: eventId: %d", __FUNCTION__, eventId);

    // For HLS when we reach the EOS, we need to force an EOS callback as it doesn't occur otherwise.
    if (eventId == B_PlaybackIpEvent_eServerEndofStreamReached) {
        if (bcmPlayer_getDuration_ip(iPlayerIndex, &durationMs) == 0) {
            if (bcmPlayer_getCurrentPosition_ip(iPlayerIndex, &positionMs) == 0) {
                if (positionMs >= durationMs) {
                    bcmPlayerIp_endOfStreamCallback(NULL, iPlayerIndex);
                }
            }
        }
    }
    return;
}

void ipSetTransportUsage(BcmPlayerIpHandle *ipHandle) {

    BCMPLAYER_IP_DBG;

    if (ipHandle == NULL) {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return;
    }

    // Additional factors may affect this.
    if (ipHandle->psi.hlsSessionEnabled 
        || ipHandle->psi.mpegType == NEXUS_TransportType_eTs) {
        ipHandle->usePlaypump = true;
    } else {
        ipHandle->usePlaypump = false;
    }

    return;
}

int ipParseURL(char *urlstr, char *hostname, unsigned int *portnum, char **uri) {
    char *hostPtr, *portPtr, *uriPtr;

    BCMPLAYER_IP_DBG;

    if (!urlstr || !hostname || !portnum || !uri) {
        LOGE("%s: !urlstr || !hostname || !portnum || !uri ", __FUNCTION__);
        return -1;
    }

    // Skip over the protocol if it's there
    hostPtr = strstr(urlstr, "://");
    if (hostPtr) {
        hostPtr +=3;
    } else {
        // Not found
        hostPtr = urlstr;
    }

    // Find the port if it's there
    uriPtr = strstr(hostPtr, "/");
    portPtr = strstr(hostPtr, ":");
    if (portPtr) {
       if (portPtr < uriPtr || !uriPtr) {
            if (atoi(portPtr+1))
                *portnum = atoi(portPtr+1);
            // We can now copy the hostname as we know where it ends
            // Hostname can only be 255 chars
            memset(hostname, 0, 256);
            strncpy(hostname, hostPtr, ((portPtr - hostPtr) < 255)? (portPtr - hostPtr) : 255);
       } else {
           portPtr = NULL;  // Reset Port Pointer
       }
    }

    if (!portPtr && uriPtr) {
        // No port specified so still need to fill in hostname
        memset(hostname, 0, 256);
        strncpy(hostname, hostPtr, ((uriPtr - hostPtr) < 255)? (uriPtr - hostPtr) : 255);
    } else if (!portPtr && !uriPtr) {
        // No port and no uri, copy the rest
        memset(hostname, 0, 256);
        strncpy(hostname, hostPtr, 255);
    }

    *uri = uriPtr;

    LOGI("%s: Hostname %s", __FUNCTION__, hostname);
    LOGI("%s: Port %d", __FUNCTION__, *portnum);
    LOGI("%s: URI %s", __FUNCTION__, uri? *uri : "");

    return 0;
}

bool ipSetOpenSessionParams(int iPlayerIndex, B_PlaybackIpSessionOpenSettings *sessionOpenSettings) {

    BCMPLAYER_IP_DBG;

    BcmPlayerIpHandle* ipHandle = nexus_handle[iPlayerIndex].ipHandle;
    char *url = nexus_handle[iPlayerIndex].url;

    if (sessionOpenSettings == NULL || ipHandle == NULL) {
        LOGE("%s: sessionOpenSettings == NULL  || ipHandle == NULL", __FUNCTION__);
        return false;
    }

    memset(sessionOpenSettings, 0, sizeof(*sessionOpenSettings));

    if (strncmp("http://", url, 7) == 0){
        sessionOpenSettings->socketOpenSettings.protocol = B_PlaybackIpProtocol_eHttp;
        sessionOpenSettings->socketOpenSettings.port = 80;
    } else if (strncmp("https://", url, 8) == 0) {
        sessionOpenSettings->socketOpenSettings.protocol = B_PlaybackIpProtocol_eHttp;
        sessionOpenSettings->socketOpenSettings.port = 443;
        sessionOpenSettings->security.securityProtocol = B_PlaybackIpSecurityProtocol_Ssl;

        B_PlaybackIpSslInitSettings sslInitSettings;
		memset(&sslInitSettings, 0, sizeof(B_PlaybackIpSslInitSettings));
		sslInitSettings.rootCaCertPath=(char *)B_PLAYBACK_IP_CA_CERT;
		sslInitSettings.clientAuth=false;
		sslInitSettings.ourCertPath=NULL;
		sslInitSettings.privKeyPath=NULL;
		sslInitSettings.password=NULL;
        if (ipHandle->sslCtx == NULL) {
            ipHandle->sslCtx = B_PlaybackIp_SslInit(&sslInitSettings);
        }
		if (!ipHandle->sslCtx) {
			LOGE("%s: SSL Security initialization failed!", __FUNCTION__);
			return false;
		}
        sessionOpenSettings->security.initialSecurityContext = ipHandle->sslCtx;
    } else if (strncmp("rtp://", url, 6) == 0) {
        sessionOpenSettings->socketOpenSettings.protocol = B_PlaybackIpProtocol_eRtpNoRtcp;
        sessionOpenSettings->socketOpenSettings.port = 5004;
    } else if (strncmp("udp://", url, 6) == 0) {
        sessionOpenSettings->socketOpenSettings.protocol = B_PlaybackIpProtocol_eUdp;
        sessionOpenSettings->socketOpenSettings.port = 5004;
    } else {
        LOGE("%s: Cannot get protocol for URL : %s", __FUNCTION__, url);
        return false;
    }

    ipParseURL(url, 
            sessionOpenSettings->socketOpenSettings.ipAddr,
            &sessionOpenSettings->socketOpenSettings.port,
            &sessionOpenSettings->socketOpenSettings.url);

    if (sessionOpenSettings->socketOpenSettings.protocol == B_PlaybackIpProtocol_eHttps
        || sessionOpenSettings->socketOpenSettings.protocol == B_PlaybackIpProtocol_eHttp) {
        sessionOpenSettings->ipMode = B_PlaybackIpClockRecoveryMode_ePull;
        sessionOpenSettings->networkTimeout = 5;    /* 5s timeout for network outage events */
        sessionOpenSettings->u.http.networkBufferSize = BCMPLAYER_NETWORK_BUFFER_SIZE;
        sessionOpenSettings->u.http.userAgent = userAgent;
        sessionOpenSettings->nonBlockingMode = false;
        sessionOpenSettings->eventCallback = ipPlaybackIpEventCallback;
        sessionOpenSettings->appCtx = (void *)iPlayerIndex;
    } else if (sessionOpenSettings->socketOpenSettings.protocol == B_PlaybackIpProtocol_eRtpNoRtcp
        || sessionOpenSettings->socketOpenSettings.protocol == B_PlaybackIpProtocol_eUdp) {
        sessionOpenSettings->ipMode = B_PlaybackIpClockRecoveryMode_ePushWithPcrSyncSlip;
        sessionOpenSettings->maxNetworkJitter = 300;
        sessionOpenSettings->networkTimeout = 1;  /* timeout in 1 sec during network outage events */
        sessionOpenSettings->nonBlockingMode = false;
    }

    return true;
}

bool ipSetSetupSessionParams(BcmPlayerIpHandle *ipHandle, B_PlaybackIpSessionSetupSettings *sessionSetupSettings) {

    BCMPLAYER_IP_DBG;

    if (sessionSetupSettings == NULL || ipHandle == NULL) {
        LOGE("%s: sessionSetupSettings == NULL  || ipHandle == NULL", __FUNCTION__);
        return false;
    }

    B_PlaybackIp_GetDefaultSessionSetupSettings(sessionSetupSettings);

    sessionSetupSettings->u.http.skipPsiParsing = false;
    sessionSetupSettings->u.http.enablePayloadScanning = true; /* turn on the deep packet inspection */
    sessionSetupSettings->u.http.avgBitRate = 20000000;

    /* set a limit on how long the psi parsing should continue before returning */
    sessionSetupSettings->u.http.psiParsingTimeLimit = 3 * 1000; /* Wait up to 3s for PSI information to be parsed */

    return true;
}

bool ipSetupNexusResources(bcmPlayer_base_nexus_handle_t *nexusHandle, int iPlayerIndex) {
    NEXUS_Error               rc;
    BcmPlayerIpHandle*        ipHandle;
    B_PlaybackIpSettings      playbackIpSettings;
    NEXUS_StcChannelSettings  stcChannelSettings;
    NEXUS_ClientConfiguration clientConfig;

    BCMPLAYER_IP_DBG;

    if (nexusHandle == NULL) {
        LOGE("%s: nexusHandle == NULL", __FUNCTION__);
        return false;
    }

    ipHandle = nexusHandle->ipHandle;
    if (ipHandle == NULL) {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return false;
    }

    ipHandle->timebase = NEXUS_Timebase_Open(NEXUS_ANY_ID);
    if (ipHandle->timebase == NEXUS_Timebase_eInvalid 
        || ipHandle->timebase >= NEXUS_Timebase_eMax) {
        LOGE("%s: ipHandle->timebase == NULL", __FUNCTION__);
        return false;
    }

    NEXUS_StcChannel_GetDefaultSettings(NEXUS_ANY_ID, &stcChannelSettings);
    stcChannelSettings.timebase = ipHandle->timebase;
    stcChannelSettings.autoConfigTimebase = false;
    stcChannelSettings.mode = NEXUS_StcChannelMode_eAuto;
    nexusHandle->stcChannel = NEXUS_StcChannel_Open(NEXUS_ANY_ID, &stcChannelSettings);
    if (nexusHandle->stcChannel == NULL) {
        LOGE("%s: stcChannel == NULL", __FUNCTION__);
        return false;
    }

    if (ipHandle->usePlaypump) 
    {
        NEXUS_PlaypumpSettings      playpumpSettings;

        NEXUS_Playpump_GetSettings(nexusHandle->playpump, &playpumpSettings);
        playpumpSettings.mode = NEXUS_PlaypumpMode_eFifo;
        playpumpSettings.transportType = nexusHandle->ipHandle->psi.mpegType;
        if (nexusHandle->ipHandle->psi.transportTimeStampEnabled) {
            playpumpSettings.timestamp.type = NEXUS_TransportTimestampType_eMod300;
            playpumpSettings.timestamp.pacing = false;
            LOGD("Setting timestamp flag");
        }
        rc = NEXUS_Playpump_SetSettings(nexusHandle->playpump, &playpumpSettings);
        if (rc != NEXUS_SUCCESS) 
        {
            LOGE("%s: NEXUS_Playpump_SetSettings returned : %d", __FUNCTION__, rc);
            return false;
        }
    } 
    else 
    {
        NEXUS_PlaybackSettings playbackSettings;
        NEXUS_CallbackDesc endOfStreamCallbackDesc;
        NEXUS_Playback_GetSettings(nexusHandle->playback, &playbackSettings);
        playbackSettings.endOfStreamAction = NEXUS_PlaybackLoopMode_eLoop;
        playbackSettings.playpump = nexusHandle->playpump;
        playbackSettings.playpumpSettings.transportType = nexusHandle->ipHandle->psi.mpegType;
        playbackSettings.stcChannel = nexusHandle->stcChannel;

        endOfStreamCallbackDesc.callback = bcmPlayerIp_endOfStreamCallback;
        endOfStreamCallbackDesc.context = NULL;
        endOfStreamCallbackDesc.param = iPlayerIndex;
        playbackSettings.endOfStreamCallback = endOfStreamCallbackDesc;

        rc = NEXUS_Playback_SetSettings(nexusHandle->playback, &playbackSettings);
        if (rc != NEXUS_SUCCESS) 
        {
            LOGE("%s: NEXUS_Playback_SetSettings returned : %d", __FUNCTION__, rc);
            return false;
        }
    }

    if (nexusHandle->simpleVideoDecoder && ipHandle->psi.videoCodec) {
        nexusHandle->videoPidChannel = ipGetVideoPidChannel(nexusHandle);
        if (nexusHandle->videoPidChannel == NULL) {
            LOGE("%s: nexusHandle->videoPidChannel == NULL", __FUNCTION__);
            return false;
        }
    }

    if (nexusHandle->simpleAudioDecoder && ipHandle->psi.audioCodec) {
        nexusHandle->audioPidChannel = ipGetAudioPidChannel(nexusHandle);
        if (nexusHandle->audioPidChannel == NULL) {
            LOGE("%s: nexusHandle->audioPidChannel == NULL", __FUNCTION__);
            return false;
        }
    }

    rc = B_PlaybackIp_GetSettings(ipHandle->playbackIp, &playbackIpSettings);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_GetSettings returned: %d", __FUNCTION__, rc);
        return -1;
    }

    playbackIpSettings.nexusHandlesValid = true;
    if (ipHandle->usePlaypump) {
        playbackIpSettings.nexusHandles.playpump = nexusHandle->playpump;
    } else {
        playbackIpSettings.nexusHandles.playback = nexusHandle->playback;
    }
    playbackIpSettings.nexusHandles.simpleVideoDecoder = nexusHandle->simpleVideoDecoder;
    playbackIpSettings.nexusHandles.simpleAudioDecoder = nexusHandle->simpleAudioDecoder;
    playbackIpSettings.nexusHandles.stcChannel         = nexusHandle->stcChannel;
    playbackIpSettings.nexusHandles.videoPidChannel    = nexusHandle->videoPidChannel;
    playbackIpSettings.nexusHandles.audioPidChannel    = nexusHandle->audioPidChannel;
    playbackIpSettings.useNexusPlaypump = ipHandle->usePlaypump;
#ifdef ENABLE_BUFFER_MONITORING
    playbackIpSettings.preChargeBuffer = true;
#endif

    rc = B_PlaybackIp_SetSettings(ipHandle->playbackIp, &playbackIpSettings);
    if (rc != B_ERROR_SUCCESS) {
        LOGE("%s: B_PlaybackIp_SetSettings returned: %d", __FUNCTION__, rc);
        return -1;
    }
    return true;
}

NEXUS_PidChannelHandle ipGetVideoPidChannel(bcmPlayer_base_nexus_handle_t *nexusHandle) {
    BcmPlayerIpHandle *ipHandle;

    BCMPLAYER_IP_DBG;

    if (nexusHandle == NULL) {
        LOGE("%s: nexusHandle == NULL", __FUNCTION__);
        return NULL;
    }

    ipHandle = nexusHandle->ipHandle;

    if (ipHandle == NULL) {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return NULL;
    }

    if (ipHandle->usePlaypump) {
        NEXUS_PlaypumpOpenPidChannelSettings playpumpPidSettings;

        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&playpumpPidSettings);
        playpumpPidSettings.pidType = NEXUS_PidType_eVideo;
        return NEXUS_Playpump_OpenPidChannel(nexusHandle->playpump,
                                             ipHandle->psi.videoPid,
                                             &playpumpPidSettings);
    } else {
        NEXUS_PlaybackPidChannelSettings playbackPidSettings;

        NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
        playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
        playbackPidSettings.pidTypeSettings.video.codec = ipHandle->psi.videoCodec;
        playbackPidSettings.pidTypeSettings.video.index = true;
        playbackPidSettings.pidTypeSettings.video.simpleDecoder = nexusHandle->simpleVideoDecoder;
        return NEXUS_Playback_OpenPidChannel(nexusHandle->playback,
                                             ipHandle->psi.videoPid,
                                             &playbackPidSettings);
    }

    return NULL;
}

NEXUS_PidChannelHandle ipGetAudioPidChannel(bcmPlayer_base_nexus_handle_t *nexusHandle) {
    BcmPlayerIpHandle *ipHandle;

    NEXUS_PidChannelHandle pidCh = NULL;

    BCMPLAYER_IP_DBG;

    if (nexusHandle == NULL) {
        LOGE("%s: nexusHandle == NULL", __FUNCTION__);
        return NULL;
    }

    ipHandle = nexusHandle->ipHandle;

    if (ipHandle == NULL) {
        LOGE("%s: ipHandle == NULL", __FUNCTION__);
        return NULL;
    }

    if (ipHandle->usePlaypump) {
        NEXUS_PlaypumpOpenPidChannelSettings playpumpPidSettings;

        NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&playpumpPidSettings);
        playpumpPidSettings.pidType = NEXUS_PidType_eAudio;
        playpumpPidSettings.pidTypeSettings.audio.codec = ipHandle->psi.audioCodec;
        pidCh = NEXUS_Playpump_OpenPidChannel(nexusHandle->playpump,
                                             ipHandle->psi.audioPid,
                                             &playpumpPidSettings);
    } else {
        NEXUS_PlaybackPidChannelSettings playbackPidSettings;
        NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
        playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eAudio;
        playbackPidSettings.pidSettings.pidTypeSettings.audio.codec = ipHandle->psi.audioCodec;
        playbackPidSettings.pidTypeSettings.audio.simpleDecoder = nexusHandle->simpleAudioDecoder;
        pidCh = NEXUS_Playback_OpenPidChannel(nexusHandle->playback,
                                             ipHandle->psi.audioPid,
                                             &playbackPidSettings);
    }

    return pidCh;
}
