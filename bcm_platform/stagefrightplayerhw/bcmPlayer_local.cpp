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
 * $brcm_Workfile: bcmPlayer_local.cpp $
 * $brcm_Revision: 15 $
 * $brcm_Date: 1/20/13 4:20p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_local.cpp $
 * 
 * 15   1/20/13 4:20p robertwm
 * SWANDROID-293: Media player doesn't work after running some apps.
 * 
 * 14   1/3/13 5:21p mnelis
 * SWANDROID-293: Remove extra prints and typo
 * 
 * 13   1/2/13 4:56p mnelis
 * SWANDROID-293: Don't use global for bof/eof offsets
 * 
 * 12   9/24/12 6:47p mnelis
 * SWANDROID-78: Fix HTML5 streaming behavior
 * 
 * 11   9/19/12 12:46p mnelis
 * SWANDROID-78: Implement APK file playback.
 * 
 * 10   9/14/12 1:31p mnelis
 * SWANDROID-78: Use NEXUS_ANY_ID for STC channel and retry playback_start
 *  without index if failure
 * 
 * 9   6/5/12 2:40p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 8   5/24/12 11:54a franktcc
 * SWANDROID-97: Adding setDataSource by file descriptor back to support
 * 
 * 7   2/24/12 4:12p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 6   2/8/12 2:54p kagrawal
 * SWANDROID-12: Initial support for Nexus client-server mode
 * 
 * 5   1/17/12 3:18p franktcc
 * SW7425-2196: Adding H.264 SVC/MVC codec support for 3d video
 * 
 * 4   12/29/11 6:45p franktcc
 * SW7425-2069: bcmPlayer code refactoring.
 * 
 * 3   12/13/11 10:28a franktcc
 * SW7420-1906: Adding capability of setAspectRatio to ICS
 * 
 * 2   12/10/11 7:20p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player
 * 
 * 9   11/28/11 6:12p franktcc
 * SW7425-1845: Adding end of stream callback to nexus media player.
 * 
 * 8   11/18/11 7:43p alexpan
 * SW7425-1804: Revert udp and http source type changes
 * 
 * 7   11/9/11 5:19p zhangjq
 * SW7425-1701 : add more IP source type into bcmPlayer
 * 
 * 6   10/7/11 11:26a franktcc
 * SW7420-2080: Should reset player once setDataSource failed.
 * 
 * 5   9/22/11 5:04p zhangjq
 * SW7425-1328 : support file handle type of URI in bcmPlayer
 * 
 * 4   8/25/11 7:31p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 * 3   8/22/11 5:38p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 * 1   8/22/11 4:05p zhangjq
 * SW7425-1172 : adjust architecture of bcmPlayer
 * 
 *****************************************************************************/
#define LOG_TAG "bcmPlayer_local"

#include "bcmPlayer.h"
#include "bcmPlayer_nexus.h"
#include "bcmPlayer_base.h"

#include "stream_probe.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

#define MAX_FILE_PATHNAME_LENGTH        256
#define MAX_FILE_HANDLE_LENGTH          100
#define MAX_FILE_OFFSET_LENGTH          50 

/*
Summary:
This function is used to wrap NEXUS_FilePlay object to apply an offset for index and data
*/

NEXUS_FilePlayHandle b_customfile_open(
    NEXUS_FilePlayHandle file, /* file object to be wrapped */
    off_t bof, /* sets new "beginning of file", in units of bytes */
    off_t eof  /* sets new "end of file", in units of bytes. if 0, then the file is unbounded. */
    );

extern bcmPlayer_base_nexus_handle_t nexus_handle[MAX_NEXUS_PLAYER];

static int bcmPlayer_connectResources_local(int iPlayerIndex) 
{
    int rc = 0;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings connectSettings;

    BKNI_Memset(&connectSettings, 0, sizeof(connectSettings));
    nexus_handle[iPlayerIndex].ipcclient->getClientInfo(nexus_handle[iPlayerIndex].nexus_client, &client_info);

    if (nexus_handle[iPlayerIndex].videoTrackIndex != -1) {
        /* TODO: Set NxClient_VideoDecoderCapabilities and NxClient_VideoWindowCapabilities of connectSettings */
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
    }
    if (nexus_handle[iPlayerIndex].audioTrackIndex != -1) {
        connectSettings.simpleAudioDecoder.id = client_info.audioDecoderId;
    }

    if (nexus_handle[iPlayerIndex].ipcclient->connectClientResources(nexus_handle[iPlayerIndex].nexus_client, &connectSettings) != true) {
        LOGE("%s: Could not connect client \"%s\" resources!", __FUNCTION__, nexus_handle[iPlayerIndex].ipcclient->getClientName());
        rc = 1;
    }
    return rc;
}

