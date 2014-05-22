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
 * $brcm_Workfile:  $
 * $brcm_Revision:  $
 * $brcm_Date:  $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log:  $
 * 
 * 
 *****************************************************************************/

#include <cutils/log.h>
#include <cutils/memory.h>
#include <utils/String8.h>

#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkColor.h>
#include <SkPaint.h>
#include <SkXfermode.h>
 
#include "subtRenderer.h"

#define LOG_TAG "SubtRenderer"
#define DBG_MSG LOGD("%s()", __FUNCTION__)

namespace android {

SubtitleRenderer::SubtitleRenderer(SUBTITLE_DATA_TYPE eSubtitleDataType){
	DBG_MSG;

	if (mSurfaceComposerClient == NULL) {
        mSurfaceComposerClient = new SurfaceComposerClient();
		mSurfaceComposerClient->incStrong(this);//This is necessary or it causes crash after ~SubtitleRender
		LOGD("New SurfaceComposerClient for subtitle");
    }

	mSubtitleType = eSubtitleDataType;
	mEnabled = true;
	mVisible = true;
}


SubtitleRenderer::~SubtitleRenderer(){
	DBG_MSG;
}

int32_t SubtitleRenderer::RequestSurface(){
	DBG_MSG;

    if (mSurfaceControl == NULL) {
        mSurfaceControl = mSurfaceComposerClient->createSurface(
                String8("Subtitle Surface"), 0,
                mLocked.mW, mLocked.mH,
                PIXEL_FORMAT_RGBA_8888, 0);
		LOGD("Subtitle Surface Created");
        if (mSurfaceControl == NULL) {
            LOGE("Error creating subtitle surface.");
            return -1;
        }
		mSurfaceComposerClient->openGlobalTransaction();
		mSurfaceControl->setLayer(100000);
		mSurfaceControl->setPosition(mLocked.mX, mLocked.mY);
		mSurfaceControl->setSize(mLocked.mW, mLocked.mH);
		mSurfaceComposerClient->closeGlobalTransaction();		
    }

	return 0;
}

void SubtitleRenderer::setRegion(int32_t x, int32_t y, int32_t w, int32_t h){
	DBG_MSG;

	mLocked.mX = x;
	mLocked.mY = y;
	mLocked.mW = w;
	mLocked.mH = h;
	RequestSurface();

}

void SubtitleRenderer::getRegion(int32_t *x, int32_t *y, int32_t *w, int32_t *h) const{
	DBG_MSG;

	*x = mLocked.mX;
	*y = mLocked.mY;
	*w = mLocked.mW;
	*h = mLocked.mH;
}

bool SubtitleRenderer::hideSubtitle(bool bHide){
	LOGD("%s(%d)", __FUNCTION__, bHide);
	
	mVisible = bHide? false:true;

	mSurfaceComposerClient->openGlobalTransaction();
	if(mEnabled && mVisible) 
		mSurfaceControl->show();
	else
		mSurfaceControl->hide();
	mSurfaceComposerClient->closeGlobalTransaction();

	return true;
}

bool SubtitleRenderer::isSubtitleVisible(void){
	DBG_MSG;
	return mVisible;
}


bool SubtitleRenderer::enableSubtitle(bool bEnable){
	LOGD("%s(%d)", __FUNCTION__, bEnable);
	//hide/unhide control
	mEnabled = bEnable;

	mSurfaceComposerClient->openGlobalTransaction();
	if(mEnabled && mVisible)
		mSurfaceControl->show();
	else
		mSurfaceControl->hide();
	mSurfaceComposerClient->closeGlobalTransaction();

	return true;
}

bool SubtitleRenderer::isSubtitleEnabled(void){
	DBG_MSG;
	return mEnabled;
}

bool SubtitleRenderer::drawSubtitle(const void *data, size_t byteLength) {
	DBG_MSG;
	LOGD("ByteLength:%d", byteLength);

    if (mEnabled && mVisible) {

        sp<Surface> surface = mSurfaceControl->getSurface();

        Surface::SurfaceInfo surfaceInfo;
        status_t status = surface->lock(&surfaceInfo);
        if (status) {
            LOGE("Error %d locking subtitle surface before drawing.", status);
            return false;
        }

        SkBitmap surfaceBitmap;
        ssize_t bpr = surfaceInfo.s * bytesPerPixel(surfaceInfo.format);
        surfaceBitmap.setConfig(SkBitmap::kARGB_8888_Config, surfaceInfo.w, surfaceInfo.h, bpr);
		//surfaceBitmap.allocPixels();
        surfaceBitmap.setPixels(surfaceInfo.bits);
		surfaceBitmap.eraseColor(0);

        SkCanvas surfaceCanvas;
        surfaceCanvas.setBitmapDevice(surfaceBitmap);

        SkPaint paint;

		paint.setColor(SK_ColorWHITE);
		paint.setTextSize(SkIntToScalar(24));
		//paint.setTextScaleX(SkFloatToScalar(0.75f));
		//paint.setXfermodeMode(SkXfermode::kSrc_Mode);

		switch(mSubtitleType){
			case SUBTITLE_DATA_ASCII:
				surfaceCanvas.drawText(data, byteLength, 0, 2*mLocked.mH/3, paint);
				//surfaceCanvas.drawLine( 0,0, 100,100,paint);
				LOGD("Type:ASCII, Drew the text via SkCanvas byteLength=%d", byteLength);
				break;
				
			default:
			case SUBTITLE_DATA_BITMAP:
				LOGE("Unsupported subtitle format!");
				return false;
				break;
		}

        status = surface->unlockAndPost();
		LOGD("Locked and posted subtitle surface");
		
        if (status) {
            LOGE("Error %d unlocking subtitle surface after drawing.", status);
            return false;
        }

	    return true;
    }
	else{
		return false;
	}

}


}

