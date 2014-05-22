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
 * $brcm_Workfile: bcmPlayer_record.cpp $
 * $brcm_Revision: 6 $
 * $brcm_Date: 8/15/12 7:08p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/stagefrightplayerhw/bcmPlayer_record.cpp $
 * 
 * 6   8/15/12 7:08p alexpan
 * SWANDROID-124: removed trackInput from
 *  NEXUS_VideoEncoderStreamStructure in latest Nexus
 * 
 * 5   7/9/12 3:52p alexpan
 * SWANDROID-124: Add camera record support for 7435
 * 
 * 4   6/25/12 6:01p fzhang
 * SWANDROID-124: Support camera recording support in ICS
 * 
 * 3   6/14/12 7:50p alexpan
 * SWANDROID-115: removed trackInput from
 *  NEXUS_VideoEncoderStreamStructure in latest Nexus
 * 
 * 2   5/29/12 1:36p kagrawal
 * SWANDROID-96: Including assert.h to fix build failure for SBS
 * 
 * 1   4/18/12 3:39p ttrammel
 * SWANDROID-66: Change bcmPlayer_record.c to cpp file.
 * 
 * 2   3/28/12 5:54p alexpan
 * SWANDROID-46: Fix camera record build errors on platforms without video
 *  encoder
 * 
 * 1   3/19/12 8:07a ttrammel
 * SWANDROID-46: Add camera driver to ICS.
 * 
 *****************************************************************************/

#include "bcmPlayer.h"
#include "bcmPlayer_base.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "bcmPlayer_record"

#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_input.h"
#include "nexus_audio_output.h"
#include "nexus_playback.h"
#include "nexus_parser_band.h"
#include "nexus_file.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include "nexus_timebase.h"
#include "bdbg.h"
#include <pthread.h>
#include "blst_squeue.h"
#include <linux/soundcard.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "nexus_types.h"
#include "nexus_video_output.h"
#include "nexus_video_image_input.h"
#include "nexus_display.h"

#ifdef NEXUS_HAS_VIDEO_ENCODER
#include "nexus_video_encoder_output.h"
#include "nexus_video_encoder.h"
#endif

#include "bcmPlayer_nexus.h"
#include "nexus_video_adj.h"

#define BVP_ERROR                      (-1)
#define BVP_SUCCESS                    (0)
#define BVP_AUDIO_DATA_LENGTH_MAX      (480)
#define BVP_GETBUFFER_MAXDESCRIPTORS   (512)
#define BVP_VIDEO_PES_HDRLEN           (14)
#define BVP_VIDEO_PES_HDRLEN_NOPTS     (9)
#define BVP_VIDEO_PES_HDRLEN_FIX       (6)
#define BVP_AUDIO_PES_HDRLEN           (40)
#define BVP_VIDEO_FRAME_FLAG           (0x10000)
#define BVP_VIDEO_ONE_FRAME_MAX        (250*1024)
#define BVP_VIDEO_PES_STREAM_ID        (0xE0)
#define BVP_AUDIO_PES_STREAM_ID        (0xC0)
#define BVP_VIDEO_CLOCK_RATE           (90000)

#define B_GET_BITS(w,e,b)  (((w)>>(b))&(((unsigned)(-1))>>((sizeof(unsigned))*8-(e+1-b))))
#define B_SET_BIT(name,v,b)  (((unsigned)((v)!=0)<<(b)))
#define B_SET_BITS(name,v,e,b)  (((unsigned)(v))<<(b))

#define B_MEDIA_SAVE_UINT16_LE(b, d) do { (b)[0] = (uint8_t)(d); (b)[1]=(uint8_t)((d)>>8);}while(0)
#define B_MEDIA_SAVE_UINT32_LE(b, d) do { (b)[0] = (uint8_t)(d); (b)[1]=(uint8_t)((d)>>8);(b)[2]=(uint8_t)((d)>>16); (b)[3] = (uint8_t)((d)>>24);}while(0)
#define BVP_PES_WAV_PCM_HEADER_LEN (26)
#define BVP_AUDIOCAPTURE_DEF_DEV       "/dev/snd/dsp"
#define UINT32_C(value) ((uint_least32_t)(value ## UL)) 

typedef unsigned long long             uint64;

typedef unsigned int                   uint32;

typedef unsigned short                 uint16;
typedef signed short                   int16;

typedef unsigned char                  uint8;
typedef signed char                    int8;

typedef struct wav_pcm_header {
uint16_t             wFormatTag;
uint16_t             nChannels;
uint32_t             nSamplesPerSec;
uint32_t             nAvgBytesPerSec;
uint16_t             nBlockAlign;
uint16_t             wBitsPerSample;
uint16_t             cbSize;
} wav_pcm_header;
typedef struct ABVP_Node {
    BLST_SQ_ENTRY(ABVP_Node)       list;
    void                              *data;
    int                                len;
} ABVP_Node;
BLST_SQ_HEAD(ABVP_NodeHead, ABVP_Node);
typedef struct ABVP_NodeHead ABVP_NodeHead; 




NEXUS_VideoImageInputHandle imageInput = NULL;
NEXUS_DisplayHandle displayTranscode;
#ifdef NEXUS_HAS_VIDEO_ENCODER
NEXUS_VideoEncoderHandle videoEncoder;
#endif
unsigned char *encoderbufferbase = NULL;
int mfd;
BKNI_EventHandle imageEvent;
NEXUS_SurfaceMemory                surfaceMemory;
void *tmpbuf = NULL;

static  int                            audioDeviceHandle;
typedef struct ABVP_Settings
{
    int                                reserved;
    int                                numAudioPesNode;             /* Number of cached audio pes node */
    int                                numVideoPesNode;             /* Number of cached video pes node */
    int                                nodesPerRead;
} ABVP_Settings;

struct PLY_ES
{
        pthread_t                      plyPESThreadId;
        pthread_t                      plyPCMThreadId;
        pthread_t                      plyWritePESThreadId;
        pthread_t                      plyWritePCMThreadId;
        pthread_mutex_t                mutex;
        BKNI_EventHandle                   audioEventHandle;
    BKNI_EventHandle                   videoEventHandle;
        ABVP_NodeHead                      audioNodeHead;
        ABVP_NodeHead                      videoNodeHead;
        pthread_mutex_t                    audiomutex;
        pthread_mutex_t                    videomutex;
        ABVP_Node                         *audioNodes;
    ABVP_Node                         *videoNodes;
        uint8                             *audioRecvBufs;
    uint8                             *videoRecvBufs;
        ABVP_NodeHead                      audioNodePool;
        ABVP_NodeHead                      videoNodePool;
        BKNI_EventHandle                   audioEvent;
    BKNI_EventHandle                   videoEvent;
    ABVP_Settings                      settings;
};

