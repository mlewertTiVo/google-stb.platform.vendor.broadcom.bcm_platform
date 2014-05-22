package com.broadcom.NdkSample;

public class MyJni {
	  static {
		    System.loadLibrary("BcmNdkSample_jni");
		  }
		  
		  /** 
		   * Adds two integers, returning their sum
		   */
		  public native int add( int v1, int v2 );
		  
		  /**
		   * Returns Hello World string
		   */
		  public native String hello();
}
