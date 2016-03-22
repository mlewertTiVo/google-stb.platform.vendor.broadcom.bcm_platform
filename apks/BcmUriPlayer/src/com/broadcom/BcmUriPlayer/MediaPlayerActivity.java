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
 * $brcm_Workfile: MediaPlayerActivity $
 * $brcm_Revision:  $
 * $brcm_Date:  $
 *
 * Module Description:
 *
 * Revision History:
 * 
 * 1   11/22/10 10:48p franktcc
 * - Initial version
 *
 *****************************************************************************/

package com.broadcom.BcmUriPlayer;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Window;
import android.view.WindowManager;
import android.widget.MediaController;
import android.widget.VideoView;
import android.view.Menu;
import android.view.MenuItem;
import android.media.MediaPlayer;


public class MediaPlayerActivity extends Activity {
       /** Tag for log */
    private static final String TAG = "MediaPlayerActivity";
    
    private static VideoView mVideoView;
    private static MediaController mMediaController;
    
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // turn off the window's title bar, must be called before setContentView
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        /* Obsolete flag used for GB/HC to work correctly, but it's causing memory leaking in 7425B1 ICS
        //set fullscreen flag to window
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, 
        WindowManager.LayoutParams.FLAG_FULLSCREEN);

        //Avoid little bar on top of screen, visible while over-scan resize enabled.
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_SCALED, 
        WindowManager.LayoutParams.FLAG_SCALED);
        */
        //Avoid screen locked during video playback
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.player);

    }

    @Override
    public void onStart() {
        super.onStart();
        Log.d(TAG, "onStart...");
        Bundle extras = getIntent().getExtras();
        String sCoverUri = extras.getString("CoverURI");
        
        if(extras !=null)
        {
            Log.d(TAG, "Cover's URI = "+ sCoverUri);
        }
        
        //Start a playback from an URI
        mVideoView = (VideoView) findViewById(R.id.VideoView);
        mMediaController = new MediaController(this);
        mMediaController.setAnchorView(mVideoView);
        //Change the URI here if you want to test some other format
        Uri video = Uri.parse(sCoverUri);
        mVideoView.setMediaController(mMediaController);
        mVideoView.setVideoURI(video);
        mVideoView.requestFocus();
        mVideoView.setOnPreparedListener(new MediaPlayer.OnPreparedListener()
        {
            private boolean canEnableLoopback(String Uri)
            {
                String rtspUri = new String("rtsp");
                String udpUri = new String("udp");
                String rtpUri = new String("rtp");
                String httpUri = new String("http");

                if( Uri.regionMatches (true, 0, rtspUri, 0, rtspUri.length()) || 
                    Uri.regionMatches (true, 0, udpUri, 0, udpUri.length()) ||
                    Uri.regionMatches (true, 0, rtpUri, 0, rtpUri.length()) ||
                    Uri.regionMatches (true, 0, httpUri, 0, httpUri.length()) )
                {
                    Log.d(TAG,"Streaming Protocol Identified- Disable Looping");
                    return false;
                }

                return true;
            }

            @Override
            public void onPrepared(MediaPlayer m)
            {
                Log.d(TAG, "VideoView Media Player Ready For Playback");
                try {

                    if (m.isPlaying()) 
                    {
                       Log.d(TAG, "isPlayeing Is Set: Stopping/Releasing and Creating New Media Player");
                        m.stop();
                        m.release();
                        m = new MediaPlayer();
                    }

                    Bundle extras = getIntent().getExtras();
                    String sCoverUri = extras.getString("CoverURI");

                    if(canEnableLoopback(sCoverUri))
                    {
                        Log.d(TAG, "Enabling Looping Now");
                        m.setLooping(true);
                    }else{
                        Log.d(TAG, "Looping Not Enabled");
                    }
                    m.start();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }
    
    @Override
    public void onPause() {
        super.onPause();
        Log.d(TAG, "onPause...");
        mVideoView.pause();
        mVideoView.suspend();
        mMediaController.hide();        
        finish();
        //System.exit(0);
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent msg) {

        if (keyCode == KeyEvent.KEYCODE_BACK){
                        
            mVideoView.pause();
            mVideoView.suspend();
            mMediaController.hide();
            mMediaController.setEnabled(false);
            
            //Go back to cover flow activity
            Log.d(TAG, "KEYCODE_BACK");
            Intent myIntent = new Intent(this, BcmUriPlayer.class);
            startActivity(myIntent);
            
            finish();
            //System.exit(0);
            return true;
        }
        else{
            Log.d(TAG, "Hit Unhandled KeyCode --- " + keyCode);
            return false;
        }
    }

    /* Creates the menu items */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
     boolean result = super.onCreateOptionsMenu(menu);

        menu.add(0, 1, 0, "Zoom");
        menu.add(0, 2, 0, "Box");
        menu.add(0, 3, 0, "Pan Scan");
        menu.add(0, 4, 0, "Fullscreen");
        
        return result;
    }

    /* Handles item selections */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {

        switch (item.getItemId()) {
        case 1:
            //mVideoView.setAspectRatio(0);            
            break;
        case 2:
            //mVideoView.setAspectRatio(1);
            break;
        case 3:
            //mVideoView.setAspectRatio(2);
            break;
        case 4:
            //mVideoView.setAspectRatio(3);
            break;

        default:
            break;
        }  

        return true;

    }
}