static int bcmPlayer_init_local(int iPlayerIndex) 
{
    int rc;

    rc = bcmPlayer_init_base(iPlayerIndex);

    if (rc == 0) {
        rc = bcmPlayer_connectResources_local(iPlayerIndex);
    }
    return rc;
}

static void bcmPlayer_uninit_local(int iPlayerIndex) {
    LOGD("bcmPlayer_uninit_local");

    bcmPlayer_uninit_base(iPlayerIndex);

    if(nexus_handle[iPlayerIndex].file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].file);
        nexus_handle[iPlayerIndex].file = NULL;
    }

    if(nexus_handle[iPlayerIndex].custom_file){
        NEXUS_FilePlay_Close(nexus_handle[iPlayerIndex].custom_file);
        nexus_handle[iPlayerIndex].custom_file = NULL;
    }

    if(nexus_handle[iPlayerIndex].stcChannel)
        NEXUS_StcChannel_Close(nexus_handle[iPlayerIndex].stcChannel);

    nexus_handle[iPlayerIndex].stcChannel = NULL;

    LOGD("bcmPlayer_uninit_local()  completed");
}

static int bcmPlayer_probeStreamFormat_local(int iPlayerIndex, const char *url, int64_t offset, stream_format_info *info)
{
    int rc = 0;
    stream_format_info bcm_stream_format_info;
    const char *ext = NULL; /* file extension */

    /* get steam information */
    memset(&bcm_stream_format_info, 0, sizeof(bcm_stream_format_info));

    /* Make the media probe start from the right place */
    bcm_stream_format_info.offset = offset;

    probe_stream_format(url, nexus_handle[iPlayerIndex].videoTrackIndex, nexus_handle[iPlayerIndex].audioTrackIndex, &bcm_stream_format_info);
    if(!bcm_stream_format_info.transportType)  
    {
        LOGE("Not support the stream type of file:%s !", url); 
        return -1;
    }

    if(0 == bcm_stream_format_info.videoCodec) {
        if (0 == bcm_stream_format_info.audioCodec){
            LOGE("Unknown Video and Audio codec type, the media type may not be supported"); 
            return -2;
        }
        else if (isAwesomePlayerAudioCodecSupported(bcm_stream_format_info.audioCodec)) {
            LOGD("Only audio codec required - reverting to AwesomePlayer.");
            return -2;
        }
    }

    if((0 == bcm_stream_format_info.videoPid) && (0 == bcm_stream_format_info.audioPid)){
        LOGE("Unknown Video and Audio PIDs, the media type may not be supported"); 
        return -3;
    }

    /* check to see if local file is m2ts. */
    ext = strrchr(url, '.');
    if (ext)
    {
      ext++;
    }
    if (bcm_stream_format_info.transportType == NEXUS_TransportType_eTs)
    {
      if (strcasecmp("m2ts", ext)==0)
      {
        LOGV("settings m2ts to true");
        bcm_stream_format_info.m2ts = true;
      }
    }

    *info = bcm_stream_format_info;
    return 0;
}

