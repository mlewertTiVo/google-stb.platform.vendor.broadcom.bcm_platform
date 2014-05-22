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

public class BcmHomeKey extends Activity
{
    public String TAG = "BcmAppMgr:HomeKey";

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
//        textStatus = (TextView) findViewById(R.id.textStatus);
    }

	@Override
	protected void onNewIntent(Intent intent)
	{
		super.onNewIntent(intent);

		boolean action = intent.getExtras().getBoolean("KEEP_ACTIVITY");
		if (action == false)
		{
			finish();
		}
	}

    @Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
		super.onActivityResult(requestCode, resultCode, data);
	
    	if (requestCode == 18)
        {
            Log.e(TAG, "onActivityResult:Enter");
        }
	}

    @Override
    protected void onPause ()
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
}