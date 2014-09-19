package com.android.bcmnativeplayer;
public class native_BcmNativePlayer {
    static {
        System.loadLibrary("jni_BcmNativePlayer");
    }
    
    public native int start(String uri);     
    public native int stop(); 
}
