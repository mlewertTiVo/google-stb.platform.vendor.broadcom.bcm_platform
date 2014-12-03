package com.broadcom.tvinput;

import android.util.Log;
import com.broadcom.tvinput.ChannelInfo;
import com.broadcom.tvinput.ProgramInfo;

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

    static native int initialize(Object o);
    static native int scan();
    static native int tune(String id); // from ChannelInfo
    static native ChannelInfo[] getChannelList();
    static native ProgramInfo[] getProgramList(String internalid);
    static native int stop();
    static native int release();
}
