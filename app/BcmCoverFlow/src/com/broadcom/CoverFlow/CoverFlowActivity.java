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
 * $brcm_Workfile: CoverFlowActivity.java $
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

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.FrameLayout;

public class CoverFlowActivity extends Activity {

       /** Tag for log */
    private static final String TAG = "CoverFlowActivity";

    /** Using mCoverFlowSurfaceView is deprecated, it can't be run in ics 4.0.4 or crashing once playing video
     *   Note that if you switch below you will also need to switch the instantiation code in onCreate()
     */
    private CoverFlowView mCoverFlowView;
    //private CoverFlowSurfaceView mCoverFlowView;
    
    private float fLastTouchX;

    private  FrameLayout fl;
     FrameLayout.LayoutParams flp_parent;
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.v(TAG,"onCreate");

        fl = new FrameLayout(this);

        flp_parent = new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT, 
                        FrameLayout.LayoutParams.MATCH_PARENT);        
        fl.setLayoutParams(flp_parent);
        
        // Link View with layout res
        setContentView(fl);

        mCoverFlowView = new CoverFlowView(this, null);
        //mCoverFlowView = new CoverFlowSurfaceView(this, null);

        fl.addView(mCoverFlowView, flp_parent);
    }

    @Override 
    public void onDestroy(){
        super.onDestroy();
        fl.removeView(mCoverFlowView);
        mCoverFlowView = null;
        flp_parent = null;
        fl = null;
        Log.d(TAG, "onDestroy");
    }

    @Override
    public void onPause() {
        super.onPause();
        Log.d(TAG, "onPause");
    }

    @Override  
    public boolean onTouchEvent(MotionEvent event) {
        int iAction = event.getAction();
        
        //Log.d(TAG, "get action --- " + String.valueOf(iAction));        
        //Log.d(TAG, "get X --- " + String.valueOf(event.getX()));

        //MouseDown/FingerTouch event handler
        if(iAction == MotionEvent.ACTION_DOWN){
            //Memorize the x position for later reference
            Log.d(TAG, "ACTION_DOWN!!");
            fLastTouchX = event.getX();
            
            return mCoverFlowView.myTouchEvent(event);
        }

        if(iAction == MotionEvent.ACTION_UP){
            
            //Log.d(TAG, "ACTION_UP!!");
            if(fLastTouchX == event.getX()){

                Log.d(TAG, "Touch to start media playback !!");

                if(null == mCoverFlowView.getCoverURI()){
                    Log.e(TAG, "No movie matched for the selected poster");
                    return true;
                }

                //Start media player activity
                Intent myIntent = new Intent(this, MediaPlayerActivity.class);
                
                //Get URI for media player from CoverFlowView
                myIntent.putExtra("CoverURI", mCoverFlowView.getCoverURI());
                myIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
               
                startActivityForResult(myIntent, 0);            

                return true;
            }
            else{
                return mCoverFlowView.myTouchEvent(event);
            }
        }
        
        return mCoverFlowView.myTouchEvent(event);
    }  
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent msg) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT){
            Log.d(TAG, "KEYCODE_DPAD_LEFT");
            return mCoverFlowView.myKeyDown(keyCode, msg);
        }
        else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT){
            Log.d(TAG, "KEYCODE_DPAD_RIGHT");
            return mCoverFlowView.myKeyDown(keyCode, msg);
        } 
        else if (keyCode == KeyEvent.KEYCODE_F){
            Log.d(TAG, "KEYCODE_F");
            return mCoverFlowView.myKeyDown(keyCode, msg);
        } 
        else if (keyCode == KeyEvent.KEYCODE_ENTER){
            Log.d(TAG, "KEYCODE_ENTER or KEYCODE_BUTTON_SELECT received");
            
            //Start media player activity
            Intent myIntent = new Intent(this, MediaPlayerActivity.class);
            
            //Get URI for media player from CoverFlowView
            myIntent.putExtra("CoverURI", mCoverFlowView.getCoverURI());
            myIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
            startActivityForResult(myIntent, 0);            
            
            /** Test code for 2 views in a activity
            mVideoView.bringToFront(); 
            mVideoView.setZOrderOnTop(true);
            //mCoverFlowView.setEnabled(false);
            
            MediaController mediaController = new MediaController(this);
            mediaController.setAnchorView(mVideoView);
            // Set video link (mp4 format )
            Uri video = Uri.parse("http://www.pocketjourney.com/downloads/pj/video/famous.3gp");
            mVideoView.setMediaController(mediaController);
            mVideoView.setVideoURI(video);
            mVideoView.requestFocus();
            mVideoView.start();
            */

            return true;
        }
        else{
            /* test code for memory heap
            ActivityManager actvityManager = (ActivityManager) this.getSystemService(ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo mInfo = new ActivityManager.MemoryInfo ();
            actvityManager.getMemoryInfo( mInfo );
            
            Log.d(TAG, "mem_info.availMem = " + mInfo.availMem);

            Log.d(TAG, "Hit Unhandled KeyCode --- " + keyCode);
            */
            return super.onKeyDown(keyCode, msg);
        }        

    }
    
    
    /**
     * Notification that something is about to happen, to give the Activity a
     * chance to save state.
     * 
     * @param outState a Bundle into which this Activity should save its state
     */
    @Override
    protected void onSaveInstanceState(Bundle outState) {
        // just have the View's thread save its state into our Bundle
        super.onSaveInstanceState(outState);
    }    
}
