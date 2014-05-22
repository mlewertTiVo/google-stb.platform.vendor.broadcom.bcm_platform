
package com.broadcom.bcmdmsserver;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

import android.app.AlertDialog;
import android.content.DialogInterface;


public class BcmDMSServer extends Activity {

	/** Tag for log */
	private static final String TAG = "BcmDMSServerActivity";

    public BcmDMSServer() {
    }

    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		/* Pass the control to the service */
		Intent intent = new Intent(this, BcmDMSService.class);		
		startService(intent);    

		/* Dialog alert */
		AlertDialog dialog=new AlertDialog.Builder(BcmDMSServer.this).create();
		dialog.setTitle("DLNA DMS");
		dialog.setMessage("Server has been started!");
		dialog.setButton("OK",
		new DialogInterface.OnClickListener()
		{
			public void onClick(DialogInterface dialog, int whichButton)
			{
				BcmDMSServer.this.finish();
			}
		});

		dialog.show();
	}
}

