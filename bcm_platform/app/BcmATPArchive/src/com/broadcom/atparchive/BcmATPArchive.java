package com.broadcom.atparchive;

import android.util.Log;

public class BcmATPArchive
{
    public String TAG = "BcmATPArchive";
    private BcmATPArchive_JNI mNative;

    public BcmATPArchive()
    {
		Log.e(TAG, "Creating BcmATPArchive_JNI object!!");
		mNative = new BcmATPArchive_JNI();
    }

	public int setDataSource(String uri)
	{
		int iRet;

		Log.e(TAG, "Calling setDataSource, uri = " + uri);
		iRet = mNative.setDataSource(uri);
		Log.e(TAG, "Returned from setDataSource, iRet = " +iRet);

		return iRet;
	}

	public int stop()
	{
		int iRet;

		Log.e(TAG, "Calling stop");
		iRet = mNative.stop();
		Log.e(TAG, "Returned from stop");

		return iRet;
	}

	public int getPlaybackStatus()
	{
		int iRet;

		Log.e(TAG, "Calling getPlaybackStatus");
		iRet = mNative.getPlaybackStatus();
		Log.e(TAG, "Returned from getPlaybackStatus");

		return iRet;
	}
}
