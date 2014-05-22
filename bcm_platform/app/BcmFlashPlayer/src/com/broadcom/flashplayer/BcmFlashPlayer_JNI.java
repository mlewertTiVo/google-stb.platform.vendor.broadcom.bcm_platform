package com.broadcom.flashplayer;

import android.util.Log;

public class BcmFlashPlayer_JNI 
{
    private static String LIB_NAME = "jni_bcmfp";
//    private static String LIB_NAME = "/system/lib/libjni_bcmfp.so";

    static
    {
        Log.e("BcmFlashPlayer_JNI", "Loading " + LIB_NAME +"...");
//        System.load(LIB_NAME);
        System.loadLibrary(LIB_NAME);
        Log.e("BcmFlashPlayer_JNI", "Loaded " + LIB_NAME +"...");
    }

    public native int switchToTrellis();	
}