typedef struct PLY_ES *PLYES_Handle;
static PLYES_Handle                          handlePLYES = NULL;
static int statPES = 0;
static int statPESVideo = 0;
static int statPESAudio = 0;
static bool videoRawDataReady = false;
static NEXUS_VideoWindowHandle windowTranscode = NULL;
static NEXUS_VideoInput videoInput = NULL;
static NEXUS_SurfaceHandle nxYuvSurface[3] = {NULL};
static NEXUS_StcChannelHandle stcChannelEncoder = NULL;
static struct timeval                         timeBase;

static ABVP_Settings                 g_ABVP_DefaultSettings = 
{
    0,  /* reserved */
    20, /* numAudioPesNode */
    10, /* numVideoPesNode */
    16  /* nodesPerRead */
};

static void imageBufferCallback( void *context, int param )
{
    BSTD_UNUSED(param);
    BKNI_SetEvent((BKNI_EventHandle)context);
}

void bcmRecorder_parser_audio_pes_init(uint8  *buf,uint32 timestamp)
{
    uint8                            *waveheader = buf;
    wav_pcm_header                     wf;
    int                                index = 0;
    BKNI_Memset(&wf,0,sizeof(wf));
    wf.wFormatTag = 1;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 8000;
    wf.nAvgBytesPerSec = 8000;
    wf.nBlockAlign = 1;
    wf.wBitsPerSample = 16;
    waveheader[index++] = 0x00;
    waveheader[index++] = 0x00;    
    waveheader[index++] = 0x01;
    waveheader[index++] = 0xc0;
    waveheader[index++] = 0x02;      /* Size bits 8..15 */
    waveheader[index++] = 0x02;    /* Size bits 0..7 */
    waveheader[index++] = 0x81;                 /* Header, no scrambling, priority = 0, align=0, copyright =0, original */
    waveheader[index++] = 0x80;                 /* All flags = 0 */
    waveheader[index++] = 0x05;                 /* PES header data length = 0 */  
    waveheader[index++] = 0x21;
    waveheader[index++] = B_GET_BITS(timestamp,28,21); /* PTS [29..15] -> PTS [29..22] */
    waveheader[index++] = 
    B_SET_BITS("PTS [29..15] -> PTS [21..15]", B_GET_BITS(timestamp,20,14), 7, 1) |
    B_SET_BIT("marker_bit", 1, 0);
    waveheader[index++] = B_GET_BITS(timestamp,13,6); /* PTS [14..0] -> PTS [14..7]  */
    waveheader[index++] = 
    B_SET_BITS("PTS [14..0] -> PTS [6..0]", B_GET_BITS(timestamp,5,0), 7, 2) |
    B_SET_BIT("PTS[0]", 0, 1) |
    B_SET_BIT("marker_bit", 1, 0);
    waveheader[index++] = 0x42;
    waveheader[index++] = 0x43;
    waveheader[index++] = 0x4d;
    waveheader[index++] = 0x41;
    waveheader[index++] = 0x00;
    waveheader[index++] = 0x00;
    waveheader[index++] = 0x01;
    waveheader[index++] = 0xe0;
    B_MEDIA_SAVE_UINT16_LE(waveheader+index+0,  wf.wFormatTag);
    B_MEDIA_SAVE_UINT16_LE(waveheader+index+2,  wf.nChannels);
    B_MEDIA_SAVE_UINT32_LE(waveheader+index+4,  wf.nSamplesPerSec);
    B_MEDIA_SAVE_UINT32_LE(waveheader+index+8,  wf.nAvgBytesPerSec);
    B_MEDIA_SAVE_UINT16_LE(waveheader+index+12, wf.nBlockAlign);
    B_MEDIA_SAVE_UINT16_LE(waveheader+index+14, wf.wBitsPerSample);
    B_MEDIA_SAVE_UINT16_LE(waveheader+index+16, wf.cbSize);
    return;
}