static int bcmPlayer_startVideoDecoder_local(int iPlayerIndex, stream_format_info *info)
{
    int rc = 0;
    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_SimpleVideoDecoderStartSettings svdStartSettings;
    NEXUS_VideoDecoderSettings videoDecoderSettings;

    if (nexus_handle[iPlayerIndex].simpleVideoDecoder && info->videoCodec) {
        LOGD("bcm_stream_format_info.videoPid = %d;  bcm_stream_format_info.extVideoPid = %d", info->videoPid, info->extVideoPid);
        
        NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
        videoProgram.codec = info->videoCodec;
        videoProgram.pidChannel = nexus_handle[iPlayerIndex].videoPidChannel;
        if (info->extVideoPid) {
            videoProgram.enhancementPidChannel = nexus_handle[iPlayerIndex].enhancementVideoPidChannel;
        }
        videoProgram.stcChannel = nexus_handle[iPlayerIndex].stcChannel;

        NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&svdStartSettings);
        svdStartSettings.settings = videoProgram;
        NEXUS_SimpleVideoDecoder_GetSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderSettings);
        
        if ((iPlayerIndex == 0) && 
            (videoDecoderSettings.supportedCodecs[NEXUS_VideoCodec_eH265] == NEXUS_VideoCodec_eH265) &&
            (info->videoCodec == NEXUS_VideoCodec_eH265) &&
            ((nexus_handle[iPlayerIndex].maxVideoFormat >= NEXUS_VideoFormat_e3840x2160p24hz) &&
                (nexus_handle[iPlayerIndex].maxVideoFormat < NEXUS_VideoFormat_e4096x2160p24hz)))
        {
            svdStartSettings.maxWidth  = 3840;
            svdStartSettings.maxHeight = 2160;
        }

        rc = NEXUS_SimpleVideoDecoder_Start(nexus_handle[iPlayerIndex].simpleVideoDecoder, &svdStartSettings);
        if (rc != 0) {
            LOGE("[%d]%s: Could not start SimpleVideoDecoder [rc=%d]!!!", iPlayerIndex, __FUNCTION__, rc);
        }
    }
    return rc;
}

static int bcmPlayer_setupVideoDecoder_local(int iPlayerIndex, stream_format_info *info)
{
    int rc = 0;

    if (nexus_handle[iPlayerIndex].simpleVideoDecoder && info->videoCodec) {
        if (info->extVideoPid) 
        {
            NEXUS_VideoDecoderSettings videoDecoderSettings;
            NEXUS_VideoDecoderExtendedSettings videoDecoderExtendedSettings;

            NEXUS_SimpleVideoDecoder_GetSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderSettings);
            videoDecoderSettings.customSourceOrientation = true;
            videoDecoderSettings.sourceOrientation = NEXUS_VideoDecoderSourceOrientation_e3D_LeftRightFullFrame;
            rc = NEXUS_SimpleVideoDecoder_SetSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderSettings);
            if (rc != 0) {
                LOGE("[%d]%s: Could not set simple video decoder settings!", iPlayerIndex, __FUNCTION__);
            }
            else if (iPlayerIndex == 0) {
                NEXUS_SimpleVideoDecoder_GetExtendedSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderExtendedSettings);
                videoDecoderExtendedSettings.svc3dEnabled = true;
                rc = NEXUS_SimpleVideoDecoder_SetExtendedSettings(nexus_handle[iPlayerIndex].simpleVideoDecoder, &videoDecoderExtendedSettings);
                if (rc != 0) {
                    LOGE("[%d]%s: Could not set simple decoder extended settings!", iPlayerIndex, __FUNCTION__);
                }
            }
        }

        if (rc == 0) {
            NEXUS_PlaybackPidChannelSettings pidSettings;
            NEXUS_Playback_GetDefaultPidChannelSettings(&pidSettings);
            pidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
            pidSettings.pidTypeSettings.video.codec = info->videoCodec; /* must be told codec for correct parsing */
            pidSettings.pidTypeSettings.video.index = true;
            pidSettings.pidTypeSettings.video.simpleDecoder = nexus_handle[iPlayerIndex].simpleVideoDecoder;
            nexus_handle[iPlayerIndex].videoPidChannel = NEXUS_Playback_OpenPidChannel(nexus_handle[iPlayerIndex].playback, info->videoPid, &pidSettings);
            if (nexus_handle[iPlayerIndex].videoPidChannel == NULL) {
                LOGE("[%d]%s: Could not open video pid playback channel!", iPlayerIndex, __FUNCTION__);
                return 1;
            }

            if (info->extVideoPid) {
                nexus_handle[iPlayerIndex].enhancementVideoPidChannel = NEXUS_Playback_OpenPidChannel(nexus_handle[iPlayerIndex].playback, info->extVideoPid, &pidSettings);
                if (nexus_handle[iPlayerIndex].enhancementVideoPidChannel == NULL) {
                    LOGE("[%d]%s: Could not open enhancement video pid playback channel!", iPlayerIndex, __FUNCTION__);
                    return 1;
                }
            }

            rc = bcmPlayer_startVideoDecoder_local(iPlayerIndex, info);
        }
    }
    return rc;
}

