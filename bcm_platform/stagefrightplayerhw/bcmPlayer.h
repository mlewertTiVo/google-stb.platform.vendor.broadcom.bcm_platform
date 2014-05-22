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
 * $brcm_Workfile: bcmPlayer.h $
 * $brcm_Revision: 10 $
 * $brcm_Date: 2/14/13 1:36p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer.h $
 * 
 * 10   2/14/13 1:36p mnelis
 * SWANDROID-336: Refactor bcmplayer_ip code
 * 
 * 9   2/7/13 11:06a mnelis
 * SWANDROID-274: Correct media state handling
 * 
 * 8   12/11/12 10:58p robertwm
 * SWANDROID-255: [BCM97346 & BCM97425] Widevine DRM and GTS support
 * 
 * SWANDROID-241/SWANDROID-255/1   12/11/12 1:55a robertwm
 * SWANDROID-255:  [BCM97346 & BCM97425] Widevine DRM and GTS support.
 * 
 * SWANDROID-241/1   10/30/12 4:10p mnelis
 * SWANDROID-241 : Add RTSP support to Android
 * 
 * 6   6/21/12 3:07p franktcc
 * SWANDROID-120: Adding error callback function
 * 
 * 5   5/3/12 3:49p franktcc
 * SWANDROID-67: Adding UDP/RTP/RTSP streaming playback support
 * 
 * 4   2/24/12 4:11p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 3   12/29/11 6:44p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 2   12/10/11 7:16p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 7   11/28/11 6:10p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 6   11/18/11 7:43p alexpan
 * SW7425-1804: Revert udp and http source type changes
 * 
 * 5   11/9/11 5:15p zhangjq
 * SW7425-1701 : add more IP source type into bcmPlayer
 * 
 * 4   9/22/11 4:55p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 3   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 1   8/22/11 4:03p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 *
 *****************************************************************************/
#ifndef BCM_PLAYER_H
#define BCM_PLAYER_H


#include <stdio.h>
#include <utils/Log.h>
#include <utils/Timers.h>
#include <system/window.h>


#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    INPUT_SOURCE_LOCAL = 0,
    INPUT_SOURCE_IP,
    INPUT_SOURCE_LIVE,
    INPUT_SOURCE_HDMI_IN,
    INPUT_SOURCE_ANALOG_IN,
    INPUT_SOURCE_RAW_DATA,
    INPUT_SOURCE_RTSP,
    INPUT_SOURCE_MAX
} bcmPlayer_input_source;

typedef enum
{
    FIFO_VIDEO = 0,
    FIFO_AUDIO,
} bcmPlayer_fifo_t;

typedef enum 
{
    TRACK_TYPE_UNKNOWN = 0,
    TRACK_TYPE_VIDEO = 1,
    TRACK_TYPE_AUDIO = 2,
    TRACK_TYPE_TIMEDTEXT = 3,
} bcmPlayer_track_t;

/* declare bcmPlayer function */
typedef void (*onEventCbPtr)(int, int, int);
typedef int  (*bcmPlayer_init_func_t)(int);
typedef void (*bcmPlayer_uninit_func_t)(int);
typedef int  (*bcmPlayer_setDataSource_func_t)(int, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader);
typedef int  (*bcmPlayer_setDataSource_file_handle_func_t)(int, int, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight);
typedef int  (*bcmPlayer_setVideoSurface_func_t)(int);
typedef int  (*bcmPlayer_setVideoSurfaceTexture_func_t)(int, buffer_handle_t bufferHandle, bool visible);
typedef int  (*bcmPlayer_setAudioSink_func_t)(int, bool connect);
typedef int  (*bcmPlayer_prepare_func_t)(int);
typedef int  (*bcmPlayer_start_func_t)(int);
typedef int  (*bcmPlayer_stop_func_t)(int);
typedef int  (*bcmPlayer_pause_func_t)(int);
typedef int  (*bcmPlayer_isPlaying_func_t)(int);
typedef int  (*bcmPlayer_seekTo_func_t)(int, int msec);
typedef int  (*bcmPlayer_getCurrentPosition_func_t)(int, int *msec);
typedef int  (*bcmPlayer_getDuration_func_t)(int, int *msec);
typedef int  (*bcmPlayer_getFifoDepth_func_t)(int, bcmPlayer_fifo_t fifo, int *depth);
typedef int  (*bcmPlayer_reset_func_t)(int);
typedef int  (*bcmPlayer_setLooping_func_t)(int, int loop);
typedef int  (*bcmPlayer_suspend_func_t)(int);
typedef int  (*bcmPlayer_resume_func_t)(int);
typedef int  (*bcmPlayer_setAspectRatio_func_t)(int, int ar);
typedef void (*bcmPlayer_config_func_t)(int, const char* flag,const char* value);
typedef int  (*bcmPlayer_putData_func_t)(int, const void* data, size_t data_len);
typedef int  (*bcmPlayer_putAudioData_func_t)(int, const void* data, size_t data_len);
typedef int  (*bcmPlayer_getMediaExtractorFlags_func_t)(int, unsigned *flags);
typedef int  (*bcmPlayer_selectTrack_func_t)(int, bcmPlayer_track_t trackType, unsigned trackIndex, bool select);

