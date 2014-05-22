package android.hdmiinplayer;


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

public class hdmiinplayer extends Activity {
    /** Called when the activity is first created. */
	private static final String TAG = "hdmiinplayer";
	
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
		Log.v(TAG,"Select HDMI-in as video source");
    }
    
    public void video_play(int index)
    {
        VideoView videoView = (VideoView) findViewById(R.id.videoView1);
        MediaController mediaController = new MediaController(this);
        mediaController.setAnchorView(videoView);


	/* test for hdmi_input by fzhang */
        Uri video = Uri.parse("brcm:hdmi_in");
	 Map<String, String> headers = null;
/*
        Uri video = Uri.parse("brcm:live");
        //Uri video = Uri.parse("/pbssd1.ts");
        videoView.setMediaController(mediaController);
        //videoView.setVideoURI(video);
        //
        //
        
        Map<String, String> headers = null;
        headers = new HashMap<String, String>();
        //headers.put("input", nav.info[index].input);
        headers.put("input", ""+input);
        headers.put("v_codec",""+ nav.info[index].v_c);
        headers.put("a_codec",""+ nav.info[index].a_c);
        headers.put("a_pid",""+ nav.info[index].a);
        headers.put("v_pid",""+ nav.info[index].v);
        headers.put("p_pid",""+ nav.info[index].p);
*/
        videoView.setVideoURI(video);

        videoView.requestFocus();
        videoView.start();
        Log.d(TAG, "Playback Start...");
    }
}