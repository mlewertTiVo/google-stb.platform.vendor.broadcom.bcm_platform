package com.android.changedisplayformat;
import android.util.Log;
public class native_changedisplayformat {
    static {
        System.loadLibrary("jni_changedisplayformat");
    }

    /** 
     * set display format
     */
    public native int setDisplayFormat(int displayFormat);
    /** 
     * get display format
     */
    public native int getDisplayFormat();
	
}
