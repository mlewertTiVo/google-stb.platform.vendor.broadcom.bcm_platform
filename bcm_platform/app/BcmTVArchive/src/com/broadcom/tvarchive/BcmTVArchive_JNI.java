package com.broadcom.tvarchive;

import android.util.Log;

public class BcmTVArchive_JNI 
{
    private static String LIB_NAME = "jni_bcmtv";

    static
    {
        Log.e("BcmTVArchive_JNI", "Loading " + LIB_NAME +"...");
        System.loadLibrary(LIB_NAME);
        Log.e("BcmTVArchive_JNI", "Loaded " + LIB_NAME +"...");
    }

	public native boolean setChannel(int pos);
	public native int stop();
    public native String[] getChannelList(int pos);
	public native int setChannel_pip(int pos, int x, int y, int w, int h);
	public native int stop_pip();
	public native int setMode(boolean bIsFCCOn);
	public native int reprime(int iNum);
	public native String[] getPrimedList();
	public native int force_scan();
	public native int close();
}
