package com.android.bcmnativeplayer;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Window;
import android.view.WindowManager;
import android.widget.MediaController;
import android.widget.TextView;
import android.widget.VideoView;

public class BcmNativePlayer extends Activity {
    private static VideoView mVideoView;
    private static MediaController mMediaController;
	private native_BcmNativePlayer nativePlayer;
	private int index = 0;

	private String[] streams = {
			  "airshow-harmonic-4Kp60-8bit.brcm-remux.ts",
			  "ibc2014-hevc-4Kp60.v1.ts",
			  "bbb-3840x2160-cfg02.mkv",
			  "captainamerica_AVC_15M.ts",
			  "greenlantern_AVC_15M.ts",
			  "byu_wisconsin_720p_60fps.yuv_qp24.v2.ts",
			  "fresno_wyoming_720p_60fps.yuv_qp24.v2.ts",
			};

	private String[] titles = {
			  "Harmonic Air Show",
			  "ibc2014-hevc-4Kp60.v1.ts",
			  "Big Buck Bunny",
			  "Captain America: The First Avenger",
			  "Green Lantern",
			  "BYU vs. Wisconsin",
			  "Fresno vs. Wyoming",
			};

	private String[] descriptions = {
			  "Harmonic On Location Footage at California Airshow.",
			  "No Description",
			  "Big Buck Bunny is a short computer animated comedy film by the Blender Institute, part of the Blender Foundation.",
			  "After being deemed unfit for military service, Steve Rogers volunteers for a top secret research project that turns him into Captain America, a superhero dedicated to defending USA ideals.",
			  "A test pilot is granted an alien ring that bestows him with otherworldly powers that inducts him into an intergalactic police force.",
			  "BYU Cougars vs. Wisconsin Badgers\nNov. 9, 2013\nCamp Randall Stadium, Madison, Wisconsin.",
			  "Fresno State Bulldogs vs. Wyoming Cowboys\nNov. 1, 2013\nBulldog Stadium, Fresno, California.",
			};

	private String[] specifications = {
			  "HEVC 4Kp60",
			  "HEVC 4Kp60",
			  "HEVC 4Kp30 6Mbps",
			  "AVC 15Mbps",
			  "AVC 15Mbps",
			  "HEVC 720p60fps",
			  "HEVC 720p60fps",
			};

	protected void start(int index) {
		mVideoView = (VideoView) findViewById(R.id.VideoView);
	    TextView displayTextView = (TextView)findViewById(R.id.textView1);
        displayTextView.setText("Channel 10"+index+": "+titles[index]+" ("+specifications[index]+")\n"+descriptions[index]);
		nativePlayer.start("/storage/sdcard/"+streams[index]);
	}
    
    protected void stop() {
		Log.d("Trellis Interface", "stop");
		nativePlayer.stop();
	}
    
    protected int prevIndex() {
    	if (index > 0) {
    		index--;
    	}
    	return index;
    }
    
    protected int nextIndex() {
   		index++;
    	if (index >= streams.length) {
    		index = streams.length-1;
    	}
    	return index;
    }
    
	@Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
		nativePlayer=new native_BcmNativePlayer();
        // turn off the window's title bar, must be called before setContentView
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        //Avoid screen locked during video playback
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
        WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.player);

        // Setup the media controller
        mVideoView = (VideoView) findViewById(R.id.VideoView);
        mMediaController = new MediaController(this);
        mMediaController.setAnchorView(mVideoView);
        mVideoView.setMediaController(mMediaController);
        start(index);
    }
	
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
        	stop();	
            start(prevIndex());
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
        	stop();
            start(nextIndex());
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_ESCAPE)
		{
        	stop();
			finish();
            return true;
        }
        return false;
    }
}
