package com.broadcom.TrellisUI;

public class settings {
	private static String mURL="ws://10.132.66.94:8787";

	public static void setUrl(String aUrl){
		mURL = aUrl;
	}

	public static String getUrl (){
		return mURL;
	}
}