static int bcmPlayer_startAudioDecoder_local(int iPlayerIndex, stream_format_info *info)
{
    int rc = 0;

    if (nexus_handle[iPlayerIndex].simpleAudioDecoder && info->audioCodec) {
        LOGD("bcm_stream_format_info.audioPid = %d", info->audioPid);

        NEXUS_SimpleAudioDecoderStartSettings audioProgram;
        NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
        audioProgram.primary.codec = info->audioCodec;
        audioProgram.primary.pidChannel = nexus_handle[iPlayerIndex].audioPidChannel;
        audioProgram.primary.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
        rc = NEXUS_SimpleAudioDecoder_Start(nexus_handle[iPlayerIndex].simpleAudioDecoder, &audioProgram);
    }
    return rc;
}

static int bcmPlayer_setupAudioDecoder_local(int iPlayerIndex, stream_format_info *info)
{
    int rc = 0;
    NEXUS_PlaybackPidChannelSettings pidSettings;

    if (nexus_handle[iPlayerIndex].simpleAudioDecoder && info->audioCodec) {
        NEXUS_Playback_GetDefaultPidChannelSettings(&pidSettings);
        pidSettings.pidSettings.pidType = NEXUS_PidType_eAudio;
        pidSettings.pidTypeSettings.audio.simpleDecoder = nexus_handle[iPlayerIndex].simpleAudioDecoder;
        nexus_handle[iPlayerIndex].audioPidChannel = NEXUS_Playback_OpenPidChannel(nexus_handle[iPlayerIndex].playback, info->audioPid, &pidSettings);

        if (nexus_handle[iPlayerIndex].audioPidChannel == NULL) {
            LOGE("[%d]%s: Could not open audio pid playback channel!", iPlayerIndex, __FUNCTION__);
            rc = 1;
        }
        else {
            rc = bcmPlayer_startAudioDecoder_local(iPlayerIndex, info);
        }
    }
    return rc;
}

