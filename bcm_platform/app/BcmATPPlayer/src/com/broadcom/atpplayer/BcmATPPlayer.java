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
 *****************************************************************************/
package com.broadcom.atpplayer;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import android.net.Uri;
import android.text.Editable;
import android.widget.TextView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.content.Intent;
import android.content.SharedPreferences;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.view.WindowManager;
import android.widget.MediaController;
import android.widget.VideoView;
import android.view.KeyEvent;
import android.os.Bundle;
import android.app.ActivityManager;
import java.util.List;
import android.app.ActivityManager.RunningTaskInfo;
import android.content.Context;
import java.util.Properties;

import com.broadcom.atparchive.*;

public class BcmATPPlayer extends Activity
{
    private static final String TAG = "BcmATPPlayer";
    public static final String PREFS_NAME = "BcmATPPlayerPrefs";
    EditText uriEditText;
    BcmATPArchive mArchive;
	Intent FakeIntent;
	MediaThread mMT;
	SwitchAppThread mSAT;
	FakeMediaPlayer mFMP = new FakeMediaPlayer();

	// Playback states
	public static int STATE_INITIALIZING	= 1;
    public static int STATE_PLAYING		= 2;
    public static int STATE_EOS			= 3;
    public static int STATE_STOPPED		= 4;

	// Initialize the state machine
	public static int iState				= STATE_INITIALIZING;

    @Override
    public void onCreate(Bundle savedInstanceState) 
	{
        super.onCreate(savedInstanceState);
        TextView txtView = new TextView(this);    
        TextView uriExampleView = new TextView(this);    
        uriEditText = new EditText(this);
        Button btnView = new Button(this);
        String savedUriString;
        btnView.setText("Start");
        uriExampleView.setText("Usage:\n[Local File] file:///data/file.mp4\n[Streaming] http://IP-Address/file.mpg\n\nEnter the URL here:");
        uriExampleView.setTextSize(20);

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        savedUriString = settings.getString("SavedURI", "");
        Log.i(TAG, "Restored URI:"+savedUriString);
        uriEditText.setText(savedUriString);
        
        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.VERTICAL);

