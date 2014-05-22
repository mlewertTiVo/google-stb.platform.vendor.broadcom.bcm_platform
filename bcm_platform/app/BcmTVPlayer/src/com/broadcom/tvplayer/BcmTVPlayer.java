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
package com.broadcom.tvplayer;

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

import android.widget.RelativeLayout;
import android.util.DisplayMetrics;
import java.util.ArrayList;
import android.app.ListActivity;
import android.widget.BaseAdapter;
import android.widget.ListAdapter;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.Toast;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView.OnItemClickListener;
import android.content.Context;
import java.lang.Object;
import android.os.AsyncTask;
import android.app.AlertDialog;
import java.util.Properties;
import android.graphics.Color;
import android.os.Handler;
import android.os.Message;
import java.util.Timer;
import java.util.TimerTask;
import java.io.File;
import java.io.FileWriter;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.FileNotFoundException;
import android.graphics.Typeface;
import android.view.Gravity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;

import com.broadcom.tvarchive.*;

public class BcmTVPlayer extends Activity {
    private static final String TAG = "BcmTVPlayer";
    public static BcmTVArchive TV_Arc;
    int iRet;
    public static String[] ChannelList, PrimedList;
	TVThread mThread;
	String val;
	public static String channel_file = "/data/data/com.broadcom.tvplayer/channel.cfg";
	Bundle extras;
    RelativeLayout rl;
    private VideoView vv_main, vv_pip;
    RelativeLayout.LayoutParams lp_main, lp_pip;
    DisplayMetrics dm;
	public  static ArrayAdapter ChannelAdapter;
	public  static ListView lv;
	public  static boolean bInFocus = false, bPIPInPlay = false, bFCCMode = true, bKeepAlive = true;
	public  static boolean bFirst = true, bUsingArtemis = false, bForcedScan = false;
	public	static int x_pip, y_pip, w_pip, h_pip;
	public	static int i_CurChannel, iPrimeTo = -1;
	File f_svc;

	// Playback states
	public static int STATE_INITIALIZING	= 1;
    public static int STATE_PLAYING			= 2;
    public static int STATE_EOS				= 3;
    public static int STATE_STOPPED			= 4;
	public static int STATE_GUIDE			= 5;

	// Initialize the state machine
	public static int iState = STATE_INITIALIZING;
			 
    @Override
    public void onCreate(Bundle savedInstanceState) 
	{
		Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);

		// Don't need a title
		this.requestWindowFeature(Window.FEATURE_NO_TITLE);

		// Check if Artemis is running
		f_svc = new File("/mnt/romfs/usr/local/bin/trellis/media_artemistvservice");

		if (f_svc.exists() == true)
		{
			bUsingArtemis = true;
			bFCCMode = false;
			Log.d(TAG, "onCreate: Detected Artemis, disabling FCC mode...");
		}

		// Get the default display settings
        dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(dm);

		// Create the app thread
		Log.d(TAG, "Creating TVThread");
		mThread = new TVThread();
					
		// Kick it off now
		Log.d(TAG, "Starting TVThread");
		mThread.start();

		// Setup the layout and various views
        rl = new RelativeLayout(this);
        vv_main = new VideoView(this);
        vv_pip = new VideoView(this);
		lv = new ListView(this);

