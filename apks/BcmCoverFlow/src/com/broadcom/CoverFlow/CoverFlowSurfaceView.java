/******************************************************************************
 *    (c)2009-2010 Broadcom Corporation
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
 * $brcm_Workfile: CoverFlowSurfaceView.java $
 * $brcm_Revision:  $
 * $brcm_Date:  $
 *
 * Module Description:
 *
 * Revision History:
 * 
 * 1   11/15/10 12:14p franktcc
 * - Initial version
 *
 *****************************************************************************/

package com.broadcom.CoverFlow;

import com.broadcom.CoverFlow.R;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.util.Log;
import android.os.Environment;
import android.net.Uri;
import java.io.File;
import java.io.FilenameFilter;
import java.io.FileNotFoundException;
import java.io.IOException;
import android.content.res.AssetFileDescriptor;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;

class CoverFlowSurfaceView extends SurfaceView implements SurfaceHolder.Callback{

    /** Obsoleted */
    class MyDecodeThread extends Thread {

        /** Task running control flag */
        private boolean mRun = false;

        /** Draw thread constructor */
        public MyDecodeThread(SurfaceHolder surfaceHolder, Context context,
                Handler handler) {
                Log.d(TAG, "Instantiated MyDecodeThread");
        }
        
        public void setRunning(boolean b) {
            mRun = b;
        }

        @Override
        public void run() {

            //Link drawable to the rest of resource pictures
            for(int iIdx = iNumOfPreloadImg; iIdx < aiCoverResList.length; iIdx++){

                if(mRun == false) break;
                
                // If cover is moving, we stop decoding the images
                while(bMovingCovers) 
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                    
                if(mRun == false) break;
                
                Log.d(TAG, "Loading " + iIdx + "-th image");
                DrawThread.setDrawable(mContext.getResources().getDrawable(aiCoverResList[iIdx]), iIdx);

                if(mRun == false) break;
                                
                //Must reload the position so picture aspect ratio can be adjusted.
                DrawThread.refreshCoverPosition();
            }
        }
    }
    
    
    class MyDrawThread extends Thread {
        
        /** The drawable to use as the background of the animation canvas */
        private Bitmap mBackgroundImage;
        
        /** How fast we move the covers, lower value has faster speed */
        private static final int MOVING_SPEED = 3;
        
        /**
         * Define fixed locations of covers in the view
         */
        private int[] iDefCoverLeft;
        private int[] iDefCoverTop;
        private int[] iDefCoverRight;
        private int[] iDefCoverBottom;
       
        
        /** Canvas' Rotation Degree */
        private int iRotDegree = 0;

        /** Canvas' Scale */
        private int iScale = 0;

        /** Relative size of the covers */
        private int iFocusCoverSize = 0;
        private int iOtherCoverSize = 0;
        
        /** Handle to the surface manager object we interact with */
        private SurfaceHolder mSurfaceHolder;

        /** Indicate whether the surface has been created & is ready to draw */
        private boolean mRun = false;

        private Uri[] mPosterUris;  
     