static int bcmPlayer_setDataSource_local(
        int iPlayerIndex, const char *url, uint16_t *videoWidth, uint16_t *videoHeight, char* extraHeader) {
    LOGD("bcmPlayer_setDataSource_local('%s')", url); 

    int rc;
    stream_format_info bcm_stream_format_info;
    NEXUS_StcChannelSettings stcSettings;
    NEXUS_PlaybackSettings playbackSettings;
    NEXUS_CallbackDesc endOfStreamCallbackDesc;
    NEXUS_PlaybackStartSettings playbackStartSettings;
    NEXUS_FilePlayHandle *playback_file = NULL;
    NEXUS_FilePlayOpenSettings filePlaySettings;
    int64_t file_chunk_eof = 0, file_chunk_bof = 0;

    if (extraHeader) {
        sscanf(extraHeader, "bof=%lld&eof=%lld", &file_chunk_bof, &file_chunk_eof);
    }

    rc = bcmPlayer_probeStreamFormat_local(iPlayerIndex, url, file_chunk_bof, &bcm_stream_format_info);

    if (rc != 0) {
        LOGE("[%d]%s: Could not probe stream \"%s\"!", iPlayerIndex, __FUNCTION__, url);
        return rc;
    }

    NEXUS_FilePlay_GetDefaultOpenSettings(&filePlaySettings);
#ifndef ANDROID_SUPPORTS_O_DIRECT
    filePlaySettings.data.directIo = false;
#endif
    filePlaySettings.data.filename = url;

    if(bcm_stream_format_info.transportType == NEXUS_TransportType_eWav) {
        filePlaySettings.index.filename = NULL;
        nexus_handle[iPlayerIndex].file = NEXUS_FilePlay_Open(&filePlaySettings);

        if (!nexus_handle[iPlayerIndex].file) {
            LOGE("can't open file:%s\n", url);
            return -4;
        }
    }
    else {
        filePlaySettings.index.filename = url;
        nexus_handle[iPlayerIndex].file = NEXUS_FilePlay_Open(&filePlaySettings);
        if (!nexus_handle[iPlayerIndex].file) {
            LOGE("can't open file:%s\n", url);
            return -5;
        }
    }

    playback_file = &nexus_handle[iPlayerIndex].file;

    if (file_chunk_bof) {
        /*open the custom wrapper around that file, to only play the bit we're told to*/
        LOGD("%s: Setting bounds for file I/O: %lld..%lld",
            __FUNCTION__, file_chunk_bof, file_chunk_eof);

        nexus_handle[iPlayerIndex].custom_file = b_customfile_open(nexus_handle[iPlayerIndex].file, file_chunk_bof, file_chunk_eof);

        playback_file = &nexus_handle[iPlayerIndex].custom_file;
    }

    *videoWidth  = bcm_stream_format_info.videoWidth;
    *videoHeight = bcm_stream_format_info.videoHeight;

    if (!(nexus_handle[iPlayerIndex].playback)) {
        LOGE("Playback handle is not created\n");
        return -6;
    }

    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    nexus_handle[iPlayerIndex].stcChannel = NEXUS_StcChannel_Open(NEXUS_ANY_ID, &stcSettings);
    if (nexus_handle[iPlayerIndex].stcChannel == NULL) {
        LOGE("can not open a STC channel");
        return -7;
    }

    /* Tell playpump which transport type should be selected */
    NEXUS_Playback_GetSettings(nexus_handle[iPlayerIndex].playback, &playbackSettings);
    playbackSettings.playpump = nexus_handle[iPlayerIndex].playpump;
    playbackSettings.playpumpSettings.transportType = bcm_stream_format_info.transportType;

    if (bcm_stream_format_info.m2ts == true)
    {
        playbackSettings.playpumpSettings.timestamp.type = NEXUS_TransportTimestampType_e30_2U_Mod300;
    }

    LOGD("bcm_stream_format_info.transportType = %d", bcm_stream_format_info.transportType);

    playbackSettings.startPaused = true; 
    playbackSettings.endOfStreamAction = NEXUS_PlaybackLoopMode_eLoop;
    playbackSettings.stcChannel = nexus_handle[iPlayerIndex].stcChannel;
    LOGD("set   playbackSettings.stcChannel = stcChannel ");

    /* end of stream callback */
    endOfStreamCallbackDesc.callback = bcmPlayer_endOfStreamCallback;
    endOfStreamCallbackDesc.context = NULL;
    endOfStreamCallbackDesc.param = iPlayerIndex;
    playbackSettings.endOfStreamCallback = endOfStreamCallbackDesc;

    NEXUS_Playback_SetSettings(nexus_handle[iPlayerIndex].playback, &playbackSettings);

    /* Playback must be told which stream ID is used for video and for audio. */
    rc = bcmPlayer_setupVideoDecoder_local(iPlayerIndex, &bcm_stream_format_info);
    if (rc != 0) {
        LOGE("[%d]%s: Error setting up video decoder!", iPlayerIndex, __FUNCTION__);
        return -1;
    }

    rc = bcmPlayer_setupAudioDecoder_local(iPlayerIndex, &bcm_stream_format_info);
    if (rc != 0) {
        LOGE("[%d]%s: Error setting up audio decoder!", iPlayerIndex, __FUNCTION__);
        return -1;
    }

    NEXUS_Playback_GetDefaultStartSettings(&playbackStartSettings);

    rc = NEXUS_Playback_Start(nexus_handle[iPlayerIndex].playback, *playback_file, &playbackStartSettings);

    if (rc) {
        LOGD("NEXUS_Playback_Start failed with Inxdex, trying without");
        playbackStartSettings.mode = NEXUS_PlaybackMode_eAutoBitrate;
        rc = NEXUS_Playback_Start(nexus_handle[iPlayerIndex].playback, *playback_file, &playbackStartSettings);
        if (rc) {
            LOGD("Retrying NEXUS_Playback_Start failed ");
            return -1;
        }
    }
    return 0;
}

