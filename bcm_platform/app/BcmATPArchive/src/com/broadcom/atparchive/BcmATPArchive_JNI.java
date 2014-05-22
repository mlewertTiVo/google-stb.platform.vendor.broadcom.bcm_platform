package com.broadcom.atparchive;

import android.util.Log;

public class BcmATPArchive_JNI
{
    private static String LIB_NAME = "jni_bcmtp";
	private static final String TAG = "BcmATPArchive_JNI";

	public BcmATPArchive_JNI()
	{
        Log.e(TAG, "Loading " + LIB_NAME +"...");
        System.loadLibrary(LIB_NAME);
        Log.e(TAG, "Loaded " + LIB_NAME +"...");
	}

	public native int setDataSource(String uri);
	public native int stop();
	public native int getPlaybackStatus();
}
