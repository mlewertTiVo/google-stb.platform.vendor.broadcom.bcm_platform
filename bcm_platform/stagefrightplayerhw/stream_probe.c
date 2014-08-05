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
 * $brcm_Workfile: stream_probe.c $
 * $brcm_Revision: 6 $
 * $brcm_Date: 9/27/12 11:25a $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/stream_probe.c $
 * 
 * 6   9/27/12 11:25a prasadv
 * SWANDROID-78: Implement FLAC support.
 * 
 * 5   9/19/12 12:46p mnelis
 * SWANDROID-78: Implement APK file playback.
 * 
 * 4   9/14/12 1:25p mnelis
 * SWANDROID-78: Recognise and play OGG files With Nexus.
 * 
 * 3   3/16/12 10:40a ttrammel
 * SWANDROID-46: Camera driver for ICS.
 * 
 * 2   1/17/12 3:18p franktcc
 * SW7425-2196: Adding H.264 SVC/MVC codec support for 3d video
 * 
 * 2   1/12/11 7:26p alexpan
 * SW7420-1240: Add html5 video support
 * 
 * 1   12/22/10 1:55p alexpan
 * SW7420-1240: add media_probe
 * 
 *****************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "bcmPlayer"
#include <cutils/log.h>
#include <cerrno>

#include "bstd.h"
#include "bmedia_probe.h"
#include "bmedia_types.h"
#include "bmedia_cdxa.h"
#include "bmpeg2ts_probe.h"
#include "bfile_stdio.h"
#include "stream_probe.h"

#define ANDROID_BVP_MODE

/************************** local define and functions ***************************/
static struct {
    NEXUS_VideoCodec nexus;
    bvideo_codec settop;
} g_videoCodec[] = {
    {NEXUS_VideoCodec_eUnknown, bvideo_codec_none},
    {NEXUS_VideoCodec_eUnknown, bvideo_codec_unknown},
    {NEXUS_VideoCodec_eMpeg1, bvideo_codec_mpeg1},
    {NEXUS_VideoCodec_eMpeg2, bvideo_codec_mpeg2},
    {NEXUS_VideoCodec_eMpeg4Part2, bvideo_codec_mpeg4_part2},
    {NEXUS_VideoCodec_eH263, bvideo_codec_h263},
    {NEXUS_VideoCodec_eH264, bvideo_codec_h264},
    {NEXUS_VideoCodec_eH264_Svc, bvideo_codec_h264_svc},
    {NEXUS_VideoCodec_eH264_Mvc, bvideo_codec_h264_mvc},
    {NEXUS_VideoCodec_eVc1, bvideo_codec_vc1},
    {NEXUS_VideoCodec_eVc1SimpleMain, bvideo_codec_vc1_sm},
    {NEXUS_VideoCodec_eDivx311, bvideo_codec_divx_311},
    {NEXUS_VideoCodec_eRv40, bvideo_codec_rv40},
    {NEXUS_VideoCodec_eVp6, bvideo_codec_vp6},
    {NEXUS_VideoCodec_eVp8, bvideo_codec_vp8},
    {NEXUS_VideoCodec_eSpark, bvideo_codec_spark},
    {NEXUS_VideoCodec_eAvs, bvideo_codec_avs},
    {NEXUS_VideoCodec_eH265, bvideo_codec_h265}
};

