package com.broadcom.tvinput;

import android.util.Log;
import com.broadcom.tvinput.ChannelInfo;

public class TunerHAL
{
    private static String LIB_NAME = "jni_tuner";
    private static final String TAG = "Tuner_HAL";

    static
    {
        Log.e(TAG, "Loading " + LIB_NAME +"...");
        System.loadLibrary(LIB_NAME);
        Log.e(TAG, "Loaded " + LIB_NAME +"...");
    }

    static native int initialize();
    static native int tune(int channel);
    static native ChannelInfo[] getChannelList();
    static native int stop();
}
