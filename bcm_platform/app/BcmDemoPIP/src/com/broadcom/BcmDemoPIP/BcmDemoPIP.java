/******************************************************************************
 *    (c)2009-2011 Broadcom Corporation
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
 * $brcm_Workfile: BcmDemoPIP.java $
 * $brcm_Revision:  $
 * $brcm_Date:  $
 *
 * Module Description:
 *
 * Revision History:
 * 
 *
 *****************************************************************************/
package com.broadcom.BcmDemoPIP;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import android.media.MediaPlayer;
import android.widget.MediaController;
import android.widget.VideoView;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.widget.RelativeLayout;
import android.util.DisplayMetrics;

import android.view.KeyEvent;
import android.view.Window;
import android.view.WindowManager;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class BcmDemoPIP extends Activity {
    RelativeLayout ll;
    private VideoView vv1, vv2;
    private MediaController mediaController, mediaController2;
    RelativeLayout.LayoutParams lp1, lp2;
    DisplayMetrics dm;
    private final static String LOGTAG = "BcmDemoPIP";
    private final static String appPath = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES).getPath()+"/BcmCoverFlow/";
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);

        //Avoid screen locked during video playback
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(dm);
        Log.d("AddByLayout", "dm.widthPixels=" + dm.widthPixels + "dm.heightPixels" + dm.heightPixels);

        ll = new RelativeLayout(this);
        vv1 = new VideoView(this);
        vv2 = new VideoView(this);
        mediaController = new MediaController(this);
        mediaController2 = new MediaController(this);
        setContentView(ll);

        /** Fullscreen for video 1 */
        lp1 = new RelativeLayout.LayoutParams(
                        RelativeLayout.LayoutParams.MATCH_PARENT, 
                        RelativeLayout.LayoutParams.MATCH_PARENT);

        lp1.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        lp1.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        lp1.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
        lp1.addRule(RelativeLayout.ALIGN_PARENT_LEFT);        

        ll.addView(vv1, lp1);

        Log.w(LOGTAG, "###########Start Video 1");
        mediaController.setAnchorView(vv1);
        //Fixed Video URI for demo purpose
        //Uri videoUri = Uri.parse(Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow/elephantsdream-hd-large.mov");
        Uri videoUri = Uri.parse(appPath + "elephantsdream-hd-large.mov");
        vv1.setMediaController(mediaController);
        vv1.setVideoURI(videoUri);
        vv1.requestFocus();
        vv1.setOnPreparedListener(new MediaPlayer.OnPreparedListener()
        {
            @Override
            public void onPrepared(MediaPlayer m)
            {
                try {
                    if (m.isPlaying()) {
                        m.stop();
                        m.release();
                        m = new MediaPlayer();
                    }
                    m.setLooping(true);
                    m.start();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
        Log.i("BcmDemoPIP", "videoView1 screen size height : "+vv1.getLayoutParams().height+", width : "+vv1.getLayoutParams().width);
        
        /* delayed runnable for creating second video view to avoid race condition */
        Handler mHandler = new Handler();
        mHandler.postDelayed(new Runnable() {
            public void run()
            {
                /* 1/2 width and 1/2 height, align to upper right for video 2 */
                lp2 = new RelativeLayout.LayoutParams(
                                dm.widthPixels/2, dm.heightPixels/2);
                lp2.addRule(RelativeLayout.ALIGN_PARENT_TOP);
                lp2.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
                vv2.setZOrderOnTop(true);
                ll.addView(vv2, lp2);

                Log.w(LOGTAG, "###########Start Video 2");
                mediaController2.setAnchorView(vv2);
                //Fixed Video URI for demo purpose
                //Uri videoUri2 = Uri.parse(Environment.getExternalStorageDirectory().getPath()+"/BcmCoverFlow/BigBuckBunny_320x180.mp4");
                Uri videoUri2 = Uri.parse(appPath + "BigBuckBunny_320x180.mp4");
                vv2.setMediaController(mediaController2);
                vv2.setVideoURI(videoUri2);
                vv2.setOnPreparedListener(PreparedListener);
                Log.i("BcmDemoPIP", "videoView2 screen size height : "+vv2.getLayoutParams().height+", width : "+vv2.getLayoutParams().width);
            }
        }, 10/* wait 10ms */);
    }
    
    MediaPlayer.OnPreparedListener PreparedListener = new MediaPlayer.OnPreparedListener() {
        private Method mMediaPlayer_getTrackInfo;
        private Method mMediaPlayer_deselectTrack;
        private Method mTrackInfo_getTrackType;
        private Field  fTrackInfo_MEDIA_TRACK_TYPE_AUDIO;

        @Override
        public void onPrepared(MediaPlayer m)
        {
            try {
                if (m.isPlaying()) {
                    m.stop();
                    m.release();
                    m = new MediaPlayer();
                }

                m.setLooping(true);

                // Use Reflection in order to be able to support versions of Android before JB (API level 16) where the
                // TrackInfo class, MediaPlayer.getTrackInfo() and MediaPlayer.deselectTrack() methods are not present...
                try {
                    Class TrackInfo = Class.forName("android.media.MediaPlayer$TrackInfo");

                    mMediaPlayer_getTrackInfo = MediaPlayer.class.getMethod( "getTrackInfo", new Class[] {} );
                    mMediaPlayer_deselectTrack = MediaPlayer.class.getMethod( "deselectTrack", new Class[] { Integer.TYPE } );

                    Object[] trackInfoList = (Object[])mMediaPlayer_getTrackInfo.invoke(m, (Object[])null);  // m.getTrackInfo()

                    mTrackInfo_getTrackType = TrackInfo.getDeclaredMethod( "getTrackType", new Class[] {} );

                    fTrackInfo_MEDIA_TRACK_TYPE_AUDIO = TrackInfo.getField("MEDIA_TRACK_TYPE_AUDIO");

                    for (int i = 0; i < trackInfoList.length; i++) {
                        Object TrackType = mTrackInfo_getTrackType.invoke(trackInfoList[i], (Object[])null); // trackInfoList[i].getTrackType()
                        if (TrackType.equals(fTrackInfo_MEDIA_TRACK_TYPE_AUDIO.get(TrackType))) {            // MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO
                            Log.d(LOGTAG, "Deselecting audio track " + i + "...");
                            mMediaPlayer_deselectTrack.invoke(m, i);                                         // m.deselectTrack(i)
                            break;
                        }
                   }
                } catch (ClassNotFoundException e) {
                    Log.e(LOGTAG, "Class not found exception raised and caught!");
                } catch (NoSuchMethodException e) {
                    Log.e(LOGTAG, "No such method exception raised and caught!");
                } catch (NoSuchFieldException e) {
                    Log.e(LOGTAG, "No such field exception raised and caught!");
                } catch (Exception e) {
                    Log.e(LOGTAG, "Unknown exception raised and caught!");
                    e.printStackTrace();
                }
                m.start();
                
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    };

    public boolean dispatchKeyEvent(KeyEvent paramKeyEvent)
    {
        if (paramKeyEvent.getKeyCode() == KeyEvent.KEYCODE_BACK)
            finish();

        return super.dispatchKeyEvent(paramKeyEvent);
    }
   
    public boolean onKeyDown(int keyCode, KeyEvent paramKeyEvent)
    {
        boolean eventHandled = false;

        if (keyCode == KeyEvent.KEYCODE_0)
        {
            Log.w("BcmDemoPIP", " KEYCODE_0 ");
            VideoView localVideoView1 = this.vv1;
            int visibility = this.vv1.getVisibility();

            if (visibility != VideoView.VISIBLE)
            {
                localVideoView1.setVisibility(VideoView.VISIBLE);
                if (!this.vv1.isPlaying())
                    this.vv1.start();
            }
            else
            {
                localVideoView1.setVisibility(VideoView.INVISIBLE);
            }
            eventHandled = true;
        }
        else if (keyCode == KeyEvent.KEYCODE_1)
        {
            Log.w("BcmDemoPIP", " KEYCODE_1 ");
            VideoView localVideoView2 = this.vv2;
            int visibility = this.vv2.getVisibility();

            if (visibility != VideoView.VISIBLE)
            {
                localVideoView2.setVisibility(VideoView.VISIBLE);
                if (!this.vv2.isPlaying())
                    this.vv2.start();
            }
            else
            {
                localVideoView2.setVisibility(VideoView.INVISIBLE);
            }
            eventHandled = true;
        }
        else if (keyCode == KeyEvent.KEYCODE_2)
        {
            Log.w("BcmDemoPIP", " KEYCODE_2 ");
            Log.w("BcmDemoPIP", "Bring video view 1 to front...");
            this.vv1.setVisibility(VideoView.INVISIBLE);
            this.vv1.setVisibility(VideoView.VISIBLE);

            if (!this.vv1.isPlaying())
                this.vv1.start();

            //this.ll.bringChildToFront(this.vv1);
            //this.vv1.postInvalidate();
            eventHandled = true;
        }
        else if (keyCode == KeyEvent.KEYCODE_3)
        {
            Log.w("BcmDemoPIP", " KEYCODE_3 ");
            Log.w("BcmDemoPIP", "Bring video view 2 to front...");
            this.vv2.setVisibility(VideoView.INVISIBLE);
            this.vv2.setVisibility(VideoView.VISIBLE);

            if (!this.vv2.isPlaying())
                this.vv2.start();

            //this.ll.bringChildToFront(this.vv2);
            //this.vv2.postInvalidate();
            eventHandled = true;
        }
        return eventHandled;
    }

    protected void onPause() {
        super.onPause();

        /** TODO: App state control for resume */
        Log.w(LOGTAG, "###########release Video 1");
        vv1.stopPlayback();            
        Log.w(LOGTAG, "###########release Video 2");                    
        vv2.stopPlayback();
        Log.w(LOGTAG, "Activity Finish!");
        vv2.requestFocus();
        finish();
    }
}