static struct {
    NEXUS_AudioCodec nexus;
    baudio_format settop;
} g_audioCodec[] = {
   {NEXUS_AudioCodec_eUnknown, baudio_format_unknown},
   {NEXUS_AudioCodec_eMpeg, baudio_format_mpeg},
   {NEXUS_AudioCodec_eMp3, baudio_format_mp3},
   {NEXUS_AudioCodec_eAac, baudio_format_aac},
   {NEXUS_AudioCodec_eAacPlus, baudio_format_aac_plus},
   {NEXUS_AudioCodec_eAacPlusAdts, baudio_format_aac_plus_adts},
   {NEXUS_AudioCodec_eAacPlusLoas, baudio_format_aac_plus_loas},
   {NEXUS_AudioCodec_eAc3, baudio_format_ac3},
   {NEXUS_AudioCodec_eAc3Plus, baudio_format_ac3_plus},
   {NEXUS_AudioCodec_eDts, baudio_format_dts},
   {NEXUS_AudioCodec_eLpcmHdDvd, baudio_format_lpcm_hddvd},
   {NEXUS_AudioCodec_eLpcmBluRay, baudio_format_lpcm_bluray},
   {NEXUS_AudioCodec_eDtsHd, baudio_format_dts_hd},
   {NEXUS_AudioCodec_eWmaStd, baudio_format_wma_std},
   {NEXUS_AudioCodec_eWmaPro, baudio_format_wma_pro},
   {NEXUS_AudioCodec_eLpcmDvd, baudio_format_lpcm_dvd},
   {NEXUS_AudioCodec_eAvs, baudio_format_avs},
   {NEXUS_AudioCodec_eAmr, baudio_format_amr},
   {NEXUS_AudioCodec_eDra, baudio_format_dra},
   {NEXUS_AudioCodec_eCook, baudio_format_cook},
   {NEXUS_AudioCodec_ePcmWav, baudio_format_pcm},
   {NEXUS_AudioCodec_eAdpcm, baudio_format_adpcm},
   {NEXUS_AudioCodec_eAdpcm, baudio_format_dvi_adpcm},
   {NEXUS_AudioCodec_eVorbis, baudio_format_vorbis},
   {NEXUS_AudioCodec_eFlac, baudio_format_flac}
};

static struct {
    NEXUS_TransportType nexus;
    unsigned settop;
} g_mpegType[] = {
    {NEXUS_TransportType_eTs, bstream_mpeg_type_unknown},
    {NEXUS_TransportType_eEs, bstream_mpeg_type_es},
    {NEXUS_TransportType_eTs, bstream_mpeg_type_bes},
    {NEXUS_TransportType_eMpeg2Pes, bstream_mpeg_type_pes},
    {NEXUS_TransportType_eTs, bstream_mpeg_type_ts},
    {NEXUS_TransportType_eDssEs, bstream_mpeg_type_dss_es},
    {NEXUS_TransportType_eDssPes, bstream_mpeg_type_dss_pes},
    {NEXUS_TransportType_eVob, bstream_mpeg_type_vob},
    {NEXUS_TransportType_eAsf, bstream_mpeg_type_asf},
    {NEXUS_TransportType_eAvi, bstream_mpeg_type_avi},
    {NEXUS_TransportType_eMpeg1Ps, bstream_mpeg_type_mpeg1},
    {NEXUS_TransportType_eMp4, bstream_mpeg_type_mp4},
    {NEXUS_TransportType_eMkv, bstream_mpeg_type_mkv},
    {NEXUS_TransportType_eWav, bstream_mpeg_type_wav},
    {NEXUS_TransportType_eMp4Fragment, bstream_mpeg_type_mp4_fragment},
    {NEXUS_TransportType_eRmff, bstream_mpeg_type_rmff},
    {NEXUS_TransportType_eFlv, bstream_mpeg_type_flv},
    {NEXUS_TransportType_eOgg, bstream_mpeg_type_ogg},
	  {NEXUS_TransportType_eFlac, bstream_mpeg_type_flac}
};

