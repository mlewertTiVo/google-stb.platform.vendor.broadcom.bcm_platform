package com.broadcom.tvinput;

import android.util.Log;
import com.broadcom.tvinput.ChannelInfo;
import com.broadcom.tvinput.ProgramInfo;
import com.broadcom.tvinput.ScanInfo;

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
    static native int startBlindScan();
    static native int stopScan();
    static native int tune(String id); // from ChannelInfo
    static native ChannelInfo[] getChannelList();
    static native ProgramInfo[] getProgramList(String internalid);
    static native ScanInfo getScanInfo();
    static native long getUtcTime();
    static native int stop();
    static native int release();
    static native int setSurface();
    static native VideoTrackInfo[] getVideoTrackInfoList();
}
