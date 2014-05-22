package com.broadcom.processviewer;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.widget.TextView;
import android.util.Log;
import java.io.*;
import android.view.ActionMode;
import java.util.Timer;
import java.util.TimerTask;
import android.os.Handler;
import android.text.method.ScrollingMovementMethod;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.LinearLayout.LayoutParams;
import android.view.View;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import java.util.List;
import java.util.Iterator;
import android.os.Process;

public class BcmPIDViewer extends Activity
{
    public String TAG = "BcmPIDViewer";
    LinearLayout LLSub;
    TextView TVHost, TVAndroid;
    FileInputStream fhost_stream, fandroid_stream;
    DataInputStream is_host, is_android;
    BufferedReader reader_host, reader_android;
    final String[] HostArray = {"lxc", "android", "init", "rcS", "system", "broadcom"};
    int iDelay = 60 * 1000;
    Timer PIDTimer;

    @Override
    public void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        setTitle(R.string.app_name);

        PIDTimer = new Timer();

        // Schedule the two timers to run with different delays.
        PIDTimer.schedule(new TimerTask() 
        {
			@Override
			public void run() 
            {
				PIDTimerCallback();
			}
			
		}, 0, iDelay);

        // Get UI
        TVHost      = (TextView) findViewById(R.id.TextView_1);
        TVAndroid   = (TextView) findViewById(R.id.TextView_2);

        TVHost.setMovementMethod(ScrollingMovementMethod.getInstance());
        TVAndroid.setMovementMethod(ScrollingMovementMethod.getInstance());
/*
        LLSub = (LinearLayout) findViewById(R.id.LinearLayout_Sub);

        // Create a ScrollView instance
        ScrollView mScrollView = new ScrollView(this);

        mScrollView.setScrollBarStyle(View.SCROLLBARS_OUTSIDE_INSET);
        LLSub.addView(mScrollView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        mScrollView.addView(TVMain);
*/
//        TVHost.append("\n\n" + R.string.layout_name);

        // Process the files
        FileHandler();
    }

    public void FileHandler()
    {
        try
        {
            // Create host objects
            fhost_stream    = new FileInputStream("pid_host_saved.log");
            is_host         = new DataInputStream(fhost_stream);
            reader_host     = new BufferedReader(new InputStreamReader(is_host));
     
            // Create container (Android) objects
            fandroid_stream = new FileInputStream("pid_android.log");
            is_android      = new DataInputStream(fandroid_stream);
            reader_android  = new BufferedReader(new InputStreamReader(is_android));

            String strHost, strAndroid;

            // Clear the text at the start
            TVHost.setText("");
            TVAndroid.setText("");

            // Display a heading
            TVHost.append("Host side processes (can watch everything, including LXC side)" + "\n\n");

            // Now, read the host file (line by line)
            while ((strHost = reader_host.readLine()) != null)   
            {
//                Log.e(TAG, strHost);
                for(String s : HostArray)
                {
                    if (strHost.indexOf(s) != -1)
                    {
                        TVHost.append(strHost + '\n');

                        // Don't compare any more after the 1st one succeeds
                        break;
                    }
                }
            }

            // Display a heading
            TVAndroid.append("Container (Android) side processes" + "\n\n");

/**
            // An alternate method to display processes (not the same as running 'ps')
            ActivityManager ActMgr = (ActivityManager)getSystemService(ACTIVITY_SERVICE);
            List AppList;
            Iterator AppIter;
            RunningAppProcessInfo PInfo;

            AppList = ActMgr.getRunningAppProcesses();
            AppIter = AppList.iterator();

            while (AppIter.hasNext())
            {
                PInfo = (RunningAppProcessInfo)(AppIter.next());
                TVAndroid.append(PInfo.processName + '\t' + PInfo.pid + '\t' + PInfo.uid + '\t' + PInfo.importanceReasonCode + '\n');
            }
**/
            // Now, read the container file (line by line)
            while ((strAndroid = reader_android.readLine()) != null)   
            {
//                Log.e(TAG, strAndroid);
                TVAndroid.append(strAndroid + '\n');
            }

            // Finally, close the input streams
            fhost_stream.close();
            fandroid_stream.close();
        }
        
        // Catch exceptions, if any
        catch (Exception e)
        {
            Log.e(TAG, "Error: " + e.getMessage());
        }
    }

    public void PIDTimerCallback()
    {
        // Called directly by the timer and runs in the same thread as the timer
        Log.e(TAG, "PIDTimerCallback: Enter...");

		// Call the method that will work on the UI
		this.runOnUiThread(TimerTick);
    }

    public Runnable TimerTick = new Runnable()
    {
		public void run() 
        {		
		    // Runs on the same thread as UI, make UI changes here
            FileHandler();
		}
    };
}