        /** Draw thread constructor */
        public MyDrawThread(SurfaceHolder surfaceHolder, Context context, Handler handler) {
            File[] fileImagelist = null;
            
            iDefCoverLeft = new int[DISPLAY_COVER_NUM];
            iDefCoverTop = new int[DISPLAY_COVER_NUM];
            iDefCoverRight = new int[DISPLAY_COVER_NUM];
            iDefCoverBottom = new int[DISPLAY_COVER_NUM];

            //Pass handles to class member
            mSurfaceHolder = surfaceHolder;
            mContext = context;
            
            //Prepare to access application's resources
            Resources res = mContext.getResources();

            //Open designate directory
            appDir = new File(Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow");

            //If external images exists, use it. If not, use built-in.
            if(appDir.isDirectory()){            
                Log.d(TAG, "BcmCoverFlow dir found");
                fileImagelist = appDir.listFiles(new FilenameFilter(){  
                    @Override  
                    public boolean accept(File dir, String name)  
                    {  
                        return ((name.endsWith(".jpg"))||(name.endsWith(".JPG"))
                              ||(name.endsWith(".png"))||(name.endsWith(".PNG")));
                    }  
                });

                iNumberOfPosters = fileImagelist.length;
                Log.d(TAG, "Found "+iNumberOfPosters+" accepted images in "+appDir);
                if(iNumberOfPosters > MAX_MOVIE_POSTER){
                    Log.w(TAG, "Limited posters from "+iNumberOfPosters+" to "+MAX_MOVIE_POSTER);
                    iNumberOfPosters = MAX_MOVIE_POSTER;
                }

                if(0 != iNumberOfPosters){
                    mExtPoster = true;
                }
                else{
                    iNumberOfPosters = aiCoverResList.length-1;/* -1 to remove default loading image */
                    mExtPoster = false;
                    Log.w(TAG, 
                             "No any png or jpg found in "+Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow");
                }
            }
            else{
                //No image in the dir, use built-in images.
                iNumberOfPosters = aiCoverResList.length;
                mExtPoster = false;
                Log.d(TAG, "Folder "+Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow"+" doesn't exist");
            }

            //Either loading poster images from external dir or built-in
            if(mExtPoster){
                Log.d(TAG, "Using external dir for images");

                AssetFileDescriptor fileDescriptor =null;
                Bitmap bm = null;

                
                mPosterPaths = new String[iNumberOfPosters];  
                
                for(int iIdx= 0 ; iIdx< iNumberOfPosters; iIdx++)  
                {  
                    mPosterPaths[iIdx] = fileImagelist[iIdx].getAbsolutePath();  
                }

                Log.d(TAG, "iNumberOfPosters = "+iNumberOfPosters);

                mPosterUris = new Uri[iNumberOfPosters];  
                
                for(int iIdx=0; iIdx < iNumberOfPosters; iIdx++)  
                {  
                    mPosterUris[iIdx] = Uri.parse("file://"+mPosterPaths[iIdx]);     

                    try {
                        fileDescriptor = getContext().getContentResolver().openAssetFileDescriptor(mPosterUris[iIdx],"r");
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                    }
                    finally{
                        try {
                            bm = BitmapFactory.decodeFileDescriptor(fileDescriptor.getFileDescriptor(), null, null);
                            fileDescriptor.close();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }

                    astCovers[iIdx] = new Cover();
                    astCovers[iIdx].drCover = new BitmapDrawable(bm);
                    astCovers[iIdx].bLoaded = true;

                    Log.d(TAG, "Loaded "+fileImagelist[iIdx].getAbsolutePath()+" into "+iIdx+"-th drawable");
                     
                }
            }
            else{
                Log.w(TAG, "Using default posters because posters not found in "+Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow");
                //Allocate memory for every cover
                for(int iIdx = 0; iIdx < iNumberOfPosters; iIdx++)
                astCovers[iIdx] = new Cover();
                
                //Link drawable to resource picture
                for(int iIdx = 0; iIdx < iNumberOfPosters; iIdx++){
                    astCovers[iIdx].drCover = context.getResources().getDrawable(aiCoverResList[iIdx]);
                    astCovers[iIdx].bLoaded = true;
                }                
                Log.d(TAG, "Set URI for covers...");
                if(iNumberOfPosters != aiCoverUriList.length){
                    Log.e(TAG, "Error! Cover number != Uri Number!");
                }
                //Load the last image which is the default image --- Loading...
                astCovers[iNumberOfPosters-1].drCover = context.getResources().getDrawable(aiCoverResList[iNumberOfPosters-1]);
                astCovers[iNumberOfPosters-1].bLoaded = true;
            }

            // load background image as a Bitmap instead of a Drawable b/c
            // we don't need to transform it and it's faster to draw this way
            mBackgroundImage = BitmapFactory.decodeResource(res,
                    R.drawable.bg7);

        }

        /** Once resource loaded, set drawable ready */
        public void setDrawable(Drawable newDrableReady, int iIdx){
            astCovers[iIdx].drCover = newDrableReady;
            astCovers[iIdx].bLoaded = true;
        }

        /**
         * Used to signal the thread whether it should be running or not.
         * Passing true allows the thread to run; passing false will shut it
         * down if it's already running. Calling start() after this was most
         * recently called with false will result in an immediate shutdown.
         * 
         * @param b true to run, false to shut down
         */
        public void setRunning(boolean b) {
            mRun = b;
        }
        

        /** Drawing thread */
        @Override
        public void run() {
            while (mRun) {

                //Make sure we start drawing after surfaceChanged callback received, 
                //or mSurfaceHolder sometimes be hold here and make rotation of pictures can not be seen.
                if(bSurfaceChanged){
                    Canvas c = null;
                    try {
                        c = mSurfaceHolder.lockCanvas(null);
                        synchronized (mSurfaceHolder) {
                            doDraw(c);
                        }
                    } finally {
                        // do this in a finally so that if an exception is thrown
                        // during the above, we don't leave the Surface in an
                        // inconsistent state
                        if (c != null) {
                            mSurfaceHolder.unlockCanvasAndPost(c);
                        }
                    }
                }
            }
        }
        
        /** Move covers */
        private boolean refreshCoverPosition(){
            //Log.d(TAG, "Hits refreshCoverPosition event");
            
            //Control which position to use for the cover
            int iPos;
            if(iFocusCoverIndex == 0) iPos = MiddleOfCovers;
            else if(iFocusCoverIndex < MiddleOfCovers) iPos = MiddleOfCovers - iFocusCoverIndex;
            else iPos = 0;

            //Set to current cover locations
            for(int iIdx = 0; iIdx < iNumberOfPosters; iIdx++){                
                if(iIdx < iFocusCoverIndex-MiddleOfCovers){
                    //For the covers beyond the left margin
                    astCovers[iIdx].iLeft = iDefCoverLeft[0]-iOtherCoverSize;
                    astCovers[iIdx].iTop = iDefCoverTop[0];
                    astCovers[iIdx].iRight = iDefCoverRight[0]-iOtherCoverSize;
                    astCovers[iIdx].iBottom = iDefCoverBottom[0];                        
                }
                else if(iIdx >= iFocusCoverIndex-MiddleOfCovers && iIdx <= iFocusCoverIndex+MiddleOfCovers){
                    //Covers on the view window
                    if(iPos < DISPLAY_COVER_NUM){
                        
                        int iIntrinsicW;
                        int iIntrinsicH;
                        int iAR_Adjustor;

                        if(astCovers[iIdx].bLoaded){
                            //Image is loaded, we take its intrinsic size for later adjustment reference
                            iIntrinsicW = astCovers[iIdx].drCover.getIntrinsicWidth();
                            iIntrinsicH = astCovers[iIdx].drCover.getIntrinsicHeight();
                            
                            //Protection for invalid value
                            if(iIntrinsicW == 0 || iIntrinsicH == 0){
                                iIntrinsicW = 1;
                                iIntrinsicH = 1;
                            }                            
                        }
                        else{
                            //This means no aspect ratio adjustment
                            iIntrinsicW = 1;
                            iIntrinsicH = 1;
                        }
                        
                        //The code we handle image's aspect ratio
                        if(iIntrinsicW <= iIntrinsicH){
                            if(iIdx == iFocusCoverIndex)
                                iAR_Adjustor = iFocusCoverSize - (iFocusCoverSize*iIntrinsicW/iIntrinsicH);
                            else
                                iAR_Adjustor = iOtherCoverSize - (iOtherCoverSize*iIntrinsicW/iIntrinsicH);

                            astCovers[iIdx].iLeft_Diff = iDefCoverLeft[iPos] - astCovers[iIdx].iLeft + (iAR_Adjustor/2);
                            astCovers[iIdx].iTop_Diff = iDefCoverTop[iPos] - astCovers[iIdx].iTop;
                            astCovers[iIdx].iRight_Diff = iDefCoverRight[iPos] - astCovers[iIdx].iRight - (iAR_Adjustor/2);;
                        }
                        else if(iIntrinsicW > iIntrinsicH){
                            if(iIdx == iFocusCoverIndex)
                                iAR_Adjustor = iFocusCoverSize - (iFocusCoverSize*iIntrinsicH/iIntrinsicW);
                            else
                                iAR_Adjustor = iOtherCoverSize - (iOtherCoverSize*iIntrinsicH/iIntrinsicW);
                            
                            astCovers[iIdx].iLeft_Diff = iDefCoverLeft[iPos] - astCovers[iIdx].iLeft;
                            astCovers[iIdx].iTop_Diff = iDefCoverTop[iPos] - astCovers[iIdx].iTop + iAR_Adjustor;
                            astCovers[iIdx].iRight_Diff = iDefCoverRight[iPos] - astCovers[iIdx].iRight;
                        }
                        //The bottom line doesn't change for aspect ratio
                        astCovers[iIdx].iBottom_Diff = iDefCoverBottom[iPos] - astCovers[iIdx].iBottom;
                        iPos++;                        
                    }
                }
                else{
                    //For the covers beyond the right margin
                    astCovers[iIdx].iLeft = iDefCoverLeft[DISPLAY_COVER_NUM-1]+iOtherCoverSize;
                    astCovers[iIdx].iTop = iDefCoverTop[DISPLAY_COVER_NUM-1];
                    astCovers[iIdx].iRight = iDefCoverRight[DISPLAY_COVER_NUM-1]+iOtherCoverSize;
                    astCovers[iIdx].iBottom = iDefCoverBottom[DISPLAY_COVER_NUM-1];                        
                }
            }

            return true;
        }

        
        /**
         * Called by a thread to draw the covers on the view
         * Canvas.
         */
        private void doDraw(Canvas canvas) {
            // Draw the background image. Operations on the Canvas accumulate
            // so this is like clearing the screen.
            canvas.drawBitmap(mBackgroundImage, 0, 0, null);
            canvas.save();
            
            //Opening effect
            /*
            if(iRotDegree < 360){
                canvas.rotate((float)iRotDegree, (mCanvasWidth/2 - mCanvasWidth/8), (mCanvasHeight/2));
                canvas.scale((float)iScale/360, (float)iScale/360);
                iRotDegree+=10;
                iScale +=10;
            }
            */
            /*
            else{
                //After opening, scale covers big <--> small
                if(bSmaller == true)
                    iScale--;
                else
                    iScale++;
                
                if(iScale >= 360) bSmaller = true;
                
                if(iScale <= 350) bSmaller = false;
                
                canvas.scale((float)iScale/360, (float)iScale/360, (mCanvasWidth/2 - mCanvasWidth/8), (mCanvasHeight/2));
            }
            */
            

            int iTempStep;
            int iIdx;

            //Draw covers
            for (int iIndex = 0; iIndex < DISPLAY_COVER_NUM; iIndex++)
            {
                //Just for index adjustment, basically we want sequence like -2 +2 -1 +1 0, for handling drawing order
                if((iIndex%2) == 0)
                    iIdx = iFocusCoverIndex - (MiddleOfCovers-(iIndex/2));
                else
                    iIdx = iFocusCoverIndex + (MiddleOfCovers-(iIndex/2));
                                    
                //Protection
                if(iIdx < 0 || iIdx >= iNumberOfPosters) continue;

/*                
                //Show default loading... for those images which are not ready
                if(astCovers[iIdx].bLoaded != true){
                    astCovers[iIdx].drCover = astCovers[iNumberOfPosters-1].drCover;
                }
   */             
                //Calculate difference for left bound
                   iTempStep = astCovers[iIdx].iLeft_Diff/MOVING_SPEED;
                if(Math.abs(astCovers[iIdx].iLeft_Diff) > Math.abs(iTempStep)){
                    if(iTempStep == 0){
                        //Final moving
                        if(astCovers[iIdx].iLeft_Diff > 0) iTempStep = 1;
                        else iTempStep = -1;
                    }

                    astCovers[iIdx].iLeft += iTempStep;
                    astCovers[iIdx].iLeft_Diff -= iTempStep;                    
                }

                //Calculate difference for top bound
                iTempStep = astCovers[iIdx].iTop_Diff/MOVING_SPEED;
                if(Math.abs(astCovers[iIdx].iTop_Diff) > Math.abs(iTempStep)){
                    if(iTempStep == 0){
                        //Final moving
                        if(astCovers[iIdx].iTop_Diff > 0) iTempStep = 1;
                        else iTempStep = -1;
                    }

                    astCovers[iIdx].iTop += iTempStep;
                    astCovers[iIdx].iTop_Diff -= iTempStep;                    
                }

                //Calculate difference for right bound
                iTempStep = astCovers[iIdx].iRight_Diff/MOVING_SPEED;
                if(Math.abs(astCovers[iIdx].iRight_Diff) > Math.abs(iTempStep)){
                    if(iTempStep == 0){
                        //Final moving
                        if(astCovers[iIdx].iRight_Diff > 0) iTempStep = 1;
                        else iTempStep = -1;
                    }

                    astCovers[iIdx].iRight += iTempStep;
                    astCovers[iIdx].iRight_Diff -= iTempStep;                    
                }

                //Calculate difference for bottom bound
                iTempStep = astCovers[iIdx].iBottom_Diff/MOVING_SPEED;
                if(Math.abs(astCovers[iIdx].iBottom_Diff) > Math.abs(iTempStep)){
                    if(iTempStep == 0){
                        //Final moving
                        if(astCovers[iIdx].iBottom_Diff > 0) iTempStep = 1;
                        else iTempStep = -1;
                    }
                    
                    astCovers[iIdx].iBottom += iTempStep;
                    astCovers[iIdx].iBottom_Diff -= iTempStep;                    
                }

                //Detect if the cover is moving.
                if(iRotDegree < 360)
                    bMovingCovers = true;
                /*else
                    bMovingCovers = false;
                */
                else if(astCovers[iIdx].iLeft_Diff == 0 && astCovers[iIdx].iTop_Diff == 0
                        && astCovers[iIdx].iRight_Diff == 0 && astCovers[iIdx].iBottom_Diff == 0)
                    bMovingCovers = false;
                else
                    bMovingCovers = true;
                
                
                //Set the new drawable bounds
                astCovers[iIdx].drCover.setBounds(
                        astCovers[iIdx].iLeft,
                        astCovers[iIdx].iTop,
                        astCovers[iIdx].iRight,
                        astCovers[iIdx].iBottom
                        );
                
                
                //Transparency effect
                if(iIdx == iFocusCoverIndex)
                    astCovers[iIdx].drCover.setAlpha(255);
                else if (iIdx == iFocusCoverIndex - 1 || iIdx == iFocusCoverIndex + 1)
                    astCovers[iIdx].drCover.setAlpha(230);
                else if (iIdx == iFocusCoverIndex - 2 || iIdx == iFocusCoverIndex + 2)
                    astCovers[iIdx].drCover.setAlpha(200);
                else
                    astCovers[iIdx].drCover.setAlpha(150);


                
                //Finally, draw the cover
                astCovers[iIdx].drCover.draw(canvas);  
                
            }
        }
        
        /* Callback invoked when the surface dimensions change. */
        public void setSurfaceSize(int width, int height) {
            /* Make our canvas fit the surface */
            synchronized (mSurfaceHolder) {
                mCanvasWidth = width;
                mCanvasHeight = height;
                
                int iWidthUnit = mCanvasWidth/(2*DISPLAY_COVER_NUM+1);
                
                // The size for each cover, it's relevant to the position of cover
                if(DISPLAY_COVER_NUM == 7){
                    iFocusCoverSize = 5*iWidthUnit;
                    iOtherCoverSize = 2*iWidthUnit;
                }
                else if(DISPLAY_COVER_NUM == 5){
                    iFocusCoverSize = 4*iWidthUnit;
                    iOtherCoverSize = 7*iWidthUnit/4;                    
                }
                
                // Decide the cover positions
                for(int iIdx = 0; iIdx < DISPLAY_COVER_NUM; iIdx++){
                    
                    if(iIdx < MiddleOfCovers){
                        //Covers on the left-hand side
                        iDefCoverLeft[iIdx] = (iIdx*iOtherCoverSize)/2;
                    }
                    else if(iIdx == MiddleOfCovers){
                        iDefCoverLeft[iIdx] = (iIdx*iOtherCoverSize)/2 + iOtherCoverSize;
                    }
                    else{
                        //Covers on the right-hand side, some adjustment due to focused cover is bigger
                        iDefCoverLeft[iIdx] = (iIdx*iOtherCoverSize)/2 + iOtherCoverSize + iFocusCoverSize;
                    }
                    
                    if(iIdx == MiddleOfCovers){
                        // Focused cover
                        iDefCoverTop[iIdx] = mCanvasHeight/6;
                        iDefCoverRight[iIdx] = iDefCoverLeft[iIdx] + iFocusCoverSize;
                        iDefCoverBottom[iIdx] = iDefCoverTop[iIdx] + iFocusCoverSize;
                    }
                    else{
                        // The other covers
                        iDefCoverTop[iIdx] = mCanvasHeight-(mCanvasHeight/2);
                        
                        if(iIdx == MiddleOfCovers-1 || iIdx == MiddleOfCovers+1)
                            iDefCoverTop[iIdx]+=(mCanvasHeight/40);
                        
                        iDefCoverRight[iIdx] = iDefCoverLeft[iIdx] + iOtherCoverSize;
                        iDefCoverBottom[iIdx] = iDefCoverTop[iIdx] + iOtherCoverSize;                    
                    }
                    
                }

                Log.d(TAG, "setSurfaceSize mCanvasWidth="+mCanvasWidth);
                Log.d(TAG, "setSurfaceSize mCanvasHeight="+mCanvasHeight);

                //Trigger the initial drawing
                DrawThread.refreshCoverPosition();
                
                //Resize background image
                mBackgroundImage = Bitmap.createScaledBitmap(
                        mBackgroundImage, width, height, true);
            }
        }
    }
    
       /** Tag for log */
    private static final String TAG = "CoverFlowSurfaceView";
    
    /** Maximum number of cover image on the view, 5 or 7 are available */
    private int DISPLAY_COVER_NUM = 5;

    private int MAX_MOVIE_POSTER = 10;
    
    /** For internal use */
    private int MiddleOfCovers = DISPLAY_COVER_NUM/2;

    /** Number of pre-loaded images before user can see the view */
    private int iNumOfPreloadImg = DISPLAY_COVER_NUM/2 + 1;

    /** Always points to the cover we are focusing, can be changed if you want */
    private int iFocusCoverIndex = 0;
    
    /** Cover files default resource list, note that default image must be put in the end of list */
    private int[] aiCoverResList = { /*R.drawable.draft, R.drawable.monsters_inc_ver1_xlg, R.drawable.knowing, */
                                    R.drawable.p1, 
                                    R.drawable.p2,  
                                    R.drawable.p3, 
                                    R.drawable.p4,
                                    R.drawable.p5, 
                                    R.drawable.images_loading/** necessary end */
                                    };

    /** Cover files URI list, !!!!! The number of URI must meet aiCoverResList !!!!! */
    static String[] aiCoverUriList = {  "/BcmCoverFlow/elephantsdream-hd-large.mov",
                                        "/BcmCoverFlow/BigBuckBunny_320x180.mp4",
                                        "/BcmCoverFlow/Polygons8_HQ.mp4",
                                        "/BcmCoverFlow/sintel-1024-stereo.mp4",
                                        "/BcmCoverFlow/gov.archives.arc.49442_512kb.mp4",
                                        ""/** necessary end */
                                        };
    
    /**
     * Current height of the surface/canvas.
     * 
     * @see #setSurfaceSize
     */
    private int mCanvasHeight = 1;

    
    /**
     * Current width of the surface/canvas.
     * 
     * @see #setSurfaceSize
     */
    private int mCanvasWidth = 1;

    /** Handle to the application context, used to e.g. fetch Drawables. */
    private Context mContext;

    /** The thread that actually draws the animation */
    private MyDrawThread DrawThread;

    /** The thread that actually draws the animation */
    private MyDecodeThread DecThread;
    
    /** Indicate cover is moving or not */
    private boolean bMovingCovers = true;

    /** Indicate user is touching screen */
    private boolean mTouchingScreen = false;

    /** Surface Callback Receiving Flag */
    private boolean bSurfaceChanged = false;

    /** multi-task to load images */
    private static final boolean bThreadLoadImageEnable = false;/** temporary disable due to re-entering instability */

    private float fLastTouchX;
    /** A cover class contains all variables/reference we need for a cover in the view */
    class Cover{
        
        
        /** Cover's current bounds on the screen */
        private int iLeft;
        private int iRight;
        private int iTop;
        private int iBottom;
        
        /** The difference between current location and the location it approaching */
        private int iLeft_Diff;
        private int iTop_Diff;
        private int iRight_Diff;
        private int iBottom_Diff;

        /** See if the cover's drCover is loaded */
        private boolean bLoaded;

        /** The drawable class reference for  the class */
        private Drawable drCover;
                                   
        /** Constructor */
        public Cover(int left, int right, int top, int bottom){
            iLeft = left;
            iRight = right;
            iTop = top;
            iBottom = bottom;
            drCover = null;
            bLoaded = false;
            iLeft_Diff = 0;
            iTop_Diff = 0;
            iRight_Diff = 0;
            iBottom_Diff = 0;
        }
        
        /** Constructor w/o arg */
        public Cover(){
            this(0,0,0,0);
        }
    }

    /** Array for all covers */
    private Cover[] astCovers = new Cover[MAX_MOVIE_POSTER/*aiCoverResList.length(*/];

    /** extenal poster paths */
    private String[] mPosterPaths=null;

    /** BcmCoverFlow folder */
    private File appDir=null;

    /** Total number of posters */    
    private int iNumberOfPosters = 0;
    
    /** indicating poster is from internal or external */
    private boolean mExtPoster;

    /** Constructor */
    public CoverFlowSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        
        //Register our interest in hearing about changes to our surface
        SurfaceHolder holder = getHolder();
        holder.addCallback(this);

        Log.d(TAG, "Got SurfaceHolder");
        
        //Protection
        //if(iFocusCoverIndex >= iNumberOfPosters) iFocusCoverIndex = iNumberOfPosters - 1;
        //if(DISPLAY_COVER_NUM > iNumberOfPosters) DISPLAY_COVER_NUM = iNumberOfPosters - iNumberOfPosters%2 - 1;
        
        //Create thread only; it's started in surfaceCreated()
        DrawThread = new MyDrawThread(holder, context, new Handler() {
            @Override
            public void handleMessage(Message m) {
               
            }
        });
        Log.d(TAG, "Created MyDrawThread");

        //Create thread only; it's started in surfaceCreated()
        /* Obsoleted 
        if(bThreadLoadImageEnable) {
            DecThread = new MyDecodeThread(holder, context, new Handler() {
                @Override
                public void handleMessage(Message m) {
            
                }
            });
            Log.d(TAG, "Created MyDecodeThread");
           }
           */

        setFocusable(true); // make sure we get key events

        Log.d(TAG, "Instantiated CoverFlowSurfaceView");
    }
    
    public String getCoverURI(){
        if(true == mExtPoster){
            File[] fileVideolist = appDir.listFiles(new FilenameFilter(){
                @Override  
                public boolean accept(File dir, String name)  
                {  
                    return ((name.endsWith(".mp4"))||(name.endsWith(".MP4"))
                            || (name.endsWith(".mov"))||(name.endsWith(".MOV")));
                }  
            });

            
            for(int iIdx= 0 ; iIdx< fileVideolist.length; iIdx++)  
            {  
                if(true == mPosterPaths[iFocusCoverIndex].regionMatches(0, 
                                                    fileVideolist[iIdx].getAbsolutePath(), 
                                                    0, 
                                                    mPosterPaths[iFocusCoverIndex].length()-3/* ignore extension */)){
                    Log.d(TAG, "Found movie for the poster "+mPosterPaths[iFocusCoverIndex]);                             
                    return fileVideolist[iIdx].getAbsolutePath();
                }
                Log.d(TAG, "No match - "+fileVideolist[iIdx].getAbsolutePath());
            }

            Log.e(TAG, "Can't find movie for the poster "+mPosterPaths[iFocusCoverIndex]);

            String alertMsg = "Video not found for "+mPosterPaths[iFocusCoverIndex];
            ShowAlertDialog(alertMsg);

            return null;
        }
        else{
            return "rtsp://10.145.65.86/h264.ts";//Environment.getExternalStorageDirectory().getPath()+aiCoverUriList[iFocusCoverIndex];
        }
    }

    private void ShowAlertDialog(String sMsg)
    {
        AlertDialog.Builder MyAlertDialog = new AlertDialog.Builder(getContext());
        MyAlertDialog.setTitle("INFO");
        MyAlertDialog.setMessage(sMsg);
        MyAlertDialog.show();
    }
    
    public boolean myTouchEvent(MotionEvent event) {  
        //Log.d(TAG, "get action --- " + String.valueOf(event.getAction()));        
        //Log.d(TAG, "get X --- " + String.valueOf(event.getX()));

        //MouseDown/FingerTouch event handler
        if(event.getAction() == MotionEvent.ACTION_DOWN){
            //Memorize the x position for later reference
            fLastTouchX = event.getX();
            mTouchingScreen = true;
            Log.d(TAG, "MotionEvent.ACTION_DOWN event.getX() == " + event.getX());
            return true;
        }
        else if (event.getAction() == MotionEvent.ACTION_UP){
            mTouchingScreen = false;
            Log.d(TAG, "MotionEvent.ACTION_UP");
            return true;
        }

        //MouseMoving/FingerMoving event handler
        if((event.getAction() == MotionEvent.ACTION_MOVE) && mTouchingScreen){
            
            if((event.getX() - fLastTouchX) < 0){
                //Moving left
                if(iFocusCoverIndex < iNumberOfPosters-1){
                    if(Math.abs(event.getX() - fLastTouchX) > (mCanvasWidth/12)){
                        iFocusCoverIndex++;
                        fLastTouchX -= mCanvasWidth/12;
                    }
                }
                
            }
            else if((event.getX() - fLastTouchX) > 0){
                //Moving right
                if(iFocusCoverIndex > 0){
                    if(Math.abs(event.getX() - fLastTouchX) > (mCanvasWidth/12)){
                        iFocusCoverIndex--;
                        fLastTouchX += mCanvasWidth/12;
                    }
                }
            }
            
            //Log.d(TAG, "iFocusCoverIndex = " + iFocusCoverIndex);
            
            //Trigger the moving event
            return DrawThread.refreshCoverPosition();
        }
        
        return false;
    }  

    /**
     * Standard override to get key-press events.
     * If you want to take over the key event process, you return true. Otherwise, return false.
     */
    public boolean myKeyDown(int keyCode, KeyEvent msg) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT){
            
            if(iFocusCoverIndex == 0){
                return true;
            }
            else{
                iFocusCoverIndex--;
                return DrawThread.refreshCoverPosition();                            
            }
        }
        else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT){
            if(iFocusCoverIndex == iNumberOfPosters-1){
                return true;
            }
            else{
                iFocusCoverIndex++;
                return DrawThread.refreshCoverPosition();                            
            }
        }
        else{
            return false;
        }        
    }

