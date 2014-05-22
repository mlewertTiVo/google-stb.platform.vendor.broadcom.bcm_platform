package com.broadcom.bcmdmsserver;

import android.app.IntentService;
import android.content.Intent;
import android.util.Log;

public class BcmDMSService extends IntentService {

	/**
	* Log tag for this service.
	*/ 
	private final String LOGTAG = "BcmDMSService";

	public BcmDMSService() {
		super("BcmDMSService");	
	}

	@Override
	protected void onHandleIntent (Intent intent) 
	{	
		if(isDMSStarted() == 0) 
			startDMS();		
		/* else
			stopDMS(); */
	}

	public native int startDMS();
	public native int stopDMS();
	public native int isDMSStarted();
	
	static 
	{
		/* Load the jni shared library */
		System.loadLibrary("bcm_dms_server_jni");		
    }
}

