/******************************************************************************
 *    (c)2011 Broadcom Corporation
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
 * $brcm_Workfile: V4L2Camera.cpp $
 * $brcm_Revision: 2 $
 * $brcm_Date: 4/13/11 1:57p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/froyo/vendor/broadcom/bcm_platform/libcamera/V4L2Camera.cpp $
 * 
 * 2   4/13/11 1:57p ttrammel
 * SW7420-1813: Check-in GoogleTV camera changes to Android.
 * 
 *****************************************************************************/
/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      Author: Niels Keeman <nielskeeman@gmail.com>
 *
 */

#define LOG_TAG "V4L2Camera"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>


#include "converter.h"
#include "V4L2Camera.h"
#include "nexus_platform_client.h"

#include "nexus_graphics2d.h"
#include "nexus_surface.h"

extern "C" { /* Android jpeglib.h missed extern "C" */
#include <jpeglib.h>
}

namespace android {
#define FRAMERATE 15
V4L2Camera::V4L2Camera ()
    : nQueued(0), nDequeued(0)
{
    videoIn = (struct vdIn *) calloc (1, sizeof (struct vdIn));
}

V4L2Camera::~V4L2Camera()
{
    free(videoIn);
}

int V4L2Camera::Open (const char *device, int width, int height, int pixelformat)
{
    int ret;
	struct v4l2_streamparm			   parm;

    if ((fd = open(device, O_RDWR)) == -1) {
        ALOGE("ERROR opening V4L interface: %s", strerror(errno));
        return -1;
    }

    ret = ioctl (fd, VIDIOC_QUERYCAP, &videoIn->cap);
    if (ret < 0) {
        ALOGE("Error opening device: unable to query device.");
        return -1;
    }

    if ((videoIn->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        ALOGE("Error opening device: video capture not supported.");
        return -1;
    }

    if (!(videoIn->cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("Capture device does not support streaming i/o");
        return -1;
    }

    videoIn->width = width;
    videoIn->height = height;
    videoIn->framesizeIn = (width * height << 1);
    videoIn->formatIn = pixelformat;

    videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->format.fmt.pix.width = width;
    videoIn->format.fmt.pix.height = height;
    videoIn->format.fmt.pix.pixelformat = pixelformat;
#if (NEXUS_PLATFORM != 97425) && (NEXUS_PLATFORM != 97231)
    ret = ioctl(fd, VIDIOC_S_FMT, &videoIn->format);
    if (ret < 0) {
        ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }
#else
	memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = FRAMERATE;

    if(ioctl(fd, VIDIOC_S_PARM, &parm) == -1) 
    {
        ALOGE("(%s,%d): Set video capture frame rate %d failed!",
            __FUNCTION__, __LINE__, parm.parm.capture.timeperframe.denominator);

       if(fd > 0)
	   {
	        close(fd);
	   }
	   return -1;
    }

    
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_G_PARM, &parm) == -1)
    {
        ALOGE("(%s,%d): Get video capture frame rate failed!",
            __FUNCTION__, __LINE__);
    }

    if(parm.parm.capture.timeperframe.denominator != FRAMERATE)
    {
        ALOGE("(%s,%d): Frame rate %d not supported, change to %d!",
            __FUNCTION__, __LINE__, FRAMERATE, parm.parm.capture.timeperframe.denominator);
    }
    else
    {
        ALOGE("(%s,%d): Set frame rate %d successfully!",
            __FUNCTION__, __LINE__, parm.parm.capture.timeperframe.denominator);
    }
#endif
    return 0;
}

void V4L2Camera::Close ()
{
    close(fd);
}

int V4L2Camera::Init()
{
    int ret;

    /* Check if camera can handle NB_BUFFER buffers */
    videoIn->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->rb.memory = V4L2_MEMORY_MMAP;
    videoIn->rb.count = NB_BUFFER;

    ret = ioctl(fd, VIDIOC_REQBUFS, &videoIn->rb);
    if (ret < 0) {
        ALOGE("Init: VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    if (videoIn->rb.count < NB_BUFFER)
	{
	    ALOGE("Init: Insufficient buffer memory!");
        return -1;
	}
	
    for (int i = 0; i < NB_BUFFER; i++) {

        memset (&videoIn->buf, 0, sizeof (struct v4l2_buffer));

        videoIn->buf.index = i;
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl (fd, VIDIOC_QUERYBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: Unable to query buffer (%s)", strerror(errno));
            return ret;
        }

        videoIn->mem[i] = mmap (0,
               videoIn->buf.length,
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               fd,
               videoIn->buf.m.offset);

        if (videoIn->mem[i] == MAP_FAILED) {
            ALOGE("Init: Unable to map buffer (%s)", strerror(errno));
            return -1;
        }

        ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("Init: VIDIOC_QBUF Failed");
            return -1;
        }

        nQueued++;
    }

    return 0;
}

void V4L2Camera::Uninit ()
{
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue everything */
    int DQcount = nQueued - nDequeued;

    for (int i = 0; i < DQcount-1; i++) {
        ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0)
            ALOGE("Uninit: VIDIOC_DQBUF Failed");
    }
    nQueued = 0;
    nDequeued = 0;

    /* Unmap buffers */
    for (int i = 0; i < NB_BUFFER; i++)
        if (munmap(videoIn->mem[i], videoIn->buf.length) < 0)
            ALOGE("Uninit: Unmap failed");
}

int V4L2Camera::StartStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (!videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (fd, VIDIOC_STREAMON, &type);
        if (ret < 0) {
            ALOGE("StartStreaming: Unable to start capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = true;
    }
    ALOGE("StartStreaming: Started without error");

    return 0;
}

int V4L2Camera::StopStreaming ()
{
    enum v4l2_buf_type type;
    int ret;

    if (videoIn->isStreaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            ALOGE("StopStreaming: Unable to stop capture: %s", strerror(errno));
            return ret;
        }

        videoIn->isStreaming = false;
    }

    return 0;
}

/* R = Y * 1.164 + Cr * 1.596 - 223 */
/* G = Y * 1.164 - Cr * 0.813 - Cb * 0.391 + 135 */
/* B = Y * 1.164 + Cb * 2.018 - 277 */
static int32_t s_ai32_Matrix_YCbCrtoRGB[20] = 
{ 
	(int32_t) ( 1.164f * (1 << 8)),   /*  Y factor for R */
	(int32_t) 0,                       /* Cb factor for R */
	(int32_t) ( 1.596f * (1 << 8)),   /* Cr factor for R */
	(int32_t) 0,                       /*  A factor for R */
	(int32_t) (-223 * (1 << 8)),      /* Increment for R */
	(int32_t) ( 1.164f * (1 << 8)),   /*  Y factor for G */
	(int32_t) (-0.391f * (1 << 8)),   /* Cb factor for G */
	(int32_t) (-0.813f * (1 << 8)),   /* Cr factor for G */
	(int32_t) 0,                       /*  A factor for G */
	(int32_t) (134 * (1 << 8)),       /* Increment for G */
	(int32_t) ( 1.164f * (1 << 8)),   /*  Y factor for B */
	(int32_t) ( 2.018f * (1 << 8)),   /* Cb factor for B */
	(int32_t) 0,                       /* Cr factor for B */
	(int32_t) 0,                       /*  A factor for B */
	(int32_t) (-277 * (1 << 8)),      /* Increment for B */
	(int32_t) 0,                       /*  Y factor for A */
	(int32_t) 0,                       /* Cb factor for A */
	(int32_t) 0,                       /* Cr factor for A */
	(int32_t) 0,               /*  A factor for A */
	(int32_t) 0,                       /* Increment for A */
};

#define USE_NEXUS_GRAPHICS2D
#define CHECK_BLIT_DONE

int blitDone = 0;

void bcm_cscdone(void *data, int unused)
{
    int *blitDone = (int *)data;
    ALOGV("Blit Completed\n");
    *blitDone=1;
}

void V4L2Camera::GrabPreviewFrame (void *previewBuffer)
{
    unsigned char *tmpBuffer;
    int ret;
    int i;
#ifdef USE_NEXUS_GRAPHICS2D    
    NEXUS_Graphics2DSettings gfxSettings;
    NEXUS_SurfaceHandle nxYuvSurface, nxRgbSurface;
    NEXUS_Graphics2DHandle nxGfx2d;
    NEXUS_SurfaceCreateSettings nxCreateSettings;
    NEXUS_Graphics2DBlitSettings nxBlitSettings;
    NEXUS_SurfaceMemory nxMemory;
    NEXUS_Error rc;

    NEXUS_Surface_GetDefaultCreateSettings(&nxCreateSettings);
    nxCreateSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;
    nxCreateSettings.width = videoIn->width;
    nxCreateSettings.height = videoIn->height;
    nxCreateSettings.bitsPerPixel = 32;
    nxYuvSurface = NEXUS_Surface_Create(&nxCreateSettings);
    
    NEXUS_Surface_GetDefaultCreateSettings(&nxCreateSettings);
    nxCreateSettings.pixelFormat = NEXUS_PixelFormat_eR5_G6_B5;
    nxCreateSettings.width = videoIn->width;
    nxCreateSettings.height = videoIn->height;
    nxCreateSettings.bitsPerPixel = 32;
    nxRgbSurface = NEXUS_Surface_Create(&nxCreateSettings);

    nxGfx2d = NEXUS_Graphics2D_Open(0, NULL);

#ifdef CHECK_BLIT_DONE
    NEXUS_Graphics2D_GetSettings(nxGfx2d, &gfxSettings);
    gfxSettings.checkpointCallback.callback = bcm_cscdone;
    gfxSettings.checkpointCallback.context = (void *) &blitDone;
    NEXUS_Graphics2D_SetSettings(nxGfx2d, &gfxSettings);
#endif

#else /* else of USE_NEXUS_GRAPHICS2D */

    tmpBuffer = (unsigned char *) calloc (1, videoIn->width * videoIn->height * 2);

#endif
    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_DQBUF Failed");

        return;
    }
    nDequeued++;

#ifdef USE_NEXUS_GRAPHICS2D  
    NEXUS_Surface_GetMemory(nxYuvSurface, &nxMemory);
    memcpy((unsigned char *)nxMemory.buffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
    NEXUS_Surface_Flush(nxYuvSurface);
    
    NEXUS_Graphics2D_GetDefaultBlitSettings(&nxBlitSettings);
    nxBlitSettings.source.surface = nxYuvSurface;
    nxBlitSettings.source.rect.width = videoIn->width;
    nxBlitSettings.source.rect.height = videoIn->height;
    nxBlitSettings.output.surface = nxRgbSurface;
    nxBlitSettings.output.rect.width = videoIn->width;
    nxBlitSettings.output.rect.height = videoIn->height;
    nxBlitSettings.conversionMatrixEnabled = true;
    nxBlitSettings.conversionMatrix.shift = 8;
    for (i = 0; i < 20; i++)
        nxBlitSettings.conversionMatrix.coeffMatrix[i] = s_ai32_Matrix_YCbCrtoRGB[i];


    NEXUS_Graphics2D_Blit(nxGfx2d, &nxBlitSettings);

#ifdef CHECK_BLIT_DONE
    rc = NEXUS_Graphics2D_Checkpoint(nxGfx2d, NULL);
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 700;
    if(rc == 0x1010002) 
    //if(rc == NEXUS_GRAPHICS2D_QUEUED) - GIVES COMPILATION ERROR
    {
        ALOGV("Blit Queued\n");
        while(!blitDone)
        {
            nanosleep(&ts, NULL);
        }
    }else if (rc) {
        return;
    }
    blitDone = 0;
#endif

    NEXUS_Surface_Flush(nxRgbSurface);
    NEXUS_Surface_GetMemory(nxRgbSurface, &nxMemory);
    memcpy((unsigned char *)previewBuffer, (unsigned char *)nxMemory.buffer, (size_t) videoIn->buf.bytesused);

#else /* else of USE_NEXUS_GRAPHICS2D */
    memcpy (tmpBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);

    //convertYUYVtoYUV422SP((uint8_t *)tmpBuffer, (uint8_t *)previewBuffer, videoIn->width, videoIn->height);
    convert((unsigned char *)tmpBuffer, (unsigned char *)previewBuffer, videoIn->width, videoIn->height);
#endif 
    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_QBUF Failed");
        return;
    }