    /**
     * Fetches the animation thread corresponding to this CoverFlowSurfaceView.
     * 
     * @return the animation thread
     */
    public MyDrawThread getDrawThread() {
        return DrawThread;
    }

    /**
     * Standard window-focus override. Notice focus lost so we can pause on
     * focus lost. e.g. user switches to take a call.
     */
    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        Log.d(TAG, "Inside onWindowFocusChanged!");
    }
    

    /** Callback invoked when the surface dimensions change. 
     *  This will be called once in app launch 
     */
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) {
        Log.d(TAG, "Inside surfaceChanged");
        DrawThread.setSurfaceSize(width, height);
        bSurfaceChanged = true;
    }

    /*
     * Callback invoked when the Surface has been created and is ready to be
     * used.
     */
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "Inside surfaceCreated!");
        // start the thread here so that we don't busy-wait in run()
        // waiting for the surface to be created
        DrawThread.setRunning(true);
        DrawThread.start();
        if(bThreadLoadImageEnable){
            DecThread.setRunning(true);
            DecThread.start();
        }
    }

    /*
     * Callback invoked when the Surface has been destroyed and must no longer
     * be touched. WARNING: after this method returns, the Surface/Canvas must
     * never be touched again!
     */
    public void surfaceDestroyed(SurfaceHolder holder) {
           Log.d(TAG, "Inside surfaceDestroyed!");
         // we have to tell thread to shut down & wait for it to finish, or else
        // it might touch the Surface after we return and explode
        boolean retry = true;
        while (retry) {
            try {
                DrawThread.setRunning(false);
                DrawThread.join();
                if(bThreadLoadImageEnable){
                    DecThread.setRunning(false);
                    DecThread.join();
                }
                retry = false;
            } catch (InterruptedException e) {
            }
        }
    }
}



