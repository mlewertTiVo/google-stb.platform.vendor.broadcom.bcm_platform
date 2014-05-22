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
 * $brcm_Workfile: ip_psi.h $
 * $brcm_Revision: 13 $
 * $brcm_Date: 10/16/12 6:49p $
 * 
 * Module Description:
 *  header file for PSI acquisition
 * 
 * Revision History:
 * 
 * $brcm_Log: /nexus/lib/playback_ip/apps/ip_psi.h $
 * 
 * 13   10/16/12 6:49p ssood
 * SW7445-53: get playback_ip to compile for 7445 tool-chain
 * 
 * 12   7/26/12 8:33p ssood
 * SW7346-944: add support to stream from OFDM-capable frontend
 * 
 * 11   5/17/12 6:36p ssood
 * SW7425-3042: simplified build flags & code by using nexus multi process
 *  capabilities
 * 
 * 10   7/14/11 12:01p ssood
 * SW7346-309: Add support for Streaming from Satellite Source
 * 
 * 9   1/27/10 10:18a ssood
 * SW7420-454: conditionally compile Live Streaming Code using
 *  LIVE_STREAMING_SUPPORT
 * 
 * 8   1/14/10 6:41p ssood
 * SW7408-47: Add support to compile IP Streamer App on platforms w/ no
 *  frontend support
 * 
 * 7   11/17/09 2:58p ssood
 * SW7420-454: Enhance IP Streamer to stream files from local disk
 * 
 * 6   10/20/09 6:33p ssood
 * SW7420-340: remove nexus playback, audio, video decoder dependencies
 *  for SMS compilation
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
 * 2   9/17/09 2:25p ssood
 * SW7420-340: include nexus files pertaining to ip streaming only
 * 
 ******************************************************************************/
#ifndef IP_PSI_H__
#define IP_PSI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bstd.h"
#include "bkni.h"
#include "blst_list.h"
#include "blst_queue.h"

#include "nexus_pid_channel.h"
#include "nexus_playpump.h"
#include "nexus_message.h"
#include "nexus_pid_channel.h"
#include "bstd.h"
#include "bkni.h"
#include <stdio.h>
#include <stdlib.h>
#include "b_playback_ip_lib.h"

#define NUM_PID_CHANNELS 4

typedef struct psiCollectionDataType
{
    NEXUS_PlaypumpHandle        playpump;
    B_PlaybackIpHandle          playbackIp;
    struct {
        uint16_t                    num;
        NEXUS_PidChannelHandle      channel;
        } pid[NUM_PID_CHANNELS];
} psiCollectionDataType,*pPsiCollectionDataType;

void acquirePsiInfo(pPsiCollectionDataType pCollectionData, B_PlaybackIpPsiInfo *psi, int programsToFind, int *numProgramsFound);
#ifdef __cplusplus
}
#endif
#endif /* IP_PSI_H__*/