    nQueued++;

#ifdef USE_NEXUS_GRAPHICS2D    
    NEXUS_Surface_Destroy(nxYuvSurface);
    NEXUS_Surface_Destroy(nxRgbSurface);
    NEXUS_Graphics2D_Close(nxGfx2d);
#else
    free(tmpBuffer);
#endif
}


void V4L2Camera::GrabPreviewFrameToPreviewWindow (void *previewBuffer, int w, int h)
{
    unsigned char *tmpBuffer;
    int ret;
    int i;
#ifdef USE_NEXUS_GRAPHICS2D    
    NEXUS_Graphics2DSettings gfxSettings;
    NEXUS_SurfaceHandle nxYuvSurface, nxRgbSurface;
    NEXUS_Graphics2DHandle nxGfx2d;
    NEXUS_SurfaceCreateSettings nxCreateSettings;
    NEXUS_Graphics2DBlitSettings nxBlitSettings;
    NEXUS_SurfaceMemory nxMemory;
    NEXUS_Error rc;
	NEXUS_Graphics2DOpenSettings openGraphicsSettings;
	NEXUS_ClientConfiguration clientConfig;

    NEXUS_Surface_GetDefaultCreateSettings(&nxCreateSettings);
    nxCreateSettings.pixelFormat = NEXUS_PixelFormat_eCr8_Y18_Cb8_Y08;
    nxCreateSettings.width = videoIn->width;
    nxCreateSettings.height = videoIn->height;
//    nxCreateSettings.bitsPerPixel = 32;
    nxYuvSurface = NEXUS_Surface_Create(&nxCreateSettings);
    
    /* create from preview buffer */
    NEXUS_Surface_GetDefaultCreateSettings(&nxCreateSettings);
    nxCreateSettings.pixelFormat = NEXUS_PixelFormat_eR5_G6_B5;
    nxCreateSettings.width = w;
    nxCreateSettings.height = h;
    nxCreateSettings.pMemory = previewBuffer;
    nxRgbSurface = NEXUS_Surface_Create(&nxCreateSettings);

    
	NEXUS_Graphics2D_GetDefaultOpenSettings(&openGraphicsSettings);
	NEXUS_Platform_GetClientConfiguration(&clientConfig);
	openGraphicsSettings.heap = clientConfig.heap[1];

	nxGfx2d = NEXUS_Graphics2D_Open(0, &openGraphicsSettings);
    
    if (nxGfx2d == NULL)
    {
        
		memset(previewBuffer,0xff,w*h*2);
		NEXUS_Surface_Destroy(nxYuvSurface);
		NEXUS_Surface_Destroy(nxRgbSurface);
		return ;
    }
#ifdef CHECK_BLIT_DONE
    NEXUS_Graphics2D_GetSettings(nxGfx2d, &gfxSettings);
    gfxSettings.checkpointCallback.callback = bcm_cscdone;
    gfxSettings.checkpointCallback.context = (void *) &blitDone;
    NEXUS_Graphics2D_SetSettings(nxGfx2d, &gfxSettings);
#endif

#else /* else of USE_NEXUS_GRAPHICS2D */
 
    tmpBuffer = (unsigned char *) calloc (1, videoIn->width * videoIn->height * 2);

#endif
    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_DQBUF Failed");

        return;
    }
    nDequeued++;

