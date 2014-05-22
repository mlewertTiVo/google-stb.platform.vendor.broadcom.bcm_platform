package android.analoginplayer;

import android.app.Activity;
import android.os.Bundle;


import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.TextView;

import android.widget.MediaController;
import android.widget.VideoView;
import android.view.Window;
import android.view.WindowManager;

import java.util.HashMap;
import java.util.Map;

import android.app.Activity;
import android.os.Bundle;

public class analoginplayer extends Activity {
    /** Called when the activity is first created. */
	private static final String TAG = "analoginplayer";
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		  // turn off the window's title bar, must be called before setContentView
        requestWindowFeature(Window.FEATURE_NO_TITLE);
		  
		//set fullscreen flag to window
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, 
                   WindowManager.LayoutParams.FLAG_FULLSCREEN);
		

		
        setContentView(R.layout.main);
		
        
        video_play(0);
		Log.v(TAG,"Select Analog-in as video source");
    }
    
    public void video_play(int index)
    {
        VideoView videoView = (VideoView) findViewById(R.id.videoView1);
        MediaController mediaController = new MediaController(this);
        mediaController.setAnchorView(videoView);

        Uri video = Uri.parse("brcm:analog_in");
	 	Map<String, String> headers = null;

        videoView.setVideoURI(video);

        videoView.requestFocus();
        videoView.start();
        Log.d(TAG, "Analog-in Starts...");
    }
}