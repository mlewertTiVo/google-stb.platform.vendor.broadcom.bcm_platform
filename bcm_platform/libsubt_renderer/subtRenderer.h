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

#ifndef _SUBT_RENDERER_H
#define _SUBT_RENDERER_H

#include <surfaceflinger/Surface.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <surfaceflinger/ISurfaceComposerClient.h>
#include <surfaceflinger/ISurfaceComposer.h>

typedef enum{
	SUBTITLE_DATA_BITMAP=0,
	SUBTITLE_DATA_ASCII,
	/** add here if additional coded text support in future */
} SUBTITLE_DATA_TYPE;

namespace android {

class SubtitleRenderer : public RefBase{
public:

	SubtitleRenderer(SUBTITLE_DATA_TYPE eSubtitleDataType);
	virtual ~SubtitleRenderer();
	int32_t RequestSurface();
	void setRegion(int32_t x, int32_t y, int32_t w, int32_t h);
	void getRegion(int32_t *x, int32_t *y, int32_t *w, int32_t *h) const;
	bool drawSubtitle(const void *data, size_t byteLength);
	bool enableSubtitle(bool bEnable);
	bool isSubtitleEnabled(void);
	bool hideSubtitle(bool bHide);
	bool isSubtitleVisible(void);
	void eraseSubtitle(void);

    bool mEnabled;
    bool mVisible;

    struct Locked {
        int32_t mX;
        int32_t mY;
        int32_t mW;
        int32_t mH;
        int32_t displayOrientation;
    } mLocked;
    SUBTITLE_DATA_TYPE mSubtitleType;

private:
    sp<SurfaceComposerClient> mSurfaceComposerClient;
    sp<SurfaceControl> mSurfaceControl;    

};

}

#endif /** _SUBT_RENDERER_H */