#ifdef USE_NEXUS_GRAPHICS2D  
    NEXUS_Surface_GetMemory(nxYuvSurface, &nxMemory);
    memcpy((unsigned char *)nxMemory.buffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
    NEXUS_Surface_Flush(nxYuvSurface);

   /* Blit the whole frame to preview window */	
    NEXUS_Graphics2D_GetDefaultBlitSettings(&nxBlitSettings);
    nxBlitSettings.source.surface = nxYuvSurface;
    nxBlitSettings.output.surface = nxRgbSurface;
    NEXUS_Graphics2D_Blit(nxGfx2d, &nxBlitSettings);

#ifdef CHECK_BLIT_DONE
    rc = NEXUS_Graphics2D_Checkpoint(nxGfx2d, NULL);
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 700;
    if(rc == 0x1010002) 
    //if(rc == NEXUS_GRAPHICS2D_QUEUED) - GIVES COMPILATION ERROR
    {
        ALOGV("Blit Queued\n");
        while(!blitDone)
        {
            nanosleep(&ts, NULL);
        }
    }else if (rc) {
		NEXUS_Surface_Destroy(nxYuvSurface);
		NEXUS_Surface_Destroy(nxRgbSurface);
		NEXUS_Graphics2D_Close(nxGfx2d);
        return;
    }
    blitDone = 0;
#endif

#else /* else of USE_NEXUS_GRAPHICS2D */
    memcpy (tmpBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);

    //convertYUYVtoYUV422SP((uint8_t *)tmpBuffer, (uint8_t *)previewBuffer, videoIn->width, videoIn->height);
    convert((unsigned char *)tmpBuffer, (unsigned char *)previewBuffer, videoIn->width, videoIn->height);
#endif 
    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabPreviewFrame: VIDIOC_QBUF Failed");
        return;
    }

    nQueued++;

