package com.broadcom.appmgr;

import android.app.Activity;
import android.content.Context;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Bundle;
import android.widget.TextView;
import android.util.Log;
import android.content.Intent;
import android.content.IntentFilter;
import java.io.IOException;
import java.io.InputStream;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.os.Handler;
import android.view.ActionMode;
import android.view.MenuItem;
import android.view.Menu;
import java.lang.CharSequence;
import android.os.Message;
import android.content.BroadcastReceiver;
import android.app.ActivityManager;
import java.util.List;
import android.app.ActivityManager.RunningTaskInfo;

import java.lang.Runtime;
import android.media.AudioManager;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.os.SystemClock;
import java.io.File;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.widget.Toast;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.*;
import android.content.pm.ApplicationInfo;

public class BcmAppHandler extends Activity
{
    public String TAG = "BcmAppMgr:Core";
    TextView textStatus;

    public static String SOCKET_ADDRESS = "broadcom_socket_server";
    String sLaunch_AB			= "START_AB";
    String sKill_AB				= "STOP_AB";
    String sDesktopHide			= "DESKTOP_HIDE";
    String sDesktopShow			= "DESKTOP_SHOW";
	String sDesktopShowNoApp	= "DESKTOP_SHOW_NO_APP";
    int nLaunch_AB				= 0x10;
    int nKill_AB				= 0x11;
    int nDesktopHide			= 0x12;
    int nDesktopShow			= 0x13;
	int nDesktopShowNoApp		= 0x14;

    LocSoc mLocalServerSocket;
    int iVolMusic, DummyCode = 18;
    Context DummyCon;
	Intent DummyIntent = null;
	boolean bNoApp = false;
	String sPkgName	= "", sPrefix;

    Handler mHandler = new Handler(new Handler.Callback()
    {
        @Override
        public boolean handleMessage(Message msg)
        {
//            Log.e(TAG, "handleMessage:msg.what = " +msg.what);

            if (msg.what == nLaunch_AB)
            {
                // Make sure we don't launch it if its already running
                if (isAngryBirdsRunning() == false)
                {
                    PushActivityToFront(false);

                    Intent IAB = new Intent();
                    IAB.setAction(Intent.ACTION_VIEW);

					// Since we can't guess what the package name could 
					// be, we shall get the details from the PM
					IAB = getPackageManager().getLaunchIntentForPackage(sPkgName);

					// This will fail if the className doesn't match!!
//					IAB.setClassName(sPkgName, "com.rovio.ka3d.App");

					Log.e(TAG, "handleMessage:nLaunch_AB, startActivityForResult for AngryBirds");
					startActivity (IAB);
                }

                else
                {
                    Log.e(TAG, "handleMessage:nLaunch_AB, AngryBirds already running");
                    PushActivityToFront(true);
                }
            }
 
            else if (msg.what == nKill_AB)
            {
                Log.e(TAG, "handleMessage:nKill_AB");

                StartDummyActivity();
            }

            else if (msg.what == nDesktopHide)
            {
                Log.e(TAG, "handleMessage:nDesktopHide");

                StartDummyActivity();
            }

            else if (msg.what == nDesktopShow)
            {
                Log.e(TAG, "handleMessage:nDesktopShow");

                // Push back the activity
                PushActivityToFront(false);
            }

            else if (msg.what == nDesktopShowNoApp)
            {
                Log.e(TAG, "handleMessage:nDesktopShowNoApp");

				bNoApp = true;

                // Push back the activity
                PushActivityToFront(false);
            }

            return true;
        }
    });

    // Called when the activity is first created
    @Override
    public void onCreate(Bundle savedInstanceState) 
    {
        Log.e(TAG, "onCreate:Enter...");
        super.onCreate(savedInstanceState);
        
	    // Don't set the view since we don't need a layout. This 
	    // needs to just run the in background & accept commands
//        setContentView(R.layout.main);

        // Get UI
        textStatus = (TextView) findViewById(R.id.textStatus);
//        textStatus.append("\n\n" + "ServerSocket");

        mLocalServerSocket = new LocSoc();
        mLocalServerSocket.start();
    }

    @Override
    protected void onPause()
    {
        Log.e(TAG, "onPause:Enter");
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        Log.e(TAG, "onResume:Enter");
        super.onResume();
    }

