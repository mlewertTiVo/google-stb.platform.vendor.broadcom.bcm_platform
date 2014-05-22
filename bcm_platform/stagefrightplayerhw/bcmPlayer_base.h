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
 * $brcm_Workfile: bcmPlayer_base.h $
 * $brcm_Revision: 8 $
 * $brcm_Date: 2/7/13 11:06a $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_base.h $
 * 
 * 8   2/7/13 11:06a mnelis
 * SWANDROID-274: Correct media state handling
 * 
 * 7   12/11/12 10:58p robertwm
 * SWANDROID-255: [BCM97346 & BCM97425] Widevine DRM and GTS support
 * 
 * SWANDROID-255/1   12/11/12 1:50a robertwm
 * SWANDROID-255:  [BCM97346 & BCM97425] Widevine DRM and GTS support.
 * 
 * 6   5/3/12 3:50p franktcc
 * SWANDROID-67: Adding UDP/RTP/RTSP streaming playback support
 * 
 * 5   2/24/12 4:11p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 4   12/29/11 6:46p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 3   12/15/11 11:43a franktcc
 * SW7420-1906: Adding capability of setAspectRatio to ICS
 * 
 * 2   12/10/11 7:17p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 6   11/28/11 6:10p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 5   9/22/11 4:57p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 4   9/19/11 5:22p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 3   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 1   8/22/11 4:04p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 *****************************************************************************/
#ifndef BCM_PLAYER_BASE_H
#define BCM_PLAYER_BASE_H

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C"
{
#endif

/* base bcmPlayer function */
void bcmPlayer_platformJoin(int);
void bcmPlayer_platformUnjoin(int);
int  bcmPlayer_init_base(int);
void bcmPlayer_uninit_base(int);
int  bcmPlayer_setDataSource_base(
        int, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader);
int  bcmPlayer_setDataSource_file_handle_base(int, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight);
int  bcmPlayer_setVideoSurface_base(int);
int  bcmPlayer_setVideoSurfaceTexture_base(int, buffer_handle_t bufferHandle, bool visible);
int  bcmPlayer_setAudioSink_base(int, bool connect);
int  bcmPlayer_prepare_base(int);
int  bcmPlayer_start_base(int);
int  bcmPlayer_stop_base(int);
int  bcmPlayer_pause_base(int);
int  bcmPlayer_isPlaying_base(int);
int  bcmPlayer_seekTo_base(int, int msec);
int  bcmPlayer_getCurrentPosition_base(int, int *msec);
int  bcmPlayer_getDuration_base(int, int *msec);
int  bcmPlayer_getFifoDepth_base(int, bcmPlayer_fifo_t fifo, int *depth);
int  bcmPlayer_reset_base(int);
int  bcmPlayer_setLooping_base(int, int loop);
int  bcmPlayer_suspend_base(int);
int  bcmPlayer_resume_base(int);
int  bcmPlayer_setAspectRatio_base(int iPlayerIndex, int ar);
void  bcmPlayer_config_base(int iPlayerIndex, const char* flag,const char* value);
int  bcmPlayer_putData_base(int iPlayerIndex, const void *data, size_t data_len);
int  bcmPlayer_putAudioData_base(int iPlayerIndex, const void *data, size_t data_len);
int  bcmPlayer_getMediaExtractorFlags_base(int iPlayerIndex, unsigned *flags);
int  bcmPlayer_selectTrack_base(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select);

#ifdef __cplusplus
}
#endif


#endif //BCM_PLAYER_BASE_H