#ifdef USE_NEXUS_GRAPHICS2D    
    NEXUS_Surface_Destroy(nxYuvSurface);
    NEXUS_Surface_Destroy(nxRgbSurface);
    NEXUS_Graphics2D_Close(nxGfx2d);
#else
    free(tmpBuffer);
#endif
}

void V4L2Camera::GrabRecordFrame (void *recordBuffer)
{
    //unsigned char *tmpBuffer;
    int ret;

    //tmpBuffer = (unsigned char *) calloc (1, videoIn->width * videoIn->height * 2);

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabRecordFrame: VIDIOC_DQBUF Failed");

        return;
    }
    nDequeued++;

    #if 0
    memcpy (tmpBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
	#else
	memcpy (recordBuffer, videoIn->mem[videoIn->buf.index], (size_t) videoIn->buf.bytesused);
    #endif
	/*fwrite((uint8_t *)tmpBuffer, 1, 352*288*2, fyuv422);*/
	

    #if 0
    yuyv422_to_yuv420((unsigned char *)tmpBuffer, (unsigned char *)recordBuffer, videoIn->width, videoIn->height);
    #endif
	/*fwrite((uint8_t *)recordBuffer, 1, 352*288*2, fyuv420);*/

    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabRecordFrame: VIDIOC_QBUF Failed");
        return;
    }

    nQueued++;

    //free(tmpBuffer);
}