		// Layout params for the main video
        lp_main = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.FILL_PARENT, RelativeLayout.LayoutParams.FILL_PARENT);

        lp_main.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        lp_main.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        lp_main.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
        lp_main.addRule(RelativeLayout.ALIGN_PARENT_LEFT);        

		// Calculate PIP values
		w_pip = dm.widthPixels/4;
		h_pip = dm.heightPixels/4;
		x_pip = dm.widthPixels - w_pip;
		y_pip = 0;

		// Layout params for the pip video
		lp_pip = new RelativeLayout.LayoutParams(w_pip, h_pip);
		lp_pip.addRule(RelativeLayout.ALIGN_PARENT_TOP);
		lp_pip.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
		vv_pip.setZOrderOnTop(true);

		// Disable visibility by default
		vv_pip.setVisibility(View.INVISIBLE);

		// Now, add all the views to the corresponding layouts
        rl.addView(vv_main, lp_main);
		rl.addView(lv, lp_main);
		rl.addView(vv_pip, lp_pip);

		// Create the JAR object
		TV_Arc = new BcmTVArchive();

		// Get the updated list of channels and initiate playback
		UpdateAndStartPlayback();

		// Setup all the widgets
        setContentView(rl);
    }

	public void UpdateAndStartPlayback()
	{
		// Get the channel list
		ChannelList = TV_Arc.getChannelList(GetSavedChannel());
		Log.d(TAG, "Returned from getChannelList()");

		// Make sure we received a valid list
		if (ChannelList != null)
		{
			// Start playback on the default channel only on startup
			ShowMessage("Please wait while we look for available TV channels...", Color.WHITE);

			// Start playback (for the 1st time)
			bFirst = true;
			InitiatePlayback_Main(GetSavedChannel());
			bFirst = false;
		}

		else
		{
			Log.d(TAG, "Failed to retrieve any channels...");

			// Display a failure message & quit
			ShowMessage("Sorry, there are no channels available at this time", Color.RED);
			finish();
		}

		if (ChannelList != null)
		{
			// Set up the adapter
			ChannelAdapter = new ArrayAdapter<String>(this, R.layout.main, R.id.cl, ChannelList);

			// Setup the ListView params
			lv.setAdapter(ChannelAdapter);
			lv.setItemsCanFocus(true);
			lv.setFocusableInTouchMode(true);
			lv.setEnabled(true);
			lv.setTextFilterEnabled(true);
			lv.setCacheColorHint(0);
			lv.setDivider(null);
			lv.setSelector(android.R.color.holo_orange_dark);
//			lv.setDividerHeight(20);

			// Disable visibility by default
			lv.setVisibility(View.INVISIBLE);

			// Enable the item selection handler (to handle the appropriate PIP video)
			lv.setOnItemSelectedListener(new OnItemSelectedListener() 
			{
				@Override
				public void onItemSelected(AdapterView<?> parent, View v, int position, long id) 
				{
					Log.d(TAG, "onItemSelected: bInFocus = " +bInFocus);
					if (bInFocus == true)
					{
						Log.d(TAG, "onItemSelected: position = " +position);
//						Toast.makeText(getApplicationContext(), "onItemSelected, position = " +position, Toast.LENGTH_SHORT).show();

						// Enable selection for the ListView
						lv.setSelected(true);

						// Enable visibility for the PIP window
						vv_pip.setVisibility(View.VISIBLE);
						vv_pip.setZOrderOnTop(true);

						// If we're already playing in PIP mode, first 
						// stop it before issuing a replay
						if (bPIPInPlay == true)
							StopPlayback_PIP();

						// Start PIP playback on the new position
						InitiatePlayback_PIP(position);
					}
				} 

				@Override 
				public void onNothingSelected(AdapterView<?>  arg0) 
				{
				} 
			});

			// Enable the item click handler (to handle the appropriate full screen video)
			lv.setOnItemClickListener(new OnItemClickListener()
			{
				@Override
				public void onItemClick(AdapterView<?> parent, View view, int position, long id) 
				{
					Log.d(TAG, "onItemClick: position = " +position);
//					Toast.makeText(getApplicationContext(), "onItemClick", Toast.LENGTH_SHORT).show();

					if (iState != STATE_STOPPED)
					{
						Log.d(TAG, "onItemClick, Stopping playback");
						StopPlayback_Main();
					}

					// Disable visibility for the PIP video
					StopPlayback_PIP();
					vv_pip.setVisibility(View.INVISIBLE);

					// Re-start playback on the new channel
					InitiatePlayback_Main(position);

					// Disable visibility on the ListView
					lv.setVisibility(View.INVISIBLE);

					// Reset the flag
					bInFocus = false;
				}
			});
		}
	}

    Handler mHandler = new Handler(new Handler.Callback()
    {
        @Override
        public boolean handleMessage(Message msg)
        {
			if (msg.what == STATE_GUIDE)
			{
			}

			return true;
		}
	});

	public static int GetSavedChannel()
	{
		int val = 0;

		StringBuilder sb = new StringBuilder();
        String line;
        BufferedReader br = null;
		FileReader f_reader;
		boolean bFound = false;

		try
		{
			f_reader = new FileReader(new File(channel_file));
			br = new BufferedReader(f_reader);

			while ((line = br.readLine()) != null)
			{
				bFound = true;
				sb.append(line);
			}

			f_reader.close();
		}
		catch (FileNotFoundException e) 
		{
			Log.d(TAG, "GetSavedChannel: BufferedReader failed!!");
        }
		catch(IOException r)
		{
			Log.d(TAG, "GetSavedChannel: File read failed!!");
		}

		// Convert to int & return
		if (bFound == true)
		{
			val = Integer.parseInt(sb.toString());
			Log.d(TAG, "GetSavedChannel (succeeded), val = " +val);
		}

		return val;
	}

	public static void SetSavedChannel(int pos)
	{
		String ch_num = new Integer(pos).toString();

		try
		{
			FileWriter f_writer = new FileWriter(new File(channel_file));
			
			f_writer.write(ch_num);
			Log.d(TAG, "SetSavedChannel, saved channel #" +ch_num);

			f_writer.close();
		}
		catch(IOException r)
		{
			Log.d(TAG, "SetSavedChannel: File write failed!!");
		}
	}

	public static void ClearSavedChannel()
    {
		try
		{
			FileWriter f_writer = new FileWriter(new File(channel_file));

			f_writer.close();
		}
		catch(IOException r)
		{
			Log.d(TAG, "ClearSavedChannel: File write failed!!");
		}
    }

	public int GetChannelCount()
	{
		return lv.getCount();
	}

	public static void InitiatePlayback_Main(int pos)
	{
		boolean bRet;

		// Set the mode
		Log.d(TAG, "InitiatePlayback_Main, Setting FCC-mode to " +bFCCMode);
		TV_Arc.setMode(bFCCMode);

		Log.d(TAG, "InitiatePlayback_Main, Calling setChannel");
		bRet = TV_Arc.setChannel(pos);
		Log.d(TAG, "Returned from setChannel: bRet = " +bRet);

		// Update the current channel
		i_CurChannel = pos;

		// Set to playing state
		iState = STATE_PLAYING;

		// Initiate a re-prime (only if we didn't reprime internally)
		if ((!bFirst) && (bRet == true))
		{
			iPrimeTo = pos;
			Log.d(TAG, "InitiatePlayback_Main, iPrimeTo = " +iPrimeTo);
		}
	}

	public static void StopPlayback_Main()
	{
		Log.d(TAG, "StopPlayback_Main, Calling stop");
		TV_Arc.stop();

		// Set to stopped state
		iState = STATE_STOPPED;
	}

	public static void InitiatePlayback_PIP(int pos)
	{	
		Log.d(TAG, "InitiatePlayback_PIP, Calling setChannel_pip");
		TV_Arc.setChannel_pip(pos, x_pip, y_pip, w_pip, h_pip);
		bPIPInPlay = true;
	}

	public static void StopPlayback_PIP()
	{
		Log.d(TAG, "StopPlayback_Main, Calling stop_pip");
		TV_Arc.stop_pip();
		bPIPInPlay = false;
	}

	@Override
    public boolean onKeyDown(int keyCode, KeyEvent msg)
	{
		super.onKeyDown(keyCode, msg);
		Log.d(TAG, "onKeyDown (keyCode = " +keyCode +")");

		// Quitting the app
		if ((keyCode == KeyEvent.KEYCODE_HOME) || (keyCode == KeyEvent.KEYCODE_ESCAPE) ||  (keyCode == KeyEvent.KEYCODE_BACK))
		{
			Log.d(TAG, "Stopping playback on detecting Home/Last/ESC key");
			QuitApp();
		}

		// Process the 'Guide' button
		else if (keyCode == KeyEvent.KEYCODE_GUIDE)
		{
			if (lv != null)
			{
				// Check if we just need to dismiss the guide
				if (bInFocus == true)
				{
					Log.d(TAG, "onKeyDown: Disabling the channel guide");
					lv.setVisibility(View.INVISIBLE);
					bInFocus = false;

					if (bPIPInPlay == true)
						StopPlayback_PIP();
				}

				// Re-enable visibility
				else
				{
					Log.d(TAG, "onKeyDown: Launching the channel guide");
					ChannelAdapter.notifyDataSetChanged();

					// Enable the ListView and make sure it grabs the focus
					lv.setVisibility(View.VISIBLE);
					lv.requestFocus();

					// Reset the adapter so that the selection will be enabled
					lv.setAdapter(ChannelAdapter);

					// Set the flag
					bInFocus = true;
				}
			}
		}

		// Ch+/Ch-
		else if ((keyCode == KeyEvent.KEYCODE_CHANNEL_UP) || (keyCode == KeyEvent.KEYCODE_CHANNEL_DOWN))
		{
			int iMin = 0, iMax = (GetChannelCount() - 1), iNew;
			String FailMessage = "";

			// Are we going up?
			if (keyCode == KeyEvent.KEYCODE_CHANNEL_UP)
			{
				iNew = (i_CurChannel + 1);
				Log.d(TAG, "KEYCODE_CHANNEL_UP: iNew = " +iNew +", iMax = " +iMax);
			}

			// or down?
			else
			{
				iNew = (i_CurChannel - 1);
				Log.d(TAG, "KEYCODE_CHANNEL_DOWN: iNew = " +iNew +", iMax = " +iMax);
			}

			// Validate the request
			if (iNew > iMax)
			{
				iNew = 0;
				ShowMessage("Rolling back to the 1st channel", Color.MAGENTA);
			}

			else if (iNew < iMin)
			{
				iNew = iMax;
				ShowMessage("Rolling over to the last channel", Color.CYAN);
			}

			// Stop playback for the main window
			if (iState != STATE_STOPPED)
				StopPlayback_Main();

			// Start playback on the new channel
			InitiatePlayback_Main(iNew);
		}

		// Buttons 0 through 9
		else if ((keyCode >= KeyEvent.KEYCODE_0) && (keyCode <= KeyEvent.KEYCODE_9))
		{
			int iMax = (GetChannelCount() - 1), iNew = (keyCode - KeyEvent.KEYCODE_0);
			StringBuilder sb = new StringBuilder();
			String FailMessage;

			Log.d(TAG, "KEYCODE_*: iNew = " +iNew +", iMax = " +iMax);

			// Decrement the channel # since we need to follow 0-indexing
			iNew -= 1;

			if (iNew == i_CurChannel)
			{
				sb.append("You are already on channel #");
				sb.append(iNew + 1);
				sb.append("...");
				FailMessage = sb.toString();

				ShowMessage(FailMessage, Color.WHITE);
			}

			else if ((iNew >= 0) && (iNew <= iMax))
			{
				// Stop playback for the main window
				if (iState != STATE_STOPPED)
					StopPlayback_Main();

				InitiatePlayback_Main(iNew);
			}

			else
			{
				sb.append("Sorry, can't navigate to that channel. The available range is 1-");
				sb.append(GetChannelCount());
				sb.append("...");
				FailMessage = sb.toString();

				ShowMessage(FailMessage, Color.WHITE);
			}
		}

		// Red button to toggle between FCC & standard modes
		else if (keyCode == KeyEvent.KEYCODE_PROG_RED)
		{
			StringBuilder sb = new StringBuilder();
			int color;

			if (bUsingArtemis)
			{
				sb.append("FCC (Fast Channel Change) is disabled in this build!!");
				color = Color.RED;
			}

			else
			{
				// Stop playback for the main window
				if (iState != STATE_STOPPED)
					StopPlayback_Main();

				// Toggle FCC mode
				bFCCMode = !bFCCMode;

				Log.d(TAG, "KEYCODE_PROG_RED: Toggling FCC mode to " +bFCCMode);

				if (bFCCMode)
				{
					sb.append("FCC (Fast Channel Change): Turned On");
					color = Color.GREEN;
				}

				else
				{
					sb.append("FCC (Fast Channel Change): Turned Off");
					color = Color.YELLOW;
				}

				// Start playback
				InitiatePlayback_Main(i_CurChannel);
			}

			// Finally, show the message
			ShowMessage(sb.toString(), color);
		}

		// Blue button to force a re-scan
		else if (keyCode == KeyEvent.KEYCODE_PROG_BLUE)
		{
			Log.d(TAG, "BLUE_Key, Enter");

			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setCancelable(true);

			builder.setTitle("TV Player");
			builder.setInverseBackgroundForced(true);
			builder.setMessage("Ready to kick off a rescan (might take a while). Press 'Yes' to continue or 'No' to cancel");

			builder.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
			  @Override
			  public void onClick(DialogInterface dialog, int which) {
				Log.d(TAG, "BLUE_Key, Initiating rescan");

				// Stop playback for the main window
				if (iState != STATE_STOPPED)
					StopPlayback_Main();

				// Wipe out the saved channel
				ClearSavedChannel();

				// Set the flag
				bForcedScan = true;

				// Wait till we complete the rescan
				while (bForcedScan)
				{
					// Sleep while the re-scan completes
					TimedSleep(2);
				}

				// Get the updated list of channels and initiate playback
				UpdateAndStartPlayback();
			  }
			});

			builder.setNegativeButton("No", new DialogInterface.OnClickListener() {
			  @Override
			  public void onClick(DialogInterface dialog, int which) {
				dialog.dismiss();
			  }
			});

			// Show the box
			AlertDialog alert = builder.create();
			alert.show();
		}

		// Info button
		else if (keyCode == KeyEvent.KEYCODE_INFO)
		{
			StringBuilder sb = new StringBuilder();
			int color;

			if (bFCCMode)
			{
				sb.append("Currently Playing : Channel #" +(i_CurChannel + 1));
				sb.append("\n\n");
				sb.append("FCC (Fast Channel Change): Turned On");

				PrimedList = TV_Arc.getPrimedList();

				if (PrimedList != null)
				{
					int i = 0, size = PrimedList.length;

					sb.append("\n\nCurrently primed channels are:\n");

					while (i < size)
						sb.append(PrimedList[i++]);
				}

				color = Color.GREEN;
			}

			else
			{
				sb.append("Currently Playing : Channel #" +(i_CurChannel + 1));
				sb.append("\n\n");
				sb.append("FCC (Fast Channel Change): Turned Off");
				color = Color.YELLOW;
			}

			// Finally, show the message
			ShowMessage(sb.toString(), color);
		}

		return true;
	}

	public void TimedSleep(int nSec)
	{
		try
		{
			Thread.sleep(nSec * 1000);
		} 
					
		catch (InterruptedException e) 
		{
			Log.d(TAG, "Sleep failed...");
		}
	}

	public void ShowMessage(String Msg, int color)
	{
		Toast toast = Toast.makeText(getApplicationContext(), Msg, Toast.LENGTH_LONG);
		TextView v = (TextView) toast.getView().findViewById(android.R.id.message);
		v.setTextColor(color);
		v.setTypeface(null, Typeface.BOLD);
		toast.setGravity(Gravity.BOTTOM|Gravity.CENTER_HORIZONTAL, 0, 0);
		toast.show();
	}

	@Override
	protected void onStart() {
		Log.d(TAG, "onStart");
		super.onStart();
	}

    @Override
    protected void onPause() 
	{
        super.onPause();
        Log.d(TAG, "onPause");
    }

	@Override
	protected void onResume() {
		Log.d(TAG, "onResume");
		super.onResume();
	}

	@Override
	protected void onStop() {
		Log.d(TAG, "onStop");
		super.onStop();

		// Handle Apps/Home key
		Log.d(TAG, "Stopping playback on detecting Apps/Home key");
		QuitApp();
	}

	@Override
	protected void onRestart() {
		Log.d(TAG, "onRestart");
		super.onRestart();

//		System.setProperty ("TVPlayer.state", mResume);
	}

	@Override
	protected void onDestroy() {
		Log.d(TAG, "onDestroy");
		super.onDestroy();
	}

	public void QuitApp()
	{
		// Stop playback for the main window
		if (iState != STATE_STOPPED)
			StopPlayback_Main();

		// Stop playback for the pip window as well
		if (bPIPInPlay == true)
			StopPlayback_PIP();

		// Make sure the ListView selection is disabled
		bInFocus = false;

		// Save the channel position
		SetSavedChannel(i_CurChannel);

		// Closing all open handles
		TV_Arc.close();

		// Stop the thread
		Log.d(TAG, "Stopping TVThread");
		bKeepAlive = false;

		// wrap up the activity
		finish();
	}

	// Waits for notification & processes commands
	public class TVThread extends Thread
	{
		private volatile boolean bStopThread;

		public TVThread()
		{
			Log.e(TAG, "TVThread:Enter...");

			// Reset the flag
			bStopThread = false;
		}

		public void run()
		{
			// Process commands here forever
			while (bKeepAlive)
			{
				// Time to reprime?
				if (iPrimeTo != -1)
				{
					Log.d(TAG, "TVThread:run, Calling reprime...");

					// Re-arrange the prime list
					if (bFCCMode)
						TV_Arc.reprime(iPrimeTo);

					// Reset the flag
					iPrimeTo = -1;
				}

				else if (bForcedScan)
				{
					Log.d(TAG, "Please wait while we complete rescanning...");

					// Force a new scan
					TV_Arc.enforce_scan();

					// Reset the flag
					bForcedScan = false;
                }
			}

			Log.e(TAG, "TVThread:exiting run...");
		}

		public void setStopThread(boolean value)
		{
			Log.e(TAG, "TVThread:setStopThread, Enter");
			bStopThread = value;
			Thread.currentThread().interrupt();
		}
	}
}
