package com.broadcom.dlna;

import android.content.Context;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.FrameLayout;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebChromeClient;
import android.view.KeyEvent;
import android.view.View;

//Java package
import java.net.NetworkInterface;
import java.util.Enumeration;
import java.net.InetAddress;
import java.net.SocketException;
import java.lang.InterruptedException;

import android.widget.MediaController;
import android.content.Intent;
import android.widget.Toast;
import android.os.Handler;
import android.os.Message;

public class BcmDlna extends Activity {
	private String mURL = "http://";
	private FrameLayout mFL;
	private static WebView mWebView;	
	Intent FakeIntent;
    private static final String TAG = "BcmDlna";
	public HandlePlayback mPlayback;

	private class TrellisWV extends WebViewClient
    {
		@Override
		public boolean shouldOverrideUrlLoading(WebView view, String url) 
		{
			Log.d("TrellisWV", "shouldOverrideUrlLoading, url = " +url);
			return false;
		}
    }

	public class HandlePlayback
    {
		private BcmDlna mDlna;

		public HandlePlayback(BcmDlna x)
		{
			mDlna = x;
		}

		public void process_stream(String url_from_js)
		{
			Log.e(TAG,"HandlePlayback::process_stream: url = " +url_from_js);

			FakeIntent = new Intent(mDlna, FakeMediaPlayer.class);
			FakeIntent.putExtra("URL From Trellis", url_from_js);
			startActivity(FakeIntent);
		}

		public void quit_app()
		{
			Log.e(TAG,"HandlePlayback::quit_app");
			finish();
		}
    }

    @Override
    public void onCreate(Bundle icicle) 
	{
		Log.d(TAG, "onCreate");
		Toast.makeText(getApplicationContext(), "Please wait while we look for available media servers...", Toast.LENGTH_LONG).show();

        super.onCreate(icicle);
        setContentView(R.layout.main);

		mFL = (FrameLayout) findViewById(R.id.base_layout);

		mWebView = new WebView(getApplicationContext());
//		mWebView = new WebView(this);
		mFL.addView(mWebView);

		mWebView.setWebViewClient(new TrellisWV());

		Log.e(TAG, "onCreate: Calling setJavaScriptEnabled");
		mWebView.getSettings().setJavaScriptEnabled(true);

		Log.e(TAG, "onCreate: Calling addJavascriptInterface");
        mPlayback = new HandlePlayback(this);
        mWebView.addJavascriptInterface(mPlayback, "android_view");

		mURL += getLocalIpAddress();
		mURL += ":80/trellis/apps/mediaPlayer/mediaPlayer.html";

		Log.e(TAG, "onCreate: Calling loadUrl (mURL = " +mURL +")");
		mWebView.loadUrl(mURL);
    }

	static private String getLocalIpAddress() 
	{
		try 
		{
			//Generic way to obtain IP addr
			for (Enumeration<NetworkInterface> en = NetworkInterface.getNetworkInterfaces(); en.hasMoreElements();) 
			{
				NetworkInterface NwkInterface = en.nextElement();

				for (Enumeration<InetAddress> enumIpAddr = NwkInterface.getInetAddresses(); enumIpAddr.hasMoreElements();) 
				{
					InetAddress inetAddress = enumIpAddr.nextElement();

					if (!inetAddress.isLoopbackAddress()) 
					{
						Log.d(TAG, "Got IP from generic nwk interface! addr=" + inetAddress.getHostAddress().toString());

						if( inetAddress.getHostAddress().toString().indexOf(':') == -1)
                            return inetAddress.getHostAddress().toString();
					}
				}
			}
		} 
		
		catch (SocketException ex) 
		{
			Log.e(TAG, ex.toString());
		}

		return null;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) 
	{
		// Check if the key event was the Back button and if there's history
		if ((keyCode == KeyEvent.KEYCODE_BACK) && mWebView.canGoBack()) 
		{
			mWebView.goBack();
			return true;
		}

		return super.onKeyDown(keyCode, event);
	}

	@Override
	protected void onPause() 
	{
		Log.d(TAG, "onPause");
		super.onPause();
	}

	@Override
	protected void onDestroy() 
	{
		Log.d(TAG, "onDestroy");
		super.onDestroy();

		mFL.removeAllViews();

		mWebView.removeAllViews();
		mWebView.clearHistory();
		mWebView.loadUrl("about:blank");
		mWebView.freeMemory();
		mWebView.destroy();
		mWebView = null;

		System.gc();
		Log.d(TAG, "onDestroy: exit");
	}
}