    @Override
    protected void onStop()
    {
        Log.e(TAG, "onStop:Enter");
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        Log.e(TAG, "onDestroy:Enter");
        super.onDestroy();
    }

    @Override
    public boolean isFinishing ()
    {
        Log.e(TAG, "isFinishing:Enter");
        return true;
    }

    private boolean isAngryBirdsRunning()
    {	
        ActivityManager activityManager = (ActivityManager)getSystemService (Context.ACTIVITY_SERVICE); 
        List<RunningTaskInfo> services = activityManager.getRunningTasks(Integer.MAX_VALUE); 
        boolean iFound = false; 

        for (int i = 0; i < services.size(); i++) 
        { 
//            Log.e(TAG, "isAngryBirdsRunning:Name = " +services.get(i).topActivity.toString() + ", id = " +services.get(i).id);
            if (services.get(i).topActivity.toString().startsWith(sPrefix)) 
            {
//				Log.e(TAG, "isAngryBirdsRunning: Found AngryBirds!!");
                iFound = true;
            }
        }

        return iFound; 
    }

    private void PushActivityToFront(boolean bPushAB)
    {
        ActivityManager activityManager = (ActivityManager)getSystemService (Context.ACTIVITY_SERVICE); 
        List<RunningTaskInfo> services = activityManager.getRunningTasks(Integer.MAX_VALUE); 
        int TaskId = -1;

		// Finish up this dummy activity (else the black screen will remain active)
		StopDummyActivity();

		if (bNoApp)
		{
			int j = 0;

			while (j < 2)
			{
				Toast.makeText(getApplicationContext(), "Oops!! Sorry, could not find AngryBirds. Please install the APK & try again", Toast.LENGTH_LONG).show();
				j++;
			}

			bNoApp = false;
		}


        for (int i = 0; i < services.size(); i++) 
        { 
            Log.e(TAG, "PushActivityToFront:Name = " +services.get(i).topActivity.toString() + ", id = " +services.get(i).id);
            if ((services.get(i).topActivity.toString().equalsIgnoreCase("ComponentInfo{com.broadcom.appmgr/com.broadcom.appmgr.BcmHomeKey}")) ||
                (services.get(i).topActivity.toString().equalsIgnoreCase("ComponentInfo{com.broadcom.appmgr/com.broadcom.appmgr.BcmAppHandler}")))
            {
                continue;
            }

            else
            {
                TaskId = services.get(i).id;

                // If we need to push AngryBirds in particular, look for that task
                if (bPushAB) 
                {                
		            if (services.get(i).topActivity.toString().startsWith(sPrefix)) 
				    {
//						Log.e(TAG, "PushActivityToFront: Found AngryBirds!!");
                        break;
					}

                    else
                        continue;
                }

                else
                    break;
            }
        }

        if (TaskId != -1)
        {
            Log.e(TAG, "PushActivityToFront:Pushing task now, id = " +TaskId);
            activityManager.moveTaskToFront(TaskId, ActivityManager.MOVE_TASK_WITH_HOME);
        }
    }