#define CONVERT(g_struct) \
    unsigned i; \
    for (i=0;i<sizeof(g_struct)/sizeof(g_struct[0]);i++) { \
        if (g_struct[i].settop == settop_value) { \
            return g_struct[i].nexus; \
        } \
    } \
    LOGE("unable to find Settop API value %d in %s\n", settop_value, #g_struct); \
    return g_struct[0].nexus

static NEXUS_VideoCodec b_videocodec2nexus(bvideo_codec settop_value)
{
    CONVERT(g_videoCodec);
}

static NEXUS_AudioCodec b_audiocodec2nexus(baudio_format settop_value)
{
    CONVERT(g_audioCodec);
}

static NEXUS_TransportType b_mpegtype2nexus(bstream_mpeg_type settop_value)
{
    CONVERT(g_mpegType);
}
/********************************************************************************************************************/

static const NEXUS_AudioCodec AwesomePlayerSupportedNexusAudioCodecs[] = {
    NEXUS_AudioCodec_eMp3,
    NEXUS_AudioCodec_ePcmWav,
    NEXUS_AudioCodec_eAmrNb,
    NEXUS_AudioCodec_eAmrWb,
    NEXUS_AudioCodec_eAac,
    NEXUS_AudioCodec_eVorbis,
};

bool isAwesomePlayerAudioCodecSupported(NEXUS_AudioCodec audioCodec)
{
    bool supported = false;
    unsigned i;

    for (i = 0; i < sizeof(AwesomePlayerSupportedNexusAudioCodecs)/sizeof(AwesomePlayerSupportedNexusAudioCodecs[0]); i++) {
        if (audioCodec == AwesomePlayerSupportedNexusAudioCodecs[i]) {
            supported = true;
            break;
        }
    }
    return supported;
}

void probe_stream_format(const char *url, int videoTrackIndex, int audioTrackIndex, stream_format_info *p_stream_format_info)    
{
    /* use media probe to set values */
    bmedia_probe_t            probe = NULL;
    bmedia_probe_config       probe_config;
    const bmedia_probe_stream *stream = NULL;
    const bmedia_probe_track  *track = NULL;
    int                       trackIndex;
    bfile_io_read_t           fd = NULL;
    bool                      foundAudio = false;
    bool                      foundVideo = false;
    bool                      foundAudioTrack = false;
    bool                      foundVideoTrack = false;
    bool                      foundExtVideo = false;
    FILE                      *fin;
    char                      stream_info[512];

    fin = fopen(url,"rb");
    if (!fin) {
        LOGE("can't open media file '%s' for probing!!! [%s]\n", url, strerror(errno));
        return;
    }

    probe = bmedia_probe_create();

    fd = bfile_stdio_read_attach(fin);

    bmedia_probe_default_cfg(&probe_config);
    probe_config.file_name = url;
    probe_config.type = bstream_mpeg_type_unknown;
    probe_config.probe_offset = p_stream_format_info->offset;
    stream = bmedia_probe_parse(probe, fd, &probe_config);

    if (stream) {
        if (stream->type == bstream_mpeg_type_cdxa) {
            bcdxa_file_t cdxa_file;
            bmedia_stream_to_string(stream, stream_info, sizeof(stream_info));
            LOGV( "Media Probe:\n" "%s\n\n", stream_info);
            cdxa_file = bcdxa_file_create(fd);
            if (cdxa_file) {
                const bmedia_probe_stream *cdxa_stream;
                cdxa_stream = bmedia_probe_parse(probe, bcdxa_file_get_file_interface(cdxa_file), &probe_config);
                bcdxa_file_destroy(cdxa_file);
                if (cdxa_stream) {
                    bmedia_probe_stream_free(probe, stream);
                    stream = cdxa_stream;
                }
            }
        }
        else if (stream->type == bstream_mpeg_type_ts) {
            p_stream_format_info->tsPktLen = ((bmpeg2ts_probe_stream*)stream)->pkt_len;
        }
    }

    p_stream_format_info->indexPresent = (stream->index == bmedia_probe_index_available || stream->index == bmedia_probe_index_required);

    /* now stream is either NULL, or stream descriptor with linked list of audio/video tracks */
    bfile_stdio_read_detach(fd);

    fclose(fin);
    if(!stream) {
        LOGE("media probe can't parse stream '%s'\n", url);
        return;
    }
        
    bmedia_stream_to_string(stream, stream_info, sizeof(stream_info));
    LOGV(
        "Media Probe:\n"
        "%s\n\n",
        stream_info);
        
    for (trackIndex=0, track=BLST_SQ_FIRST(&stream->tracks); track; track=BLST_SQ_NEXT(track, link), trackIndex++) 
    {
        switch(track->type) 
        {
            case bmedia_track_type_audio:
                if(audioTrackIndex != -1 && (track->info.audio.codec != baudio_format_unknown)) {
                    /* Find the first audio track... */
                    if (!foundAudio) {
                        p_stream_format_info->audioPid = track->number;
                        p_stream_format_info->audioCodec = b_audiocodec2nexus(track->info.audio.codec);
                        foundAudio = true;
                    }

                    /* Find the desired audio track... */
                    if (!foundAudioTrack && audioTrackIndex == trackIndex) {
                        p_stream_format_info->audioPid = track->number;
                        p_stream_format_info->audioCodec = b_audiocodec2nexus(track->info.audio.codec);
                        foundAudioTrack = true;
                    }
                }
                break;

            case bmedia_track_type_video:
                if (videoTrackIndex != -1 && (track->info.video.codec != bvideo_codec_unknown)) {
                    /* Find the first video track... */
                    if (!foundVideo) {
                        p_stream_format_info->videoPid = track->number;
                        p_stream_format_info->videoCodec = b_videocodec2nexus(track->info.video.codec);
                        p_stream_format_info->videoWidth = track->info.video.width;
                        p_stream_format_info->videoHeight = track->info.video.height;
                        foundVideo = true;
                    }
                    else if ((track->info.video.codec == bvideo_codec_h264_svc || track->info.video.codec == bvideo_codec_h264_mvc) && !foundExtVideo) {
                        p_stream_format_info->extVideoPid = track->number;
                        p_stream_format_info->videoCodec = b_videocodec2nexus(track->info.video.codec);
                        foundExtVideo = true;
                    }

                    /* Find the desired video track... */
                    if (!foundVideoTrack && videoTrackIndex == trackIndex) {
                        p_stream_format_info->videoPid = track->number;
                        p_stream_format_info->videoCodec = b_videocodec2nexus(track->info.video.codec);
                        p_stream_format_info->videoWidth = track->info.video.width;
                        p_stream_format_info->videoHeight = track->info.video.height;
                        foundVideoTrack = true;
                    }
                } 
                break;

            default:
                break;
        }
    }

    p_stream_format_info->transportType = b_mpegtype2nexus(stream->type);
#ifdef ANDROID_BVP_MODE
    if (p_stream_format_info->audioCodec == 0 && p_stream_format_info->transportType == NEXUS_TransportType_eMpeg2Pes) {
        p_stream_format_info->audioCodec = NEXUS_AudioCodec_ePcmWav;
        LOGV("___probe__audioCodec fake mode line:%d\n", __LINE__);
    }

    if (p_stream_format_info->audioPid == 0 && p_stream_format_info->transportType == NEXUS_TransportType_eMpeg2Pes) {
        p_stream_format_info->audioPid = 0xc0;
        LOGV("___probe__audioPid fake mode line:%d\n", __LINE__);
    }
#endif
    LOGD("transportType = %d;  videoCodec = %d;  videoPid = %d;  extVideoPid = %d; audioCodec = %d;  audioPid = %d;  video width = %d;  video height = %d\n",
        p_stream_format_info->transportType, p_stream_format_info->videoCodec, p_stream_format_info->videoPid, p_stream_format_info->extVideoPid,
        p_stream_format_info->audioCodec, p_stream_format_info->audioPid,
        p_stream_format_info->videoWidth, p_stream_format_info->videoHeight
        );
            
    if (probe) {
        if (stream) {
            bmedia_probe_stream_free(probe, stream);
        }
        bmedia_probe_destroy(probe);
    }
}
