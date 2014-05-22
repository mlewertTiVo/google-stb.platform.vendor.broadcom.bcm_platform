package com.android.tsplayer;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.TextView;
import android.widget.MediaController;
import android.widget.VideoView;
import java.util.HashMap;
import java.util.Map;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

public class show_video extends Activity implements Runnable{
    private static final String TAG = "tsplayer_video";
    private ProgressDialog dlg;
    private int f,s,m;
    private int f1,s1,m1;
    private double input;
    private boolean scan_finished;
    private native_frontend nav;
    private int iCurplay;
    private     VideoView videoView; 
    //private    MediaController mediaController;
    private boolean isexit;

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Window win = getWindow();
        WindowManager.LayoutParams winParams = win.getAttributes();
        winParams.flags |= WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN;
        winParams.flags |= WindowManager.LayoutParams.FLAG_FULLSCREEN;		
        win.setAttributes(winParams);

        requestWindowFeature(Window.FEATURE_NO_TITLE);

        setContentView(R.layout.video);
        String t;

        videoView = (VideoView) findViewById(R.id.videoView1);
        //mediaController = new MediaController(this);
        //mediaController.setAnchorView(videoView);

        videoView.setSystemUiVisibility(View.SYSTEM_UI_CLEARABLE_FLAGS);
        nav=new native_frontend();

        Bundle extras = getIntent().getExtras();
        t = extras.getString("frequency");
        Log.v(TAG,t);

        f = Integer.parseInt(t);

        t = extras.getString("symbol_rate");
        s = Integer.parseInt(t);

        t = extras.getString("mode");
        m = Integer.parseInt(t);