static int bcmPlayer_setDataSource_file_handle_local(int iPlayerIndex, int fd, int64_t offset, int64_t length, uint16_t *videoWidth, uint16_t *videoHeight) {
    LOGD("bcmPlayer_setDataSource_file_handle_local(%d, %lld, %lld)", fd, offset, length);

    /* get file path name from file handle! */
    char file_handle[MAX_FILE_HANDLE_LENGTH] = {0};
    char path_name[MAX_FILE_PATHNAME_LENGTH] = {0};
    char offset_string[MAX_FILE_OFFSET_LENGTH];
    char *extra_header = NULL;
    int ret;

    snprintf(file_handle, MAX_FILE_HANDLE_LENGTH, "/proc/%d/fd/%d", getpid(), fd);

    ret = readlink(file_handle, path_name, MAX_FILE_PATHNAME_LENGTH);

    if(ret <= 0)
        return 1;

    LOGD("[%d]%s: file handle :%s, path name :%s, offset :%lld, length :%lld \n", iPlayerIndex, __FUNCTION__, file_handle, path_name, offset, length);

    if (offset)
    {
        snprintf(&offset_string[0], MAX_FILE_OFFSET_LENGTH, "bof=%lld&eof=%lld", offset, offset + length);
        extra_header = &offset_string[0];

        // Stash the extra header information
        if (extra_header != NULL) {
            nexus_handle[iPlayerIndex].extraHeaders = strdup(extra_header);
            if (nexus_handle[iPlayerIndex].extraHeaders == NULL) {
                LOGE("[%d]%s: Couldn't save extra header!", iPlayerIndex, __FUNCTION__);
                return 1;
            }
        }
    }

    // Stash the URL 
    nexus_handle[iPlayerIndex].url = strdup(path_name);
    if (nexus_handle[iPlayerIndex].url == NULL) {
        LOGE("[%d]%s: Couldn't save URL", iPlayerIndex, __FUNCTION__);
        return 1;
    }

    return bcmPlayer_setDataSource_local(iPlayerIndex, path_name, videoWidth, videoHeight, extra_header);
}

static int bcmPlayer_getMediaExtractorFlags_local(int iPlayerIndex, unsigned *flags)
{
    *flags = ( CAN_SEEK_BACKWARD |
               CAN_SEEK_FORWARD  |
               CAN_PAUSE         |
               CAN_SEEK );

    LOGD("[%d]%s: flags=0x%08x", iPlayerIndex, __FUNCTION__, *flags);

    return 0;
}

int  bcmPlayer_selectTrack_local(int iPlayerIndex, bcmPlayer_track_t trackType, unsigned trackIndex, bool select)
{
    int rc;
    stream_format_info bcm_stream_format_info;

    LOGD("[%d]%s(%d, %d, %d)", iPlayerIndex, __FUNCTION__, trackType, trackIndex, select);
    rc = bcmPlayer_selectTrack_base(iPlayerIndex, trackType, trackIndex, select);

    if (rc == 0) {
        if (trackType == TRACK_TYPE_AUDIO) {
            int64_t file_chunk_bof = 0;

            if (nexus_handle[iPlayerIndex].extraHeaders) {
                sscanf(nexus_handle[iPlayerIndex].extraHeaders, "bof=%lld", &file_chunk_bof);
            }

            rc = bcmPlayer_probeStreamFormat_local(iPlayerIndex, nexus_handle[iPlayerIndex].url, file_chunk_bof, &bcm_stream_format_info);
            if (rc != 0) {
                LOGE("[%d]%s: Could not probe stream \"%s\"!", iPlayerIndex, __FUNCTION__, nexus_handle[iPlayerIndex].url);
            }
            else {
                rc = bcmPlayer_connectResources_local(iPlayerIndex);

                if (rc == 0) {
                    if (select) {
                        LOGD("[%d]%s: Selecting audio track %d", iPlayerIndex, __FUNCTION__, trackIndex);
                        int64_t file_chunk_bof = 0;

                        rc = bcmPlayer_setupAudioDecoder_local(iPlayerIndex, &bcm_stream_format_info);
                        if (rc != 0) {
                            LOGE("[%d]%s: Error setting up audio decoder!", iPlayerIndex, __FUNCTION__);
                            return -1;
                        }
                    }
                    rc = bcmPlayer_startVideoDecoder_local(iPlayerIndex, &bcm_stream_format_info);
                    if (rc == 0) {
                        bcmPlayer_start_base(iPlayerIndex);
                    }
                }
            }
        }
        else if (trackType == TRACK_TYPE_VIDEO) {
            int64_t file_chunk_bof = 0;

            if (nexus_handle[iPlayerIndex].extraHeaders) {
                sscanf(nexus_handle[iPlayerIndex].extraHeaders, "bof=%lld", &file_chunk_bof);
            }

            rc = bcmPlayer_probeStreamFormat_local(iPlayerIndex, nexus_handle[iPlayerIndex].url, file_chunk_bof, &bcm_stream_format_info);
            if (rc != 0) {
                LOGE("[%d]%s: Could not probe stream \"%s\"!", iPlayerIndex, __FUNCTION__, nexus_handle[iPlayerIndex].url);
            }
            else {
                rc = bcmPlayer_connectResources_local(iPlayerIndex);

                if (rc == 0) {
                    if (select) {
                        LOGD("[%d]%s: Selecting video track %d", iPlayerIndex, __FUNCTION__, trackIndex);

                        rc = bcmPlayer_setupVideoDecoder_local(iPlayerIndex, &bcm_stream_format_info);
                        if (rc != 0) {
                            LOGE("[%d]%s: Error setting up video decoder!", iPlayerIndex, __FUNCTION__);
                            return -1;
                        }
                    }
                    rc = bcmPlayer_startAudioDecoder_local(iPlayerIndex, &bcm_stream_format_info);
                    if (rc == 0) {
                        bcmPlayer_start_base(iPlayerIndex);
                    }
                }
            }
        }
    }
    else {
        LOGE("[%d]%s: Could not select track!", iPlayerIndex, __FUNCTION__);
    }
    return rc;
}

