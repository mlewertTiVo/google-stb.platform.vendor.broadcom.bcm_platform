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
 *****************************************************************************/

package com.broadcom.epgbrowser;

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
import java.io.File;
import java.util.Properties;

public class FakeMediaPlayer extends Activity
{
    private static final String TAG = "FakeMediaPlayer";    
    private static VideoView mVideoView;
    private static MediaController mMediaController;
	private static boolean bKeyProcessed = false;
    
    @Override
    public void onCreate(Bundle savedInstanceState) 
	{
		Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);

        // turn off the window's title bar, must be called before setContentView
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        //Avoid screen locked during video playback
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.player);

        // Setup the media controller
        mVideoView = (VideoView) findViewById(R.id.VideoView);
        mMediaController = new MediaController(this);
        mMediaController.setAnchorView(mVideoView);

		String video_url = getIntent().getStringExtra("URL From Trellis");
		Log.e(TAG,"onCreate: video_url = " +video_url);

        mVideoView.setMediaController(mMediaController);
		bKeyProcessed = false;
    }

	@Override
    public boolean onKeyDown(int keyCode, KeyEvent msg)
	{
		super.onKeyDown(keyCode, msg);
		Log.d(TAG, "onKeyDown (keyCode = " +keyCode +")");

		if ((keyCode == KeyEvent.KEYCODE_GUIDE) || (keyCode == KeyEvent.KEYCODE_INFO))
		{
			// Close the fake activity  
			finish();

			Log.d(TAG, "onKeyDown (Dispatching the event back to the main activity)");

			bKeyProcessed = true;
			BcmEPGBrowser.ke = msg;
			BcmEPGBrowser.bDispatched = true;

			return true;
        }

        else if ((keyCode == KeyEvent.KEYCODE_ESCAPE) || (keyCode == KeyEvent.KEYCODE_BACK))
		{
			bKeyProcessed = true;

			// Close the fake activity  
			finish();

			return true;
        }

	    else
		{
            Log.d(TAG, "Unhandled KeyCode: " +keyCode);

			bKeyProcessed = false;
            return false;
        }
    }

	@Override
	protected void onResume() 
	{
		super.onResume();

		Log.d(TAG, "onResume");
		mVideoView.resume();
	}

	@Override
	protected void onPause() 
	{
		super.onPause();

		Log.d(TAG, "onPause, bKeyProcessed = " +bKeyProcessed);

		// Close up
		finish();

		if (!bKeyProcessed)
		{
			Log.d(TAG, "onPause, Initiating close");
			BcmEPGBrowser.bClose = true;
		}

		mVideoView.suspend();
	}

	@Override
	protected void onDestroy() 
	{
		super.onDestroy();

		Log.d(TAG, "onDestroy");
		mVideoView.stopPlayback();
	}
}