        scan_finished = false;
        isexit =false;
        dlg = new ProgressDialog(this);
        dlg.setMessage("scaning channel, please wait...");
        dlg.show();
        dlg.setIndeterminate(false); 
        fork_thread();
    }
    private int show_dialog(int type,CharSequence title){
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setTitle(title);
        if (type == 0)
        {
            builder.setPositiveButton("ok", new DialogInterface.OnClickListener(){
                public void onClick(DialogInterface dialog, int item) {

                    dialog.dismiss();
                }
            });
        }
        else
        {
            int count=nav.service_amount();
            int i;

            String [] items = new String[count];

            Log.v(TAG,"get program amount: " + count);
            for (i=0;i<count;i++)
            {
                items[i] = "channel " + i + ": audio:"+ nav.info[i].a + "   video:" + nav.info[i].v;
                Log.v(TAG,"channel["+i+"]"+items[i]); 							
            }			
            builder.setItems(items, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int item) {
                    dialog.dismiss();

                    iCurplay = item;

                    Log.v(TAG,"Select item " + item + " to play");
                    video_play(item);
                }
            });
        }
        builder.create().show();
        return 0;
    }
    public void video_play(int index)
    {
        /*      VideoView videoView = (VideoView) findViewById(R.id.videoView1);
                MediaController mediaController = new MediaController(this);
                mediaController.setAnchorView(videoView);
                */     
        int play=1;
        int noplay = 0;
        int previndex, nextindex;
        int inputindex, inputinput,wantplay;
        int vcondec;
        int acondec;
        int vpid;
        int apid;
        int ppid;

        /* Uri video = Uri.parse("brcm:fastrdvlive"); */

        Uri video = Uri.parse("brcm:live");
        //Uri video = Uri.parse("/pbssd1.ts");
        //videoView.setMediaController(mediaController);
        //videoView.setVideoURI(video);

        Map<String, String> headers = null;

        if(nav.service_amount()>=3)
        {

            if(index == 0)
            {
                previndex=nav.service_amount()-1;
            }
            else
            {
                previndex=index-1;
            }

            if(index==nav.service_amount()-1)
            {
                nextindex=0;
            }
            else
            {
                nextindex=index+1;
            }
        }
        else
        {
            if(nav.service_amount()==2)
            {
                if(index == 0)
                {
                    previndex=nav.service_amount()-1;
                }
                else
                {
                    previndex=index-1;
                }

                nextindex=index;
            }
            else
            {
                previndex=index;
                nextindex=index;
            }

        }

        headers = new HashMap<String, String>();
        //headers.put("input", nav.info[index].input);
        headers.put("num", ""+previndex);

        inputindex = (int)nav.info[previndex].input;
        headers.put("input", ""+inputindex);
        headers.put("play", ""+noplay);
        headers.put("v_codec",""+ nav.info[previndex].v_c);
        headers.put("a_codec",""+ nav.info[previndex].a_c);
        headers.put("a_pid",""+ nav.info[previndex].a);
        headers.put("v_pid",""+ nav.info[previndex].v);
        headers.put("p_pid",""+ nav.info[previndex].p);

        Log.d(TAG, "num "+previndex);
        Log.d(TAG, "input "+inputindex);
        Log.d(TAG, "play "+noplay);
        Log.d(TAG, "v_codec "+nav.info[previndex].v_c);
        Log.d(TAG, "v_pid "+nav.info[previndex].v);

        headers.put("onum", ""+index);

        Log.d(TAG, "1num "+index);

        inputindex = (int)nav.info[index].input;
        headers.put("oinput", ""+inputindex);

        Log.d(TAG, "1input "+inputindex);

        headers.put("oplay", ""+play);
        headers.put("ov_codec",""+ nav.info[index].v_c);

        Log.d(TAG, "1v_codec "+nav.info[index].v_c);
        headers.put("oa_codec",""+nav.info[index].a_c );
        headers.put("oa_pid",""+ nav.info[index].a);
        headers.put("ov_pid",""+ nav.info[index].v);

        Log.d(TAG, "1v_pid "+nav.info[index].v);

        headers.put("op_pid",""+ nav.info[index].p);
        headers.put("tnum", ""+ nextindex);


        inputindex = (int)nav.info[nextindex].input;

        headers.put("tinput", ""+inputindex);
        headers.put("tplay", ""+noplay);
        headers.put("tv_codec",""+ nav.info[nextindex].v_c);
        headers.put("ta_codec",""+ nav.info[nextindex].a_c);
        headers.put("ta_pid",""+ nav.info[nextindex].a);
        headers.put("tv_pid",""+ nav.info[nextindex].v);
        headers.put("tp_pid",""+ nav.info[nextindex].p);

        Log.d(TAG, "Set video header ");

        videoView.setVideoURI(video,headers);

        nav.dvbca_playset(nav.info[index].v,nav.info[index].a,nav.info[index].ecm,nav.info[index].emm);

        //Change the URI here if you want to test some other format
        //Uri video = Uri.parse("/Fantastic_Four_720p_HD.mp4");
        //videoView.setMediaController(mediaController);
        //videoView.setVideoURI(video);
        videoView.requestFocus();
        videoView.start();
        Log.d(TAG, "Playback Start...");
    }

    public boolean onKeyDown(int keyCode, KeyEvent msg) {

        Log.d(TAG, "onKeyDown KeyEvent "+ keyCode);

        if (keyCode == KeyEvent.KEYCODE_BACK)
        {
            Log.d(TAG, "KEYCODE_BACK");
            //Intent myIntent = new Intent(this, tsplayer.class);
            //startActivity(myIntent);
            isexit = true;

            finish();
            return true;
        }
        if (nav.service_amount()>0)
        {
            /* videoView.pause();*/

            Log.d(TAG,"stopplayback\n");
            videoView.stopPlayback();

            Log.d(TAG, "show channel list\n");
            /*    show_dialog(1,"channel list");*/

            int count=nav.service_amount();

            iCurplay ++;

            if(count > iCurplay)
            {            	
                Log.d(TAG, "show channel list have " +count + "count");

                Log.d(TAG, "now  play "+iCurplay);

            }
            else
            {
                iCurplay = 0;
            }

            Log.v(TAG,"Select item " + iCurplay + " to play");
            video_play(iCurplay);
        }
        return true;
    }
    public void run() {
        /*do channel scan here, this will call to jni code*/
        nav.reset();

        input = nav.lock_signal(0,f,s,m);
        if (input >= 0 && nav.services_get((int)input) == 0)
            scan_finished = true;

        handler.sendEmptyMessage(0);
    }
    private void fork_thread(){
        Thread thread = new Thread(this);
        thread.start();
    }
    private Handler handler = new Handler() {
        @Override
            public void handleMessage(Message msg) {
                if (!isexit)
                {
                    dlg.dismiss();
                    if (scan_finished == false)
                        show_dialog(0,"unable to scan channel,please check");
                    else
                        show_dialog(1,"channel list");
                }
            }
    };
}