int bcmRecorder_init_audio()
{ 

    int    channels = 1;
    int    speed = 8000;
    int    fragSize = 0;
    int    newFrag = 0x7fff0006;
    int    format = AFMT_S16_LE;
    int    ret = BVP_SUCCESS;

    audioDeviceHandle = open(BVP_AUDIOCAPTURE_DEF_DEV, O_RDWR);

    if (audioDeviceHandle < 0) 
    {
         LOGE("(%s,%d)",__FUNCTION__, __LINE__);
         ret = BVP_ERROR;
         return ret;
    }

    fcntl(audioDeviceHandle, F_SETFL,/* O_NONBLOCK*/~O_NONBLOCK);

    if (ioctl(audioDeviceHandle,SNDCTL_DSP_SETFMT, &format) == -1) 
    {
        LOGE("(%s,%d)",__FUNCTION__, __LINE__);
        ret = BVP_ERROR;
        return ret;    
    }

    if (ioctl(audioDeviceHandle, SNDCTL_DSP_CHANNELS, &channels) == -1)
    {
        LOGE("(%s,%d)",__FUNCTION__, __LINE__);
        ret = BVP_ERROR;
        return ret;
    }


    if (ioctl(audioDeviceHandle, SNDCTL_DSP_SPEED, &speed) == -1) 
    {
        LOGE("(%s,%d)",__FUNCTION__, __LINE__);     
        ret =  BVP_ERROR;
        return ret;
    } 



    if (ioctl(audioDeviceHandle, SNDCTL_DSP_GETBLKSIZE, &fragSize) == -1) 
    {
        LOGE("(%s,%d)",__FUNCTION__, __LINE__);     
        ret =  BVP_ERROR;
        return ret;
    }

    if (ioctl (audioDeviceHandle, SNDCTL_DSP_SETFRAGMENT, &newFrag) == -1)
    {
        LOGE("(%s,%d)",__FUNCTION__, __LINE__);     
        ret = BVP_ERROR;
        return ret;
    }
    
    

    return ret;

}
static size_t
bcmRecorder_pesHeader_init(uint8_t *pes_header, uint8_t stream_id, uint32_t pts, bool pts_valid, size_t length)
{
    unsigned off;
    const bool frame_boundary=false;
    const bool extension=false;

    pes_header[0] = 0;
    pes_header[1] = 0;
    pes_header[2] = 1;
    pes_header[3] = stream_id;

    pes_header[6] = 
        B_SET_BITS( "10", 2, 7, 6 ) |
        B_SET_BITS("PES_scrambling_control", 0, 5, 4) |
        B_SET_BIT("PES_priority", 0, 3) |
        B_SET_BIT("data_alignment_indicator", frame_boundary, 2) |
        B_SET_BIT("copyright", 0, 1) |
        B_SET_BIT("original_or_copy", 1, 0);
    pes_header[7] = 
        B_SET_BITS("PTS_DTS_flags", pts_valid?2:0, 7, 6) |
        B_SET_BIT("ESCR_flag", 0, 5) |
        B_SET_BIT("ES_rate_flag", 0, 4) |
        B_SET_BIT("DSM_trick_mode_flag", 0, 3) |
        B_SET_BIT("additional_copy_info_flag", 0, 2) |
        B_SET_BIT("PES_CRC_flag", 0, 1) |
        B_SET_BIT("PES_extension_flag", extension, 0);
    if (pts_valid) {
        pes_header[9] = 
            B_SET_BITS("0010", 0x2, 7, 4) |
            B_SET_BITS("PTS [32..30]", B_GET_BITS(pts,31,29), 3, 1) |
            B_SET_BIT("marker_bit", 1, 0);
        pes_header[10] = B_GET_BITS(pts,28,21); /* PTS [29..15] -> PTS [29..22] */
        pes_header[11] = 
            B_SET_BITS("PTS [29..15] -> PTS [21..15]", B_GET_BITS(pts,20,14), 7, 1) |
            B_SET_BIT("marker_bit", 1, 0);
        pes_header[12] = B_GET_BITS(pts,13,6); /* PTS [14..0] -> PTS [14..7]  */
        pes_header[13] = 
                        B_SET_BITS("PTS [14..0] -> PTS [6..0]", B_GET_BITS(pts,5,0), 7, 2) |
                        B_SET_BIT("PTS[0]", 0, 1) |
                        B_SET_BIT("marker_bit", 1, 0);
        off = 14;
    } else {
        off = 9;
    }

    pes_header[8] = off - 9;
    /* length */
    if (length==0) {
        pes_header[4] = 0;
        pes_header[5] = 0;
    } else {
        length += (off-6);
        if (length>65536) {
            BDBG_WRN(("unbounded pes %02x %u", stream_id, length));
            length = 0;
        }
        pes_header[4] = B_GET_BITS(length, 15, 8);
        pes_header[5] = B_GET_BITS(length, 7, 0);
    }

    return off;
}

void readPESPlaypump_Open(PLYES_Handle *handle)
{
    PLYES_Handle                         hdl = NULL;
    /* Create BVP handle */
    hdl = (PLYES_Handle)BKNI_Malloc(sizeof(struct PLY_ES));
    if(hdl == NULL)
    {
        LOGD("(%s,%d): Malloc plyS handle failed!",__FUNCTION__, __LINE__);     
        goto fail;
    }
    memset(handle, 0, sizeof(*handle));
    *handle = hdl;
    return;
fail:
    if(hdl)
        BKNI_Free(hdl);

    return;

}
static int acquire_readmutexlock(PLYES_Handle handle)
{
    return pthread_mutex_trylock(&handle->mutex);   
}
static void release_readmutexlock(PLYES_Handle handle)
{
    pthread_mutex_unlock(&handle->mutex);
}
static void modifyAudioPts(uint8 *pPesHeader,uint32 timeStamp)
{
    
}

void *readPCMPlaypumpThread(void *arg)
{

    PLYES_Handle                         handle = (PLYES_Handle)arg;
    int                                  num = 0;
    static bool                          enterpro = true;
    ABVP_Node                            *pkt;
    int     ret = BVP_SUCCESS;
    uint8                               *pesOneFramebuf = NULL;
    static uint32                        timeStamp = 0;
    struct timeval                       tv;

    pesOneFramebuf = (uint8 *)BKNI_Malloc(BVP_AUDIO_DATA_LENGTH_MAX);
    if (NULL == pesOneFramebuf)
    {
        BDBG_ERR(("(%s,%d):  malloc assem frame failed!",
                        __FUNCTION__, __LINE__));
        return NULL;        /* Remove compile warning */
    } 

    while (1)
    {
        if (statPES == 1)
        {
            statPESAudio = 1;
            /* read pcm */
            num = read(audioDeviceHandle, pesOneFramebuf, BVP_AUDIO_DATA_LENGTH_MAX);
            if(num < 0)
            {
                if(errno == EAGAIN)
                {
                    ret= BVP_SUCCESS;
                }
                else
                {
                    ret= BVP_ERROR;  
                }    
            }
            else
            {
                gettimeofday(&tv, 0);
                timeStamp =  (tv.tv_sec -timeBase.tv_sec)*1000*45 + (tv.tv_usec - timeBase.tv_usec)/1000*45;
                if (num != BVP_AUDIO_DATA_LENGTH_MAX)
                {
                    LOGD("(%s,%d): read a abnormal audio data num:[%d]!",__FUNCTION__, __LINE__,num); 
                    continue;
                }
                pthread_mutex_lock(&(handlePLYES->audiomutex));
                pkt = BLST_SQ_FIRST(&handlePLYES->audioNodePool);
                if(pkt && num>BVP_AUDIO_PES_HDRLEN)
                {
                    BLST_SQ_REMOVE_HEAD(&handlePLYES->audioNodePool, list);
                    pthread_mutex_unlock(&(handlePLYES->audiomutex));
                    bcmRecorder_parser_audio_pes_init((uint8*)pkt->data,timeStamp);
                    BKNI_Memcpy((uint8*)pkt->data+BVP_AUDIO_PES_HDRLEN, 
                                pesOneFramebuf, 
                                num);
                    pkt->len = num+BVP_AUDIO_PES_HDRLEN;
                    

                    pthread_mutex_lock(&(handlePLYES->audiomutex));
                    BLST_SQ_INSERT_TAIL(&handlePLYES->audioNodeHead, pkt, list);
                    pthread_mutex_unlock(&(handlePLYES->audiomutex));

                    BKNI_SetEvent(handlePLYES->audioEvent);
                }
                else
                {
                    pthread_mutex_unlock(&(handlePLYES->audiomutex));
                }
                
            }

        }
        else
        {
            statPESAudio = 0;
            BKNI_Sleep(200);
        }
    }   
    BKNI_Free(pesOneFramebuf);
    return NULL;

}
void readPCMPlaypumpStart(PLYES_Handle                        handle)
{
    int                                ret = 0;
    /*int                                policy = 0;*/
    
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 10;
    pthread_attr_setschedparam(&attr, &param);
    /*sched_get_priority_min(policy);*/

    ret = pthread_create(&(handle->plyPCMThreadId), NULL, /*(void *)*/readPCMPlaypumpThread, (void *)handle);
    if (ret)
    {
        LOGD("+++++++++++++++plyES error");
        
    }
    
    
    pthread_attr_destroy(&attr);

}

