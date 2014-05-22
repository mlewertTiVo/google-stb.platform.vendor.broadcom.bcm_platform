package com.broadcom.TrellisUI;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.view.KeyEvent;
import android.view.View;

import org.jwebsocket.api.*;
import org.jwebsocket.kit.*;
import org.jwebsocket.token.*;


public class WebsocketActivity extends Activity implements TrellisUIWebsocketListener{
	private String mURL = "http://";
	private String mAndroidPort = "7681";
	private String mTrellisURL = ":80/trellis/apps/index.html";
	WebView mWebView;	
	jws bam_socket = new jws("BAMSocket_onopen","BAMSocket_onmessage","BAMSocket_onclose");
	jws socket_di = new jws("socket_di_onopen","socket_di_onmessage","socket_di_onclose");
	jws socket = new jws("socket_onopen","socket_onmessage","socket_onclose");
    private static final String TAG = "WebsocketActivity";
	private boolean needKeyboard = true;

	private class TrellisUIWebView extends WebViewClient {
		@Override
		public boolean shouldOverrideUrlLoading(WebView view, String url) {
			view.loadUrl(url);
			socket_di.close();
			needKeyboard = false;
			return true;
		}
	}

	
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.main);

        socket_di.addTrellisListner(this);
        socket.addTrellisListner(this);
        bam_socket.addTrellisListner(this);

		mWebView = (WebView) findViewById(R.id.webview);
		mWebView.setWebViewClient(new TrellisUIWebView());
        mWebView.getSettings().setJavaScriptEnabled(true);
        mWebView.addJavascriptInterface(socket_di, "socket_di");
        mWebView.addJavascriptInterface(socket, "socket");
        mWebView.addJavascriptInterface(bam_socket, "bamSocket");

		wsfactory factoryobj = new wsfactory(mWebView);
        mWebView.addJavascriptInterface(factoryobj, "websocketFactory");
        mURL += settings.getUrl();
		/* If URL does not contain android port number, then remote box is Trellis.
		    Prepare Trellis URL. */
		if (!(mURL.contains(mAndroidPort)))
			mURL += mTrellisURL;
		Log.v(TAG,"Loading URL"+mURL);
        mWebView.loadUrl(mURL);
    }

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if ((keyCode == KeyEvent.KEYCODE_BACK) && mWebView.canGoBack()) {
			socket.close();
			bam_socket.close();
			mWebView.goBack();
			needKeyboard = true;
			return true;
		}
		return super.onKeyDown(keyCode, event);
	}
    
    public void processOpened(String processOpenedHandler){
		Log.v(TAG, "Socket opened");;
		if (needKeyboard)
			mWebView.requestFocus(View.FOCUS_DOWN);
		mWebView.loadUrl("javascript:"+processOpenedHandler+"()");
	}

	public void processPacket(String processPacketHandler, WebSocketPacket aPacket) {
		Log.v(TAG, "processPacket "+processPacketHandler);;
		if (aPacket!=null) Log.e(TAG, aPacket.getString());;
		String msg = aPacket.getString();
		if(!needKeyboard) {
			mWebView.loadUrl("javascript:"+processPacketHandler+"()");
		}
	}

    public void processClosed(String processClosedHandler){
		Log.v(TAG,"socket closed");
    	//mWebView.loadUrl("javascript:"+processClosedHandler+"()"); 	
    }
    
}
