package com.broadcom.BcmAdjustScreenOffset;
import android.graphics.Rect;
import android.util.Log;

public class native_adjustScreenOffset {
    static {
        System.loadLibrary("jni_adjustScreenOffset");
    }

    /* set graphics setting according to video offset settings */
    public native void setScreenOffset(Rect offset);
    public native void getScreenOffset(Rect offset);
    public native void resetScreenOffset();
}