void writeFileWithTryLock
     (void                             *arg,
      uint8                            *pMem,
      uint32                            offset)
{
    PLYES_Handle                         handle = (PLYES_Handle)arg;
    while (0 != acquire_readmutexlock(handle))
    {
        BKNI_Sleep(10);
    }
    ssize_t n = write(mfd, (const uint8_t *)pMem, offset);
    release_readmutexlock(handle);
}

static void *readPESPlaypumpThread
    (void                             *arg)
{
    size_t bytes=0;
    size_t size[2];

#ifdef NEXUS_HAS_VIDEO_ENCODER
    const NEXUS_VideoEncoderDescriptor *desc[2];
#endif

    unsigned i,j;
    unsigned descs;
    int     ret = BVP_SUCCESS;
    uint8                                *pOffset[BVP_GETBUFFER_MAXDESCRIPTORS];
    unsigned int                          pLength[BVP_GETBUFFER_MAXDESCRIPTORS];
    uint32                                pFlag[BVP_GETBUFFER_MAXDESCRIPTORS];
    uint32                                count = 0;
    static uint32                         offset = 0;
    uint8                                *pesaddr = NULL;
    uint8                                *pesOneFramebuf = NULL;
    unsigned int                          peslen = 0;
    static bool                           enterpro = true;
    ABVP_Node                            *pkt;
    static uint32                        timeStamp = 0;
    struct timeval                       tv;
    
    pesOneFramebuf = (uint8 *)BKNI_Malloc(BVP_VIDEO_ONE_FRAME_MAX);
    if (NULL == pesOneFramebuf)
    {
        BDBG_ERR(("(%s,%d):  malloc assem frame failed!",
                        __FUNCTION__, __LINE__));
        return NULL;        /* Remove compile warning */
    }

#ifdef NEXUS_HAS_VIDEO_ENCODER
    while (1)
    { 
        if ( statPES == 1 )
        {
            statPESVideo = 1;
            bytes=0;
            count = 0;
            BKNI_WaitForEvent(handlePLYES->videoEventHandle, BKNI_INFINITE); 
            if (statPES == 0)
            {
                statPESVideo = 0;
                continue;
            }

            while (bytes == 0){

                NEXUS_VideoEncoder_GetBuffer(videoEncoder, &desc[0], &size[0], &desc[1], &size[1]);
                if(size[0]==0 && size[1]==0) {
                    LOGE("NEXUS_VideoEncoder_GetBuffer, nodata, wait");
                    BKNI_Sleep(30);
                    if (statPES == 0)
                    {
                        statPESVideo = 0;
                        break;
                    }
                    continue;
                }
                gettimeofday(&tv, 0);
                timeStamp =  (tv.tv_sec -timeBase.tv_sec)*1000*45 + (tv.tv_usec - timeBase.tv_usec)/1000*45;
                for(descs=0,j=0;j<2;j++) {
                    descs+=size[j];
                    for(i=0;i<size[j];i++) {
                        if(desc[j][i].length > 0)
                        {
                            /*ssize_t n = write(mfd, (const uint8_t *)encoderbufferbase + desc[j][i].offset, desc[j][i].length);*/
                            bytes+= desc[j][i].length;
                            pOffset[count] =(uint8 *)encoderbufferbase + desc[j][i].offset;
                            pLength[count] = desc[j][i].length;
                            /*fwrite(pOffset[count],pLength[count],1,fes720576);*/
                            pFlag[count] = desc[j][i].flags&BVP_VIDEO_FRAME_FLAG;
                            if(desc[j][i].length > 0x100000)
                            {
                                BDBG_ERR(("++++ desc[%d][%d] length = 0x%x, offset=0x%x", 
                                               j,i, desc[j][i].length, desc[j][i].offset));
                            }
                            if (pFlag[count] && offset!=0)
                            {
                                pthread_mutex_lock(&(handlePLYES->videomutex));
                                pkt = BLST_SQ_FIRST(&handlePLYES->videoNodePool);
                                if(pkt)
                                {
                                    BLST_SQ_REMOVE_HEAD(&handlePLYES->videoNodePool, list);
                                    pthread_mutex_unlock(&(handlePLYES->videomutex));
                                    
                                    bcmRecorder_pesHeader_init((uint8*)pkt->data,
                                                       BVP_VIDEO_PES_STREAM_ID,
                                                       timeStamp,
                                                       true,
                                                       offset);

                                    BKNI_Memcpy((uint8*)pkt->data+BVP_VIDEO_PES_HDRLEN, 
                                                pesOneFramebuf, 
                                                offset);
                                    pkt->len = offset+BVP_VIDEO_PES_HDRLEN;

                                    pthread_mutex_lock(&(handlePLYES->videomutex));
                                    BLST_SQ_INSERT_TAIL(&handlePLYES->videoNodeHead, pkt, list);
                                    pthread_mutex_unlock(&(handlePLYES->videomutex));

                                    BKNI_SetEvent(handlePLYES->videoEvent);
                                }
                                else
                                {
                                    pthread_mutex_unlock(&(handlePLYES->videomutex));
                                }
                                offset=0;
                            }

                            BKNI_Memcpy((uint8 *)pesOneFramebuf+offset,(uint8 *)(*(pOffset+count)), (uint32)(pLength[count]) );
                            offset += pLength[count];
                            
                            count++;
            
                        }
                    }
                }
                NEXUS_VideoEncoder_ReadComplete(videoEncoder, descs);
            }
             
        }
        else
        {
            statPESVideo = 0;
            BKNI_Sleep(20);
        }
    }
#endif

    BKNI_Free(pesOneFramebuf);
    return NULL;

}