sp<IMemory> V4L2Camera::GrabRawFrame ()
{
    sp<MemoryHeapBase> memHeap = new MemoryHeapBase(videoIn->width * videoIn->height * 2);
    sp<MemoryBase> memBase = new MemoryBase(memHeap, 0, videoIn->width * videoIn->height * 2);

    // Not yet implemented, do I have to?

    return memBase;
}

// a helper class for jpeg compression in memory
class MemoryStream {
public:
    MemoryStream(char* buf, size_t sz);
    ~MemoryStream() { closeStream(); }

    void closeStream();
    size_t getOffset() const { return bytesWritten; }
    operator FILE *() { return file; }

private:
    static int runThread(void *);
    int readPipe();

    char *buffer;
    size_t size;
    size_t bytesWritten;
    int pipefd[2];
    FILE *file;
    Mutex lock;
    Condition exitedCondition;
};

MemoryStream::MemoryStream(char* buf, size_t sz)
    : buffer(buf), size(sz), bytesWritten(0)
{
    if ((file = pipe(pipefd) ? NULL : fdopen(pipefd[1], "w")))
        createThread(runThread, this);
}

void MemoryStream::closeStream()
{
    if (file) {
        AutoMutex l(lock);
        fclose(file);
        file = NULL;
        exitedCondition.wait(lock);
    }
}

int MemoryStream::runThread(void *self)
{
    return static_cast<MemoryStream *>(self)->readPipe();
}

int MemoryStream::readPipe()
{
    char *buf = buffer;
    ssize_t sz;
    while ((sz = read(pipefd[0], buf, size - bytesWritten)) > 0) {
        bytesWritten += sz;
        buf += sz;
    }
    close(pipefd[0]);
    AutoMutex l(lock);
    exitedCondition.signal();
    return 0;
}