typedef struct
{
    bcmPlayer_init_func_t 						      bcmPlayer_init_func;
    bcmPlayer_uninit_func_t						      bcmPlayer_uninit_func;
    bcmPlayer_setDataSource_func_t                    bcmPlayer_setDataSource_func;
    bcmPlayer_setDataSource_file_handle_func_t        bcmPlayer_setDataSource_file_handle_func;
    bcmPlayer_setVideoSurface_func_t                  bcmPlayer_setVideoSurface_func;
    bcmPlayer_setVideoSurfaceTexture_func_t           bcmPlayer_setVideoSurfaceTexture_func;
    bcmPlayer_setAudioSink_func_t                     bcmPlayer_setAudioSink_func;
    bcmPlayer_prepare_func_t                          bcmPlayer_prepare_func;
    bcmPlayer_start_func_t                            bcmPlayer_start_func;
    bcmPlayer_stop_func_t                             bcmPlayer_stop_func;
    bcmPlayer_pause_func_t                            bcmPlayer_pause_func;
    bcmPlayer_isPlaying_func_t                        bcmPlayer_isPlaying_func;
    bcmPlayer_seekTo_func_t                           bcmPlayer_seekTo_func;
    bcmPlayer_getCurrentPosition_func_t               bcmPlayer_getCurrentPosition_func;
    bcmPlayer_getDuration_func_t                      bcmPlayer_getDuration_func;
    bcmPlayer_getFifoDepth_func_t                     bcmPlayer_getFifoDepth_func;
    bcmPlayer_reset_func_t                            bcmPlayer_reset_func;
    bcmPlayer_setLooping_func_t                       bcmPlayer_setLooping_func;
    bcmPlayer_suspend_func_t                          bcmPlayer_suspend_func;
    bcmPlayer_resume_func_t                           bcmPlayer_resume_func;
    bcmPlayer_setAspectRatio_func_t                   bcmPlayer_setAspectRatio_func;
    bcmPlayer_config_func_t                           bcmPlayer_config_func;
    bcmPlayer_putData_func_t						  bcmPlayer_putData_func;
    bcmPlayer_putAudioData_func_t					  bcmPlayer_putAudioData_func;
    bcmPlayer_getMediaExtractorFlags_func_t           bcmPlayer_getMediaExtractorFlags_func;
    bcmPlayer_selectTrack_func_t                      bcmPlayer_selectTrack_func;
}bcmPlayer_func_t;

typedef void (*bcmPlayer_module_t)(bcmPlayer_func_t *pFuncs);

int bcmPlayer_init(int iPlayerIndex, onEventCbPtr onStreamDone);
void bcmPlayer_uninit(int iPlayerIndex);
int bcmPlayer_setDataSource(int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader);
int bcmPlayer_setDataSource_file_handle(int iPlayerIndex, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight);
int bcmPlayer_setVideoSurface(int iPlayerIndex);
int bcmPlayer_setVideoSurfaceTexture(int iPlayerIndex, buffer_handle_t bufferHandle, bool visible);
int bcmPlayer_setAudioSink(int iPlayerIndex, bool connect);
int bcmPlayer_prepare(int iPlayerIndex);
int bcmPlayer_start(int iPlayerIndex);
int bcmPlayer_stop(int iPlayerIndex);
int bcmPlayer_pause(int iPlayerIndex);
int bcmPlayer_isPlaying(int iPlayerIndex);
int bcmPlayer_seekTo(int iPlayerIndex, int msec);
int bcmPlayer_getCurrentPosition(int iPlayerIndex, int *msec);
int bcmPlayer_getDuration(int iPlayerIndex, int *msec);
int bcmPlayer_getFifoDepth(int iPlayerIndex, bcmPlayer_fifo_t fifo, int *depth);
int bcmPlayer_reset(int iPlayerIndex);
int bcmPlayer_setLooping(int iPlayerIndex, int loop);
int bcmPlayer_suspend(int iPlayerIndex);
int bcmPlayer_resume(int iPlayerIndex);
int  bcmPlayer_setAspectRatio(int iPlayerIndex, int ar);
void bcmPlayer_config(int iPlayerIndex, const char* flag,const char* value);
int  bcmPlayer_putData(int iPlayerIndex, const void *data, size_t data_len);
int  bcmPlayer_putAudioData(int iPlayerIndex, const void *data, size_t data_len);
int  bcmPlayer_getMediaExtractorFlags(int iPlayerIndex, unsigned *flags);
int  bcmPlayer_selectTrack(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select);
void bcmPlayer_endOfStreamCallback(void *context, int param);
void bcmPlayer_errorCallback(void *context, int param);
void bcmPlayer_widthHeightCallback(int param, int width, int height);
void bcmPlayer_bufferingPauseStartCallback (int param);
void bcmPlayer_bufferingPauseEndCallback (int param);

void player_reg_local(bcmPlayer_func_t *pFuncs);
void player_reg_ip(bcmPlayer_func_t *pFuncs);
void player_reg_hdmiIn(bcmPlayer_func_t *pFuncs);
void player_reg_analogIn(bcmPlayer_func_t *pFuncs);
void player_reg_live(bcmPlayer_func_t *pFuncs);
void player_reg_rawdata(bcmPlayer_func_t *pFuncs);
void player_reg_rtsp(bcmPlayer_func_t *pFuncs);
void player_reg_rtp(bcmPlayer_func_t *pFuncs);

void bcmPlayer_config_live(int iPlayerIndex, const char* flag,const char* value);
#ifdef __cplusplus
}
#endif


#endif//BCM_PLAYER_H