void readPESPlaypumpStart(PLYES_Handle handle)
{
    int                                ret = 0;
    /*int                                policy = 0;*/
    
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 60;
    pthread_attr_setschedparam(&attr, &param);
    /*sched_get_priority_min(policy);*/


    ret = pthread_create(&(handle->plyPESThreadId), NULL, /*(void *)*/readPESPlaypumpThread, (void *)handle); 

    if (ret)
    {
        LOGD("+++++++++++++++plyES error");
        
    }
    
    pthread_attr_destroy(&attr);

}

static void *writeAudioPESThread
    (void                             *arg)
{
   
    ABVP_Node                           *pkt;


    while (1)
    {
        /* deal with audio pkt */
        pthread_mutex_lock(&(handlePLYES->audiomutex));
        while((pkt = BLST_SQ_FIRST(&handlePLYES->audioNodeHead)) == NULL)
        {
            pthread_mutex_unlock(&(handlePLYES->audiomutex));
            BKNI_WaitForEvent(handlePLYES->audioEvent, BKNI_INFINITE);            
            pthread_mutex_lock(&(handlePLYES->audiomutex));
        }
        pthread_mutex_unlock(&(handlePLYES->audiomutex));
        /* write data  to  file  need get lock */
        writeFileWithTryLock(handlePLYES,(uint8*)pkt->data,pkt->len);
        pthread_mutex_lock(&(handlePLYES->audiomutex));
        BLST_SQ_REMOVE_HEAD(&handlePLYES->audioNodeHead, list);
        pkt->len = 0;
        BLST_SQ_INSERT_TAIL(&handlePLYES->audioNodePool, pkt, list);
        pthread_mutex_unlock(&(handlePLYES->audiomutex));

    }

    return NULL;

}

void writeAudioPESStart(PLYES_Handle handle)
{
    int                                ret = 0;
    /*int                                policy = 0;*/
    
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 60;
    pthread_attr_setschedparam(&attr, &param);
    /*sched_get_priority_min(policy);*/


    ret = pthread_create(&(handle->plyWritePCMThreadId), NULL, /*(void *)*/writeAudioPESThread, (void *)handle);

    if (ret)
    {
        LOGD("+++++++++++++++plyES error");
        
    }
    
    pthread_attr_destroy(&attr);

}
static void *writeVideoPESThread
    (void                             *arg)
{
   
    ABVP_Node                           *pkt;


    while (1)
    {
        /* deal with video pkt */
        pthread_mutex_lock(&(handlePLYES->videomutex));
        while((pkt = BLST_SQ_FIRST(&handlePLYES->videoNodeHead)) == NULL)
        {
            pthread_mutex_unlock(&(handlePLYES->videomutex));
            BKNI_WaitForEvent(handlePLYES->videoEvent, BKNI_INFINITE);            
            pthread_mutex_lock(&(handlePLYES->videomutex));
        }
        pthread_mutex_unlock(&(handlePLYES->videomutex));

        /* write data  to  file  need get lock */
        writeFileWithTryLock(handlePLYES,(uint8*)pkt->data,pkt->len);
        pthread_mutex_lock(&(handlePLYES->videomutex));
        BLST_SQ_REMOVE_HEAD(&handlePLYES->videoNodeHead, list);
        pkt->len = 0;
        BLST_SQ_INSERT_TAIL(&handlePLYES->videoNodePool, pkt, list);
        pthread_mutex_unlock(&(handlePLYES->videomutex));
    }

    return NULL;

}