void player_reg_local(bcmPlayer_func_t *pFuncs){
    /* assign function pointers implemented in this module */
    pFuncs->bcmPlayer_init_func = bcmPlayer_init_local;
    pFuncs->bcmPlayer_uninit_func = bcmPlayer_uninit_local;
    pFuncs->bcmPlayer_setDataSource_file_handle_func = bcmPlayer_setDataSource_file_handle_local;
    pFuncs->bcmPlayer_setDataSource_func = bcmPlayer_setDataSource_local;
    pFuncs->bcmPlayer_getMediaExtractorFlags_func = bcmPlayer_getMediaExtractorFlags_local;
    pFuncs->bcmPlayer_selectTrack_func = bcmPlayer_selectTrack_local;
}

/*
 * Custom NEXUS_FilePlay wrapper to support APK playback.
 *
 * APKs playback is just a case of using a specific portion of a file
 * observing the correct BOF/EOF as told by Android.
 * 
 * Heavily based on /nexus/examples/dvr/b_customfile.*
 */
#include "bfile_io.h"
#include "nexus_file_pvr.h"

static bfile_io_read_t b_customfile_read_offset_attach(bfile_io_read_t source, off_t bof, off_t eof);
static void b_customfile_read_offset_detach(bfile_io_read_t file);

/************************************
* b_customfile wrapper
**/

typedef struct b_customfile {
    struct NEXUS_FilePlay parent; /* must be first */
    NEXUS_FilePlayHandle original;
} b_customfile;

static void
NEXUS_P_FilePlayOffset_Close(struct NEXUS_FilePlay *file_)
{
    b_customfile *file = (b_customfile *)file_;
    b_customfile_read_offset_detach(file->parent.file.data);
    if(file->parent.file.index) {
        b_customfile_read_offset_detach(file->parent.file.index);
    }
    free(file);
    return;
}

NEXUS_FilePlayHandle
b_customfile_open( NEXUS_FilePlayHandle file, off_t bof, off_t eof)
{
    b_customfile *fileOffset;

    assert(file);
    assert(file->file.data);

    fileOffset = (b_customfile *) malloc(sizeof(*fileOffset));
    if(fileOffset == NULL) {
        LOGD("%s: Couldn't allocate", __FUNCTION__);
        goto err_alloc;
    }
    fileOffset->parent.file.index = NULL;

    fileOffset->parent.file.data = b_customfile_read_offset_attach(file->file.data, bof, eof);
    if(fileOffset->parent.file.data == NULL) {
        LOGD("%s: Couldn't b_customfile_read_offset_attach data", __FUNCTION__);
        goto err_data;
    }

    if(file->file.index) {
        fileOffset->parent.file.index = b_customfile_read_offset_attach(file->file.index, bof, eof);
        if(fileOffset->parent.file.index == NULL) {
            LOGD("%s: Couldn't b_customfile_read_offset_attach index", __FUNCTION__);
            goto err_index;
        }
    }
    fileOffset->parent.file.close = NEXUS_P_FilePlayOffset_Close;
    fileOffset->original = file;
    return &fileOffset->parent;

err_index:
    b_customfile_read_offset_detach(file->file.data);
err_data:
    free(fileOffset);
err_alloc:
    return NULL;
}