        LinearLayout.LayoutParams llp_wrap = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT, 
                        LinearLayout.LayoutParams.WRAP_CONTENT);

        ll.addView(uriExampleView, llp_wrap);
 
        ll.addView(uriEditText, llp_wrap);
        ll.addView(btnView, llp_wrap);
        
        setContentView(ll);

		// Create the JAR object
		mArchive  = new BcmATPArchive();

        btnView.setOnClickListener(new OnClickListener() 
		{
            @Override
            public void onClick(View v) 
			{
                Editable uriString = uriEditText.getText();

				// Pass down the URI string
				int iRet = mArchive.setDataSource(uriString.toString());
				Log.d(TAG, "Returned from setDataSource: iRet = " +iRet);

				// Initiate the Android video window & wait for EOS
				if (iRet == 0)
				{
					// Set to playing state
					iState = STATE_PLAYING;

					// Create a fake Android media path which
					// will take care of the full screen mode
					FakeIntent = new Intent(BcmATPPlayer.this, FakeMediaPlayer.class);

					startActivity(FakeIntent);
				}

				Log.d(TAG, "Creating MediaThread");
		        mMT = new MediaThread();

				Log.d(TAG, "Starting MediaThread");
				mMT.start();

//				Log.d(TAG, "Stopping MediaThread");
//				mMT.setStopThread(true);

				Log.d(TAG, "Creating SwitchAppThread");
		        mSAT = new SwitchAppThread();

				Log.d(TAG, "Starting SwitchAppThread");
				mSAT.start();
			}
		});        
    }

    @Override
    protected void onPause() 
	{
        super.onPause();
        Log.d(TAG, "onPause...");

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString("SavedURI", uriEditText.getText().toString());
        editor.commit();
        Log.d(TAG, "Saved URI:"+uriEditText.getText().toString());
        
        finish();
    }

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);
		Log.d(TAG, "onWindowFocusChanged");

		Bundle extras = getIntent().getExtras();

		if (extras != null)
		{
			String val = extras.getString("FromFakeMediaPlayer");

			if (val.compareTo("Stop") == 0)
			{
				// Stop media playback
				Log.d(TAG, "Stopping playback");
				int iRet = mArchive.stop();
				Log.d(TAG, "Returned from stop: iRet = " +iRet);

				// Set to stopped state
				iState = STATE_STOPPED;

				// Now, clear the string
				getIntent().removeExtra("FromFakeMediaPlayer");
			}
		}
	}

	// Waits for EOS on the JNI library
    public class MediaThread extends Thread
	{
		private volatile boolean stopThread;

		public MediaThread()
        {
            Log.e(TAG, "MediaThread:Enter...");

			// Reset the flag
			stopThread = false;
		}

        public void run()
        {
			int iVal, TaskId = -1;
			boolean bPushNeeded = false;

            Log.e(TAG, "MediaThread:run, Enter");

			// This is a blocking call (as long as media playback is in progress)
			iVal = mArchive.getPlaybackStatus();
			Log.d(TAG, "MediaThread: Returned from getPlaybackStatus, iRet = " +iVal);

			if (iVal == 0)
			{
				// Set to EOS state
				iState = STATE_EOS;

				ActivityManager activityManager = (ActivityManager)getSystemService (Context.ACTIVITY_SERVICE); 
				List<RunningTaskInfo> services = activityManager.getRunningTasks(Integer.MAX_VALUE); 

				for (int i = 0; i < services.size(); i++) 
				{
					if (bPushNeeded == true)
						break;

					TaskId = services.get(i).id;
//					Log.e(TAG, "Activity_Name (b4) = " +services.get(i).topActivity.toString() + ", id = " +TaskId);

					if ((i == 0) && (services.get(i).topActivity.toString().equalsIgnoreCase("ComponentInfo{com.broadcom.atpplayer/com.broadcom.atpplayer.FakeMediaPlayer}")))
						bPushNeeded = true;
				}

				// We need to push the FakeMediaPlayer activity to back. To achive that, we push the 2nd activity to front
				if (TaskId != -1)
				{
					Log.e(TAG, "Push this task to front: " +TaskId);
					activityManager.moveTaskToFront(TaskId, ActivityManager.MOVE_TASK_WITH_HOME);
				}

				Log.d(TAG, "MediaThread: EOS is handled, Launching BcmATPPlayer main UI");
				Intent myIntent = new Intent(BcmATPPlayer.this, BcmATPPlayer.class);

				myIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
				startActivity(myIntent);
			}

			Log.e(TAG, "MediaThread:run, Exit");
		}

        public void setStopThread(boolean value)
        {
			Log.e(TAG, "MediaThread:setStopThread, Enter");
            stopThread = value;
            Thread.currentThread().interrupt();
        }
	}

	// Waits for Alt+Tab notification & issues a stop
    public class SwitchAppThread extends Thread
	{
		private volatile boolean stopThread;

		public SwitchAppThread()
        {
            Log.e(TAG, "SwitchAppThread:Enter...");

			// Reset the flag
			stopThread = false;
		}

        public void run()
        {
			String val = "0", ret = "0";

			// Loop on the alt_tab property
			while (val.compareTo("0") == 0)
			{
				ret = System.getProperty ("ATPPlayer.alt_tab");

				if (ret != null)
					val = ret;
			}

			Log.d(TAG, "SwitchAppThread:run, ATPPlayer.alt_tab = " +ret);
			Log.d(TAG, "SwitchAppThread:run, iState = " +iState);

			// Stop media playback (only if we haven't already stopped it)
			if ((val.compareTo("1") == 0) && (iState != STATE_STOPPED))
			{
				Log.d(TAG, "SwitchAppThread:run, Stopping playback");
				int iRet = mArchive.stop();

				// Set to stopped state
				iState = STATE_STOPPED;

				Log.d(TAG, "SwitchAppThread:run, returned from stop: iRet = " +iRet);
			}

                	// Reset the property to avoid reentrancy
                	System.setProperty ("ATPPlayer.alt_tab", "0");

			Log.e(TAG, "SwitchAppThread:run, Exit");
		}

        public void setStopThread(boolean value)
        {
			Log.e(TAG, "SwitchAppThread:setStopThread, Enter");
            stopThread = value;
            Thread.currentThread().interrupt();
        }
	}
}
