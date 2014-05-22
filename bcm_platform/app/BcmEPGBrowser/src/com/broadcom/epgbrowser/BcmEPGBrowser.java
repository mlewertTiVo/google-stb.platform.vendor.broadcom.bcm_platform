package com.broadcom.epgbrowser;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.FrameLayout;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebChromeClient;
import android.view.KeyEvent;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.VideoView;

//Java package
import java.net.NetworkInterface;
import java.util.Enumeration;
import java.net.InetAddress;
import java.net.SocketException;

import android.widget.MediaController;
import android.content.Intent;
import android.widget.Toast;
import android.os.Handler;
import android.os.Message;
import android.net.Uri;

public class BcmEPGBrowser extends Activity {
	private String mURL = "http://";

	public static WebView mWebView;
	Intent FakeIntent;

	// Playback states
	public static int STATE_PLAY	= 1;
	public static int STATE_CLOSE	= 2;

    private static final String TAG = "BcmEPGBrowser";
	public static int i = 0;
	public static KeyEvent ke;
	EPGThread mThread;
	public static boolean bKeepAlive = true, bDispatched = false, bClose = false;

	private class TV_WVC extends WebViewClient 
	{
		@Override
		public boolean shouldOverrideUrlLoading(WebView view, String url) 
		{
			Log.d("TV_WVC", "shouldOverrideUrlLoading, url = " +url);
			return false;
		}
	}

	private class TV_WCC extends WebChromeClient 
	{
	    @Override
		public void onShowCustomView(View view, CustomViewCallback callback)
		{
			Log.d("TV_WCC", "onShowCustomView");
			super.onShowCustomView(view, callback);
		}

		@Override
		public void onHideCustomView ()
		{
			Log.d("TV_WCC", "onHideCustomView");
			super.onHideCustomView();
		}
	}

	public Handler mHandler = new Handler(new Handler.Callback()
    {
        @Override
        public boolean handleMessage(Message msg)
        {
            if (msg.what == STATE_PLAY)
            {
				Log.e(TAG, "handleMessage:Calling setVisibility");
//				mWebView.setVisibility(View.VISIBLE);
            }

			else if (msg.what == STATE_CLOSE)
			{
				Log.e(TAG, "handleMessage:Calling Close");
				finish();
				Close();
			}

            return true;
		}
	});

	public class HandlePlayback
	{
		private BcmEPGBrowser mEPG;

		public HandlePlayback(BcmEPGBrowser mObj) 
		{
			mEPG = mObj;
		}

		public void process_stream(String url_from_js)
		{
			Log.e(TAG,"HandlePlayback::process_stream");
			FakeIntent = new Intent(mEPG, FakeMediaPlayer.class);
			FakeIntent.putExtra("URL From Trellis", url_from_js);
			startActivity(FakeIntent);

//			mHandler.sendEmptyMessageDelayed(STATE_PLAY, 0);
		}

		public void quit_app()
		{
			Log.e(TAG,"HandlePlayback::quit_app");
			finish();
		}
	}
	
	// Waits for notification & processes commands
    public class EPGThread extends Thread
    {
        private volatile boolean bStopThread;

        public EPGThread()
        {
			Log.e(TAG, "EPGThread:Enter...");

			// Reset the flag
			bStopThread = false;
        }

        public void run()
        {
            // Process commands here forever
            while (bKeepAlive)
            {
				if (bDispatched)
				{
					dispatchKeyEvent(ke);
					bDispatched = false;
				}

				else if (bClose)
				{
					bClose = false;
					mHandler.sendEmptyMessageDelayed(STATE_CLOSE, 0);
				}
            }

            Log.e(TAG, "EPGThread:exiting run...");
        }

        public void stopThread(boolean value)
        {
            Log.e(TAG, "EPGThread:setStopThread, Enter");
            bStopThread = value;
            Thread.currentThread().interrupt();
        }
    }

    @Override
    public void onCreate(Bundle icicle) 
	{
		Log.d(TAG, "onCreate");
		Toast.makeText(getApplicationContext(), "Please wait while we look for available TV channels...", Toast.LENGTH_LONG).show();

		super.onCreate(icicle);

		Log.d(TAG, "Creating EPGThread");
        mThread = new EPGThread();

        // Kick it off now
        Log.d(TAG, "Starting EPGThread");
        mThread.start();

		mWebView = new WebView(this);

		mWebView.setWebViewClient(new TV_WVC());
	    mWebView.setWebChromeClient(new TV_WCC());

		Log.e(TAG, "onCreate: Calling setJavaScriptEnabled");
		mWebView.getSettings().setJavaScriptEnabled(true);

		Log.e(TAG, "onCreate: Calling addJavascriptInterface");
		HandlePlayback mPlayback = new HandlePlayback(this);
		mWebView.addJavascriptInterface(mPlayback, "android_view");

        mURL += getLocalIpAddress();
		mURL += ":80/trellis/apps/trellis/tvclient.html";

		Log.e(TAG, "onCreate: Calling loadUrl (mURL = " +mURL +")");
        mWebView.loadUrl(mURL);

		// Enable the view
        setContentView(mWebView);
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
		Log.e(TAG, "onKeyDown: " +keyCode);

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
		super.onPause();

		Log.d(TAG, "onPause");
	}

	private void Close()
	{
		Log.d(TAG, "Close");

		if (mWebView != null)
		{
			Log.d(TAG, "Close, Cleaning WebView");
			mWebView.removeAllViews();
			mWebView.clearHistory();
			mWebView.loadUrl("about:blank");
			mWebView.freeMemory();
			mWebView.destroy();
			mWebView = null;

			mThread.stopThread(true);

			System.gc();
		}
	}

	@Override
	protected void onDestroy() 
	{
		super.onDestroy();

		Log.d(TAG, "onDestroy");
		Close();
		Log.d(TAG, "onDestroy: exit");
	}
}