sp<IMemory> V4L2Camera::GrabJpegFrame ()
{
    int ret;

    videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoIn->buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue buffer */
    ret = ioctl(fd, VIDIOC_DQBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabJpegFrame: VIDIOC_DQBUF Failed");
        return NULL;
    }
    nDequeued++;

    ALOGI("GrabJpegFrame: Generated a frame from capture device");

    /* Enqueue buffer */
    ret = ioctl(fd, VIDIOC_QBUF, &videoIn->buf);
    if (ret < 0) {
        ALOGE("GrabJpegFrame: VIDIOC_QBUF Failed");
        return NULL;
    }
    nQueued++;

    size_t bytesused = videoIn->buf.bytesused;
    if (char *tmpBuf = new char[bytesused]) {
        MemoryStream strm(tmpBuf, bytesused);
        saveYUYVtoJPEG((unsigned char *)videoIn->mem[videoIn->buf.index], videoIn->width, videoIn->height, strm, 100);
        strm.closeStream();
        size_t fileSize = strm.getOffset();

        sp<MemoryHeapBase> mjpegPictureHeap = new MemoryHeapBase(fileSize);
        sp<MemoryBase> jpegmemBase = new MemoryBase(mjpegPictureHeap, 0, fileSize);
        memcpy(mjpegPictureHeap->base(), tmpBuf, fileSize);
        delete[] tmpBuf;

        return jpegmemBase;
    }

    return NULL;
}

int V4L2Camera::saveYUYVtoJPEG (unsigned char *inputBuffer, int width, int height, FILE *file, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;
    int fileSize;

    line_buffer = (unsigned char *) calloc (width * 3, 1);
    yuyv = inputBuffer;

    cinfo.err = jpeg_std_error (&jerr);
    jpeg_create_compress (&cinfo);
    jpeg_stdio_dest (&cinfo, file);

    ALOGI("JPEG PICTURE WIDTH AND HEIGHT: %dx%d", width, height);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults (&cinfo);
    jpeg_set_quality (&cinfo, quality, TRUE);

    jpeg_start_compress (&cinfo, TRUE);

    z = 0;
    while (cinfo.next_scanline < cinfo.image_height) {
        int x;
        unsigned char *ptr = line_buffer;

        for (x = 0; x < width; x++) {
            int r, g, b;
            int y, u, v;

            if (!z)
                y = yuyv[0] << 8;
            else
                y = yuyv[2] << 8;

            u = yuyv[1] - 128;
            v = yuyv[3] - 128;

            r = (y + (359 * v)) >> 8;
            g = (y - (88 * u) - (183 * v)) >> 8;
            b = (y + (454 * u)) >> 8;

            *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
            *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
            *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

            if (z++) {
                z = 0;
                yuyv += 4;
            }
        }

        row_pointer[0] = line_buffer;
        jpeg_write_scanlines (&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress (&cinfo);
    fileSize = ftell(file);
    jpeg_destroy_compress (&cinfo);

    free (line_buffer);

    return fileSize;
}

void V4L2Camera::convert(unsigned char *buf, unsigned char *rgb, int width, int height)
{
    int x,y,z=0;
    int blocks;

    blocks = (width * height) * 2;

    for (y = 0; y < blocks; y+=4) {
        unsigned char Y1, Y2, U, V;

        Y1 = buf[y + 0];
        U = buf[y + 1];
        Y2 = buf[y + 2];
        V = buf[y + 3];

        yuv_to_rgb16(Y1, U, V, &rgb[y]);
        yuv_to_rgb16(Y2, U, V, &rgb[y + 2]);
    }

}

void V4L2Camera::yuv_to_rgb16(unsigned char y, unsigned char u, unsigned char v, unsigned char *rgb)
{
    int r,g,b;
    int z;
    int rgb16;

    z = 0;

    r = 1.164 * (y - 16) + 1.596 * (v - 128);
    g = 1.164 * (y - 16) - 0.813 * (v - 128) - 0.391 * (u -128);
    b = 1.164 * (y - 16) + 2.018 * (u - 128);

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    rgb16 = (int)(((r >> 3)<<11) | ((g >> 2) << 5)| ((b >> 3) << 0));

    *rgb = (unsigned char)(rgb16 & 0xFF);
    rgb++;
    *rgb = (unsigned char)((rgb16 & 0xFF00) >> 8);

}

}; // namespace android
