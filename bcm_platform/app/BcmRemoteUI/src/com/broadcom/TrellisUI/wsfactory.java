package com.broadcom.TrellisUI;

import android.util.Log;
import org.jwebsocket.api.*;
import org.jwebsocket.kit.*;
import org.jwebsocket.token.*;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class wsfactory implements TrellisUIWebsocketListener{

    private static final String TAG = "wsfactory";
	WebView mWebView;

	wsfactory(WebView x){
		mWebView = x;
	}

	public jws createWs(String open_handler, String message_handler, String close_handler) {
		Log.v(TAG,"wsfactory::createWs");
		jws x = new jws(open_handler, message_handler, close_handler);
		x.addTrellisListner(this);
		return x;
	}
    public void processOpened(String processOpenedHandler){
		Log.v(TAG, "Socket opened");;
		//if (needKeyboard)
		//mWebView.requestFocus(View.FOCUS_DOWN);
		mWebView.loadUrl("javascript:"+processOpenedHandler+"()");
	}

	public void processPacket(String processPacketHandler, WebSocketPacket aPacket) {
		Log.v(TAG, "processPacket "+processPacketHandler);;
		if (aPacket!=null) Log.e(TAG, aPacket.getString());;
		String msg = aPacket.getString();
		//if(!needKeyboard) {
			mWebView.loadUrl("javascript:"+processPacketHandler+"()");
		//}
	}

    public void processClosed(String processClosedHandler){
		Log.v(TAG,"socket closed");
    	//mWebView.loadUrl("javascript:"+processClosedHandler+"()"); 	
    }

}