void writeVideoPESStart(PLYES_Handle handle)
{
    int                                ret = 0;
    /*int                                policy = 0;*/
    
    pthread_attr_t attr;
    struct sched_param param;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 60;
    pthread_attr_setschedparam(&attr, &param);
    /*sched_get_priority_min(policy);*/


    ret = pthread_create(&(handle->plyWritePESThreadId), NULL, /*(void *)*/writeVideoPESThread, (void *)handle);

    if (ret)
    {
        LOGD("+++++++++++++++plyES error");
        
    }
    
    pthread_attr_destroy(&attr);

}
extern "C" {

int bcmRecorder_init(int fd, int32_t videoWidth, int32_t videoHeight)
{
    NEXUS_DisplaySettings displaySettings;
#ifdef NEXUS_HAS_VIDEO_ENCODER
    NEXUS_VideoEncoderSettings videoEncoderConfig;
    NEXUS_VideoEncoderDelayRange videoDelay;
    NEXUS_VideoEncoderStartSettings videoEncoderStartConfig;
    NEXUS_VideoEncoderStatus videoEncoderStatus;
#endif
    NEXUS_DisplayCustomFormatSettings customFormatSettings;
    NEXUS_SurfaceCreateSettings nxCreateSettings;


    NEXUS_VideoImageInputSettings  imageInputSetting;

    unsigned char *previewBuffer=NULL;

    int  ret = BVP_SUCCESS;
    LOGD("====bcmRecorder_init === videoWidth=%d videoHeight=%d",videoWidth, videoHeight);


    if (fd < 0)
        return -1;

    mfd = fd;

    bcmRecorder_init_audio();

#ifdef NEXUS_HAS_VIDEO_ENCODER
    /* create imageinput */
#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    imageInput = NEXUS_VideoImageInput_Open(2, NULL);
#elif (NEXUS_PLATFORM == 97231)
    imageInput = NEXUS_VideoImageInput_Open(0, NULL);
#endif  
    videoInput = NEXUS_VideoImageInput_GetConnector(imageInput);
    assert(imageInput);
    if (videoInput == NULL)
    {
        LOGE("Open image error");
        goto ERR;
    }

    BKNI_CreateEvent(&imageEvent);
    NEXUS_VideoImageInput_GetSettings( imageInput, &imageInputSetting );
    imageInputSetting.imageCallback.callback = imageBufferCallback;
    imageInputSetting.imageCallback.context  = imageEvent;
    NEXUS_VideoImageInput_SetSettings( imageInput, &imageInputSetting );
    

    // create videoencoder, displaytranscode, transcodewindow
    videoEncoder = NEXUS_VideoEncoder_Open(0, NULL);
    assert(videoEncoder);

#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    if (videoEncoder == NULL)
    {
        videoEncoder = NEXUS_VideoEncoder_Open(1, NULL);
        if (videoEncoder == NULL)
        {
            LOGE("Open encode error");
            goto ERR;
        }
    }
#endif
    NEXUS_Display_GetDefaultSettings(&displaySettings);
    displaySettings.displayType = NEXUS_DisplayType_eAuto;
#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    displaySettings.timingGenerator = NEXUS_DisplayTimingGenerator_eEncoder;
    displaySettings.format = NEXUS_VideoFormat_e480p;
    displayTranscode = NEXUS_Display_Open(NEXUS_ENCODER_DISPLAY_IDX, &displaySettings);
#elif (NEXUS_PLATFORM == 97231)
    displaySettings.timingGenerator = NEXUS_DisplayTimingGenerator_ePrimaryInput;
    displaySettings.format = NEXUS_VideoFormat_e480p;   /* Init setting, will be set per config file later */
    displayTranscode = NEXUS_Display_Open(1/*NEXUS_ENCODER_DISPLAY_IDX*/, &displaySettings);
#endif
    assert(displayTranscode);
#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    NEXUS_Display_GetDefaultCustomFormatSettings(&customFormatSettings);
    customFormatSettings.width = videoWidth;
    customFormatSettings.height = videoHeight;
    customFormatSettings.refreshRate = 60000;
    customFormatSettings.interlaced = false;
    customFormatSettings.aspectRatio = NEXUS_DisplayAspectRatio_e4x3;
    customFormatSettings.dropFrameAllowed = true;
    NEXUS_Display_SetCustomFormatSettings(displayTranscode, NEXUS_VideoFormat_eCustom2, &customFormatSettings);
#endif
    windowTranscode = NEXUS_VideoWindow_Open(displayTranscode, 0);
    NEXUS_VideoWindow_AddInput(windowTranscode, videoInput);

#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
    videoEncoderConfig.variableFrameRate = true;
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e14_985;
    videoEncoderConfig.bitrateMax = 1*1000*1000;
    videoEncoderConfig.streamStructure.framesP = 29;
    videoEncoderConfig.streamStructure.framesB = 0;
    NEXUS_VideoEncoder_SetSettings(videoEncoder, &videoEncoderConfig);
#endif
#endif /* #ifdef NEXUS_HAS_VIDEO_ENCODER */
    return 0;
ERR:
    return -1;

}

static NEXUS_HeapHandle b_get_image_input_heap(unsigned imageInputIndex)
{
    NEXUS_PlatformSettings platformSettings;
    unsigned videoDecoderIndex, heapIndex, avdCoreIndex;
    NEXUS_PlatformConfiguration platformConfig;
    
    /* The imageInput index is the same as the MPEG feeder (MFD) index. we need to find a heap that is RTS-accessible by this MFD. 
    This can be learned from the VideoDecoderModule because the MFD normally reads from AVD picture buffers. */
    
    /* first, get platform settings, which gives us VideoDecoderModuleSettings */
    NEXUS_Platform_GetSettings(&platformSettings);
    
    /* next, find the VideoDecoder index that uses this MFD */
    for (videoDecoderIndex=0;videoDecoderIndex<NEXUS_NUM_VIDEO_DECODERS;videoDecoderIndex++) {
        if (platformSettings.videoDecoderModuleSettings.mfdMapping[videoDecoderIndex] == imageInputIndex) {
            /* we've found a video decoder that uses this MFD */
            break;
        }
    }
    if (videoDecoderIndex == NEXUS_NUM_VIDEO_DECODERS) {
        /* this MFD is unused by VideoDecoder, so we can't know the heap */
        return NULL;
    }
    
    /* next, find the avd core index that maps to this videoDecoder */
    for (avdCoreIndex=0;avdCoreIndex<NEXUS_MAX_XVD_DEVICES;avdCoreIndex++) {
        if (platformSettings.videoDecoderModuleSettings.avdMapping[avdCoreIndex] == videoDecoderIndex) {
            /* we've found avd Core that maps to this video decoder */
            break;
        }
    }
    if (avdCoreIndex == NEXUS_MAX_XVD_DEVICES) {
        /* this video decoder is unmapped by avd core, so we can't know the heap */
        return NULL;
    }

    /* now, get the heap index for this video decoder core. the MFD must be able to read from this heap 
    if it's able to read AVD decoded pictures placed into this heap. */
    heapIndex = platformSettings.videoDecoderModuleSettings.avdHeapIndex[avdCoreIndex];
    
    /* then return the heap */
    NEXUS_Platform_GetConfiguration(&platformConfig);
    return platformConfig.heap[heapIndex];
}

int bcmRecorder_start(void * recordbuf, int32_t videoWidth, int32_t videoHeight) {


    NEXUS_Error rc;

    NEXUS_SurfaceCreateSettings nxCreateSettings;
#ifdef NEXUS_HAS_VIDEO_ENCODER
    NEXUS_VideoEncoderStartSettings    videoEncoderStartConfig;
    NEXUS_VideoEncoderStatus           videoEncoderStatus;  

    NEXUS_StcChannelSettings stcSettings;   
    NEXUS_VideoEncoderSettings videoEncoderConfig;
    NEXUS_VideoEncoderDelayRange videoDelay;
#endif
    ABVP_Node                      *pkt;
    int                             i;
    
    LOGD("====bcmRecoder_start === videoWidth=%d videoHeight=%d",videoWidth, videoHeight);

#ifdef NEXUS_HAS_VIDEO_ENCODER
#if (NEXUS_PLATFORM == 97231)
    NEXUS_VideoImageInputSurfaceSettings surfaceSettings;
    NEXUS_VideoWindowScalerSettings    sclSettings;
    NEXUS_VideoWindowSettings          windowSettings;
#endif
    if (NULL == recordbuf)
        return -1;
    
    NEXUS_Surface_GetDefaultCreateSettings(&nxCreateSettings);
    nxCreateSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;
    nxCreateSettings.width = videoWidth;
    nxCreateSettings.height = videoHeight;
//  nxCreateSettings.pMemory = (unsigned char *)recordbuf;

#if (NEXUS_PLATFORM == 97425)     
    nxCreateSettings.heap = b_get_image_input_heap(2);
#elif (NEXUS_PLATFORM == 97435)       
    NEXUS_ClientConfiguration   clientConfig;
    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    nxCreateSettings.heap = clientConfig.heap[1];
#elif (NEXUS_PLATFORM == 97231)       
    nxCreateSettings.heap = NEXUS_Platform_GetFramebufferHeap(0); /* must created from device memory */
#endif  

    nxYuvSurface[0] = NEXUS_Surface_Create(&nxCreateSettings);
    nxYuvSurface[1] = NEXUS_Surface_Create(&nxCreateSettings);
    nxYuvSurface[2] = NEXUS_Surface_Create(&nxCreateSettings);
    if (nxYuvSurface[0] == NULL || nxYuvSurface[1] == NULL || nxYuvSurface[2] == NULL)
    {
        LOGE("Create nxYuvSurface failed");
        return -1;
    }

    /*
    NEXUS_Surface_GetMemory(nxYuvSurface, &surfaceMemory);
    assert(surfaceMemory);
    NEXUS_VideoImageInput_SetNextSurface(imageInput, nxYuvSurface);
    */
    

#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    NEXUS_StcChannel_GetDefaultSettings(2, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
//  stcSettings.pcrBits = NEXUS_StcChannel_PcrBits_eFull42;/* ViCE2 requires 42-bit STC broadcast */
    stcChannelEncoder = NEXUS_StcChannel_Open(2, &stcSettings);
#elif (NEXUS_PLATFORM == 97231)
    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
//  stcSettings.pcrBits = NEXUS_StcChannel_PcrBits_eFull42;/* ViCE2 requires 42-bit STC broadcast */
    stcChannelEncoder = NEXUS_StcChannel_Open(0, &stcSettings);
#endif
    if (stcChannelEncoder == NULL)
        LOGD("bcmRecoder_start ===stcChannelEncoder is NULL");
#if (NEXUS_PLATFORM == 97231)
    NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
    videoEncoderConfig.variableFrameRate = true;
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e14_985;
    videoEncoderConfig.bitrateMax = 2*1000*1000;
    videoEncoderConfig.streamStructure.framesP = 29;
    videoEncoderConfig.streamStructure.framesB = 0;
    NEXUS_VideoEncoder_SetSettings(videoEncoder, &videoEncoderConfig);
        
    /* Set encoder video window*/
    NEXUS_VideoWindow_GetSettings(windowTranscode, &windowSettings);    
    windowSettings.position.width = videoWidth;
    windowSettings.position.height = videoHeight;     
    windowSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;    
    windowSettings.visible = false;
    NEXUS_VideoWindow_SetSettings(windowTranscode, &windowSettings);

    NEXUS_VideoWindow_GetScalerSettings(windowTranscode, &sclSettings);
    sclSettings.bandwidthEquationParams.bias = NEXUS_ScalerCaptureBias_eScalerBeforeCapture;
    sclSettings.bandwidthEquationParams.delta = 1000000;
    NEXUS_VideoWindow_SetScalerSettings(windowTranscode, &sclSettings);
#endif
    NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
    NEXUS_VideoEncoder_GetDefaultStartSettings(&videoEncoderStartConfig);
    videoEncoderStartConfig.codec = NEXUS_VideoCodec_eH264;
    videoEncoderStartConfig.profile = NEXUS_VideoCodecProfile_eBaseline;
    videoEncoderStartConfig.level = NEXUS_VideoCodecLevel_e31;
    videoEncoderStartConfig.input = displayTranscode;
    videoEncoderStartConfig.interlaced = false;
    videoEncoderStartConfig.stcChannel = stcChannelEncoder;
    // NOTE: video encoder delay is in 27MHz ticks
#if (NEXUS_PLATFORM == 97231)
    videoEncoderConfig.enableFieldPairing = false;
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e14_985;
    videoEncoderStartConfig.rateBufferDelay = 0;
    videoEncoderStartConfig.bounds.inputFrameRate.min = NEXUS_VideoFrameRate_e23_976;
    videoEncoderStartConfig.bounds.outputFrameRate.min = NEXUS_VideoFrameRate_e23_976;
    videoEncoderStartConfig.bounds.outputFrameRate.max = NEXUS_VideoFrameRate_e60;
    videoEncoderStartConfig.bounds.inputDimension.max.width = videoWidth;
    videoEncoderStartConfig.bounds.inputDimension.max.height = videoHeight;
#endif
#if (NEXUS_PLATFORM == 97425) || (NEXUS_PLATFORM == 97435)
    NEXUS_VideoEncoder_GetDelayRange(videoEncoder, &videoEncoderConfig, &videoEncoderStartConfig, &videoDelay);
    BDBG_WRN(("\n\tVideo encoder end-to-end delay = [%u ~ %u] ms", videoDelay.min/27000, videoDelay.max/27000));
#endif
    videoEncoderConfig.encoderDelay = videoDelay.min;
#if (NEXUS_PLATFORM == 97231) 
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e14_985;
    videoEncoderConfig.bitrateMax = 2*1000*1000;
#endif
    /* note the Dee is set by SetSettings */
    NEXUS_VideoEncoder_SetSettings(videoEncoder, &videoEncoderConfig);
    NEXUS_VideoEncoder_Start(videoEncoder, &videoEncoderStartConfig);
    
    NEXUS_VideoEncoder_GetStatus(videoEncoder, &videoEncoderStatus);
    encoderbufferbase = (unsigned char *)videoEncoderStatus.bufferBase;
    if (encoderbufferbase == NULL)
    {
        LOGE("encoderbufferbase is NULL");
        return -1;
    }
    if (NULL == handlePLYES)
    {

        readPESPlaypump_Open(&handlePLYES);
        pthread_mutex_init(&handlePLYES->mutex, NULL);

        handlePLYES->settings = g_ABVP_DefaultSettings;
        
        handlePLYES->videoRecvBufs = (uint8 *)BKNI_Malloc(handlePLYES->settings.numVideoPesNode*BVP_VIDEO_ONE_FRAME_MAX);
        if (NULL == handlePLYES->videoRecvBufs)
        {
            BDBG_ERR(("(%s,%d): VideoEncode send video malloc assem frame failed!",
                            __FUNCTION__, __LINE__));
            return BVP_ERROR;       /* Remove compile warning */
        }
        handlePLYES->videoNodes = (ABVP_Node *)BKNI_Malloc(handlePLYES->settings.numVideoPesNode*sizeof(ABVP_Node));
        if (NULL == handlePLYES->videoNodes)
        {
            BDBG_ERR(("(%s,%d): VideoEncode send video malloc assem frame failed!",
                            __FUNCTION__, __LINE__));
            return BVP_ERROR;       /* Remove compile warning */
        }
        
        pthread_mutex_init(&(handlePLYES->videomutex), NULL);
        BLST_SQ_INIT(&handlePLYES->videoNodePool);
        for(i = 0; i < handlePLYES->settings.numVideoPesNode; i++)
        {
            pkt = handlePLYES->videoNodes + i;
            pkt->data = (void *)(handlePLYES->videoRecvBufs + (BVP_VIDEO_ONE_FRAME_MAX * i));
            pkt->len = 0;
            BLST_SQ_INSERT_TAIL(&handlePLYES->videoNodePool, pkt, list);
        }
        BKNI_CreateEvent(&(handlePLYES->videoEvent));


        handlePLYES->audioRecvBufs = (uint8 *)BKNI_Malloc(handlePLYES->settings.numAudioPesNode*(BVP_AUDIO_PES_HDRLEN+BVP_AUDIO_DATA_LENGTH_MAX));
        if (NULL == handlePLYES->audioRecvBufs)
        {
            LOGD("(%s,%d)",__FUNCTION__, __LINE__);
            return BVP_ERROR;
        }
        handlePLYES->audioNodes = (ABVP_Node *) BKNI_Malloc(handlePLYES->settings.numAudioPesNode*sizeof(ABVP_Node));
        if (NULL == handlePLYES->audioNodes)
        {
            LOGD("(%s,%d)",__FUNCTION__, __LINE__);
            return BVP_ERROR;
        }

        pthread_mutex_init(&(handlePLYES->audiomutex), NULL);
        BLST_SQ_INIT(&handlePLYES->audioNodePool);
        for(i = 0; i < handlePLYES->settings.numAudioPesNode; i++)
        {
            pkt = handlePLYES->audioNodes + i;
            pkt->data = (void *)(handlePLYES->audioRecvBufs + ((BVP_AUDIO_PES_HDRLEN+BVP_AUDIO_DATA_LENGTH_MAX) * i));
            pkt->len = 0;
            BLST_SQ_INSERT_TAIL(&handlePLYES->audioNodePool, pkt, list);
        }
        BKNI_CreateEvent(&(handlePLYES->audioEvent));

        BLST_SQ_INIT(&handlePLYES->audioNodeHead);
        BLST_SQ_INIT(&handlePLYES->videoNodeHead);
        statPES = 0;
        readPCMPlaypumpStart(handlePLYES);
        readPESPlaypumpStart(handlePLYES);
        writeAudioPESStart(handlePLYES);
        writeVideoPESStart(handlePLYES);
        LOGD("====bcmRecoder_start end ===");
    }
    /* init event */
    BKNI_CreateEvent(&(handlePLYES->videoEventHandle));
    
    tmpbuf = recordbuf;
    assert(tmpbuf); 
    gettimeofday(&timeBase, 0);
    statPES = 1;
#endif

    return 0;
}

int bcmRecorder_stop() {

#ifdef NEXUS_HAS_VIDEO_ENCODER
    LOGE("======= bcmRecord_stop");
    statPES = 0;
    while(statPESVideo == 1 || statPESAudio == 1)
    {
        LOGE("======= bcmRecord_stop[%d][%d]",statPESVideo,statPESAudio);
        BKNI_SetEvent(handlePLYES->videoEventHandle);
        BKNI_Sleep(200);
    }

    NEXUS_VideoEncoder_Stop(videoEncoder, NULL);
    BKNI_Sleep(200);
    NEXUS_StcChannel_Close(stcChannelEncoder);
    stcChannelEncoder = NULL;
    BKNI_Sleep(200);
    NEXUS_VideoEncoder_Close(videoEncoder);
    videoEncoder = NULL;

    NEXUS_VideoWindow_RemoveAllInputs(windowTranscode);
    NEXUS_VideoWindow_Close(windowTranscode);
    NEXUS_Display_Close(displayTranscode);
    NEXUS_VideoImageInput_SetSurface(imageInput, NULL);
    NEXUS_VideoInput_Shutdown(videoInput);
    NEXUS_VideoImageInput_Close(imageInput);
    BKNI_DestroyEvent(imageEvent);
    NEXUS_Surface_Destroy(nxYuvSurface[0]);
    NEXUS_Surface_Destroy(nxYuvSurface[1]);
    NEXUS_Surface_Destroy(nxYuvSurface[2]);
    BKNI_DestroyEvent(handlePLYES->videoEventHandle);
    #if DEBUG_BCMRECODER
    fclose(fout);
    fclose(fyuv);
    #endif
    if( close(audioDeviceHandle) < 0 )
    {
        LOGE("========= close audio device failed");
        return BVP_ERROR;
    }
    else
    {
        audioDeviceHandle = -1;
        return BVP_SUCCESS;
    }
#endif
    return BVP_SUCCESS;
    
}


int bcmRecorder_writebuf(int32_t videoWidth, int32_t videoHeight) 
{
#ifdef NEXUS_HAS_VIDEO_ENCODER
    int     ret = BVP_SUCCESS;
    NEXUS_SurfaceHandle                   pic = NULL;
    static unsigned int                i = 0;
    NEXUS_VideoImageInputSurfaceSettings surfaceSettings;
    uint32                             stamp = 0;
    struct timeval                       tv2;
    
    if ( statPES == 1 )
    {
        

        pic = nxYuvSurface[i%3];
        do
        {
            /* Because we re-use buffers, must check it is not in use, before filling */
            ret = NEXUS_VideoImageInput_CheckSurfaceCompletion(imageInput, pic);
            if(ret == NEXUS_IMAGEINPUT_BUSY) 
            {
                BKNI_WaitForEvent(imageEvent, BKNI_INFINITE);
            }
        }while( ret );

        i = (i%3) + 1;
        NEXUS_Surface_GetMemory(pic, &surfaceMemory);

        
        BKNI_Memcpy(surfaceMemory.buffer, tmpbuf, videoWidth*videoHeight*2);
        NEXUS_Surface_Flush(pic);
        gettimeofday(&tv2, 0);
        /*stamp = (tv2.tv_sec - timeBase.tv_sec)*BVP_VIDEO_CLOCK_RATE + (tv2.tv_usec - timeBase.tv_usec)/1000*(BVP_VIDEO_CLOCK_RATE/1000);*/
        stamp = tv2.tv_sec*1000000 + tv2.tv_usec;
        NEXUS_VideoImageInput_GetDefaultSurfaceSettings( &surfaceSettings );
        surfaceSettings.displayVsyncs = 1; /* x * Vysnc(~16mSec) = number of seconds to display */
        surfaceSettings.pts = stamp;
        do {
            ret = NEXUS_VideoImageInput_PushSurface(imageInput, pic , &surfaceSettings );
            if(ret==NEXUS_IMAGEINPUT_QUEUE_FULL) {
                BKNI_WaitForEvent(imageEvent, BKNI_INFINITE);
            }
        } while ( ret );

        BKNI_SetEvent(handlePLYES->videoEventHandle);
        BKNI_Sleep(20);
    }
#endif
    return 0;
}

}  /* extern "C" {  */

