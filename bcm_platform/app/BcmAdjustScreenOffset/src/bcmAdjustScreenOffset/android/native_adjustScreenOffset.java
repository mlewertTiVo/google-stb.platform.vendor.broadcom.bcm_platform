package com.android.adjustScreenOffset;
import android.util.Log;
public class native_adjustScreenOffset {
    static {
        System.loadLibrary("jni_adjustScreenOffset");
    }
    
    /* set graphics setting according to video offset settings */
    public native int setScreenOffset(int xoff, int yoff, int width, int height); 
    public native int getScreenOffset(int xoff, int yoff, int width, int height);     
}