    private void StartDummyActivity()
    {
        Log.e(TAG, "StartDummyActivity: pushing app to background");
        DummyCon = getApplicationContext();
        DummyIntent = new Intent(DummyCon, BcmHomeKey.class);

		DummyIntent.putExtra("KEEP_ACTIVITY", true);
        DummyIntent.setFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
        DummyIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
		DummyIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);

//        DummyIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        startActivity(DummyIntent);
    }

    private void StopDummyActivity()
	{
		if (DummyIntent != null)
		{
			DummyIntent.putExtra("KEEP_ACTIVITY", false);
			startActivity(DummyIntent);
		}
	}

    @Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        Log.e(TAG, "onActivityResult:Enter");
		super.onActivityResult(requestCode, resultCode, data);
	
    	if (requestCode == 10)
        {
            Log.e(TAG, "onActivityResult:AngryBirds exiting?");
        }
	}

    /* LocalServerSocket */
    public class LocSoc extends Thread
    {
        int bufferSize = 255;
        byte[] buffer;
        int bytesRead;
        LocalServerSocket server;
        LocalSocket receiver;
        InputStream input;
        private volatile boolean stopThread;

        public LocSoc() 
        {
            Log.e(TAG, "LocSoc:Enter...");

            buffer = new byte[bufferSize];
            bytesRead = 0;

            try
            {
                server = new LocalServerSocket(SOCKET_ADDRESS);
            } 
 
            catch (IOException e)
            {
                // TODO Auto-generated catch block
                Log.e(TAG, "LocSoc::created failed !!!");
                e.printStackTrace();
            }

            stopThread = false;
            Log.e(TAG, "LocSoc:Exit");
        }

        public void run()
        {
            Log.e(TAG, "LocSoc:run, Enter");

            while (!stopThread)
            {
                if (null == server)
                {
                    Log.d(TAG, "LocSoc:run, socket is NULL!!!");
                    stopThread = true;
                    break;
                }

                try
                {
                    // create a file to signal that the app is ready
                    File f = new File("/tmp/boot_complete.cfg");
                    f.createNewFile();
					f.setReadable(true, false);

                    Log.d(TAG, "LocSoc:run, calling accept");
                    receiver = server.accept();
                }
 
                catch (IOException e)
                {
                    // TODO Auto-generated catch block
                    Log.d(TAG, "LocSoc:run, accept failed!!!");
                    e.printStackTrace();
                    continue;
                }

                try
                {
                    Log.d(TAG, "LocSoc:run, ready to receive");
                    input = receiver.getInputStream();
                }

                catch (IOException e)
                {
                    // TODO Auto-generated catch block
                    Log.d(TAG, "LocSoc:run, getInputStream failed!!!");
                    e.printStackTrace();
                    continue;
                }
                Log.d(TAG, "LocSoc:run, connection to client established");

                while (receiver != null)
                {
                    try
                    {
                        Log.d(TAG, "LocSoc:run, ready to read data");
                        bytesRead = input.read(buffer, 0, bufferSize);
                    }

                    catch (IOException e)
                    {
                        // TODO Auto-generated catch block
                        Log.d(TAG, "LocSoc:run, read failed!!!");
                        e.printStackTrace();
                        break;
                    }

                    if (bytesRead >= 0)
                    {
                        String str = new String(buffer, 0, bytesRead);

                        Log.d(TAG, "LocSoc:run, Received data from socket, str = " +str + ", bytesRead = " +bytesRead);

                        if (str.indexOf(sLaunch_AB) != -1)
                        {
							// Get the string after "START_AB"
							sPkgName = str.substring(8);

							// Set up the fully qualified name as well
							sPrefix = "ComponentInfo{" + sPkgName;

                            Log.d(TAG, "LocSoc:run, Will launch AngryBirds, sPkgName = " +sPkgName + ", sPrefix = " +sPrefix);
                            mHandler.sendEmptyMessageDelayed(nLaunch_AB, 0);
                        }

                        if (str.compareTo(sKill_AB) == 0)
                        {
//                            Log.d(TAG, "LocSoc:run, Will kill AngryBirds");
                            mHandler.sendEmptyMessageDelayed(nKill_AB, 0);
                        }

                        if (str.compareTo(sDesktopHide) == 0)
                        {
//                            Log.d(TAG, "LocSoc:run, Will hide Android");
                            mHandler.sendEmptyMessageDelayed(nDesktopHide, 0);
                        }

                        if (str.compareTo(sDesktopShow) == 0)
                        {
//                            Log.d(TAG, "LocSoc:run, Will show Android");
                            mHandler.sendEmptyMessageDelayed(nDesktopShow, 0);
                        }

                        if (str.compareTo(sDesktopShowNoApp) == 0)
                        {
//                            Log.d(TAG, "LocSoc:run, Will show box with app not installed message");
                            mHandler.sendEmptyMessageDelayed(nDesktopShowNoApp, 0);
                        }						
                    }
                }

                Log.d(TAG, "LocSoc:run, The client socket is NULL!!!");
            }

            Log.d(TAG, "LocSoc:run, The thread is going to stop!!!");
            if (receiver != null)
            {
                try
                {
                    Log.d(TAG, "LocSoc:run, Closing receiver!!!");
                    receiver.close();
                }

                catch (IOException e)
                {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
            }

            if (server != null)
            {
                try
                {
                    Log.d(TAG, "LocSoc:run, Closing server!!!");
                    server.close();
                }

                catch (IOException e)
                {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
            }
        }

/*
        public void setStopThread(boolean value)
        {
            stopThread = value;
            Thread.currentThread().interrupt(); // TODO : Check
        }
*/
    }
}
