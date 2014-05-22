/******************************************************************************
 *    (c)2010-2012 Broadcom Corporation
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
 * $brcm_Workfile: stream_probe.h $
 * $brcm_Revision: 4 $
 * $brcm_Date: 9/19/12 12:46p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/stream_probe.h $
 * 
 * 4   9/19/12 12:46p mnelis
 * SWANDROID-78: Implement APK file playback.
 * 
 * 3   9/14/12 1:25p mnelis
 * SWANDROID-78: Recognise and play OGG files With Nexus.
 * 
 * 2   1/17/12 3:18p franktcc
 * SW7425-2196: Adding H.264 SVC/MVC codec support for 3d video
 * 
 * 2   7/28/11 3:38p franktcc
 * SW7420-2012: Use binder interface for handle sharing
 * 
 * 2   1/12/11 7:26p alexpan
 * SW7420-1240: Add html5 video support
 * 
 * 1   12/22/10 1:56p alexpan
 * SW7420-1240: add media_probe
 * 
 *****************************************************************************/

#ifndef _BCMPLAYER_STREAM_PROBE_
#define _BCMPLAYER_STREAM_PROBE_

#ifdef __cplusplus
extern "C" {
#endif


#include "nexus_types.h"
#include "nexus_video_types.h"
#include "nexus_audio_types.h"

enum StreamMediaExtractorFlags {
    CAN_SEEK_BACKWARD  = 1,  // the "seek 10secs back button"
    CAN_SEEK_FORWARD   = 2,  // the "seek 10secs forward button"
    CAN_PAUSE          = 4,
    CAN_SEEK           = 8,  // the "seek bar"
};

typedef struct stream_format_info_t {
    NEXUS_TransportType transportType;
    unsigned videoPid, extVideoPid, audioPid;
    NEXUS_VideoCodec videoCodec;
    NEXUS_AudioCodec audioCodec;
    uint16_t videoWidth; /* coded video width, or 0 if unknown */
    uint16_t videoHeight; /* coded video height, or 0 if unknown  */
    int64_t offset;
    bool m2ts;
}stream_format_info;

bool isAwesomePlayerAudioCodecSupported(NEXUS_AudioCodec audioCodec);
void probe_stream_format(const char *url, int videoTrackIndex, int audioTrackIndex, stream_format_info *p_stream_format_info);

#ifdef __cplusplus
}
#endif


#endif  