/************************************
* bfile abstraction
**/

struct bfile_read_offset {
    struct bfile_io_read ops; /* shall be the first member */
    off_t bof; /* sets new "beginning of file", in units of bytes */
    off_t eof; /* sets new "end of file", in units of bytes. if 0, then the file is unbounded. */
    bfile_io_read_t source;
};

static ssize_t
b_customfile_offset_read(bfile_io_read_t fd, void *buf, size_t length)
{
    struct bfile_read_offset *f = (bfile_read_offset *) fd;
    LOGV("%s: %#lx %u", __FUNCTION__, (unsigned long)fd, (unsigned)length);
    return f->source->read(f->source, buf, length);
}

/**
Convert actual file offset back to the virtual offset used by the caller.

NOTE: b_file_correct_offset in BSEAV/lib/bfile/bfile_utils.c uses a negative offset,
which is a little abstract. I'm using a positive offset here.
**/
static off_t
b_file_correct_offset(const struct bfile_read_offset *f, off_t offset)
{
    /* always apply bof bounds */
    if (offset >= f->bof) {
        /* apply eof bounds if non-zero */
        if (f->eof && offset > f->eof) {
            /* greater than eof is just eof-bof */
            offset = f->eof - f->bof;
        }
        else {
            /* in the middle of the file, just subtract off bof */
            offset -= f->bof;
        }
    }
    else {
        /* less than bof is just bof */
        offset = 0;
    }

    return offset;
}

static off_t
b_customfile_offset_seek(bfile_io_read_t fd, off_t offset, int whence)
{
    struct bfile_read_offset *f = (bfile_read_offset *)fd;
    off_t rc;

    LOGV("%s: %#lx %lu:%d",__FUNCTION__, (unsigned long)fd, (unsigned long)offset, whence);
    if (whence == SEEK_SET) {
        offset += f->bof;
        if (f->eof && offset > f->eof) {
            offset = f->eof;
        }
    }
    rc = f->source->seek(f->source, offset, whence);
    if (rc>=0) {
        /* if seeked in front of bof, seek back */
        if (rc<f->bof) {
            rc = f->source->seek(f->source, f->bof, SEEK_SET);
        }
        /* if seeked in back of bof, seek back */
        if (f->eof && rc > f->eof) {
            rc = f->source->seek(f->source, f->eof, SEEK_SET);
        }
    }
    if (rc>=0) { /* correct returned offset */
        rc = b_file_correct_offset(f, rc);
    }
    return rc;
}

static int
b_customfile_offset_bounds(bfile_io_read_t fd, off_t *first, off_t *last)
{
    off_t source_first=0, source_last=0;
    struct bfile_read_offset *f = (bfile_read_offset *)fd;
    int rc;

    rc = f->source->bounds(f->source, &source_first, &source_last);
    source_first = b_file_correct_offset(f, source_first);
    source_last = b_file_correct_offset(f, source_last);
    if(first) {
        *first = source_first;
    }
    if(last) {
        *last = source_last;
    }
    return rc;
}

static const struct bfile_io_read b_customfile_offset_read_ops = {
    b_customfile_offset_read,
    b_customfile_offset_seek,
    b_customfile_offset_bounds,
    BIO_DEFAULT_PRIORITY
};

static bfile_io_read_t
b_customfile_read_offset_attach(bfile_io_read_t source, off_t bof, off_t eof)
{
    struct bfile_read_offset *f;
    f = (bfile_read_offset *) malloc (sizeof(*f));
    if (!f) { return NULL; }
    f->ops = b_customfile_offset_read_ops;
    f->source = source;
    f->bof = bof;
    f->eof = eof;
    return &f->ops;
}

static void
b_customfile_read_offset_detach(bfile_io_read_t file)
{
    struct bfile_read_offset *f = (bfile_read_offset *)file;
    free(f);
    return;
}
