package com.android.generalSTBFunctions;
import android.util.Log;
public class native_generalSTBFunctions {
    static {
        System.loadLibrary("jni_generalSTBFunctions");
    }
    
    /* set graphics setting according to video offset settings */
    public native int getBrightness(); 
    public native int setBrightness(int progress);     
    
    public native int getContrast(); 
    public native int setContrast(int progress);     
    
    public native int getSaturation(); 
    public native int setSaturation(int progress);     
    
    public native int getHue(); 
    public native int setHue(int progress); 
    
    public native int removeOutput();  
    public native int addOutput();     
}
