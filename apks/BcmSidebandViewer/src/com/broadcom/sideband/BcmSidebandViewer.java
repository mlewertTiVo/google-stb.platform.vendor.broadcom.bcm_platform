package com.broadcom.sideband;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.inputmethod.EditorInfo;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Gravity;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

public class BcmSidebandViewer extends Activity implements SurfaceHolder.Callback, CompoundButton.OnCheckedChangeListener, TextView.OnEditorActionListener, View.OnFocusChangeListener {
    private static final String TAG = BcmSidebandViewer.class.getSimpleName();

    // Please include your own video file and make sure to set ownership, group, and permission flags
    private static String mFilePathDefault = "/data/data/com.broadcom.sideband/video.mp4";

    private SurfaceView mSurfaceView; // Sideband surface
    private CompoundButton mButtonFullscreen;
    private CompoundButton mButtonStartFile;
    private EditText mFilePath;

    private native boolean start_sideband(Surface s);
    private native void stop_sideband();
    private native boolean start_file_player(int x, int y, int w, int h, String path);
    private native void stop_player();
    private native boolean can_access_file(String path);

    /**
     * ---------------------------------------------------------------------
     * Activity lifecycle handlers
     * ---------------------------------------------------------------------
     */

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        // Sideband surface
        mSurfaceView = (SurfaceView) findViewById(R.id.surfaceView);
        mSurfaceView.getHolder().addCallback(this);

        mButtonFullscreen = (CompoundButton) findViewById(R.id.button_fs);
        mButtonFullscreen.setOnCheckedChangeListener(this);
        mButtonStartFile = (CompoundButton) findViewById(R.id.button_start_file);
        mButtonStartFile.setOnCheckedChangeListener(this);
        mFilePath = (EditText) findViewById(R.id.file_path);
        mFilePath.setText(mFilePathDefault);
        mFilePath.setOnEditorActionListener(this);
        mFilePath.setOnFocusChangeListener(this);
    }

    private void enableFilePlaybackButton() {
        if (can_access_file(mFilePath.getText().toString()))
            mButtonStartFile.setEnabled(true);
        else
            mButtonStartFile.setEnabled(false);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        Log.d(TAG, "onWindowFocusChanged hasFocus=" + hasFocus);
        View view = findViewById(R.id.bannerView);
        if (hasFocus && view.getAnimation() == null) {
            Log.d(TAG, "Starting Animation");
            Animation animation;
            animation = AnimationUtils.loadAnimation(this, R.anim.bounce);
            view.startAnimation(animation);
        }
    }

    @Override
    protected void onResume() {
        Log.d(TAG, "onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.d(TAG, "onPause");
        mButtonStartFile.setChecked(false);
        super.onPause();
    }

    @Override
    protected void onStop() {
        Log.d(TAG, "onStop");
        mButtonStartFile.setChecked(false);
        super.onStop();
    }

    /**
     * ---------------------------------------------------------------------
     * onCheck handler for our 2 toggle buttons (see main.xml)
     * ---------------------------------------------------------------------
     */

    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (mSurfaceView.getHolder().getSurface() == null) {
            Log.d(TAG, "onCheckedChanged: there is no surface!!!");
            return;
        }
        if (!buttonView.isEnabled()) {
            Log.d(TAG, "onCheckedChanged: button is not enabled!!!");
            return;
        }
        if (buttonView == mButtonStartFile) {
            Log.d(TAG, "onCheckedChanged (Start) isChecked=" + isChecked);
            if (isChecked) {
                int[] pos = new int[2];
                int width, height;
                mSurfaceView.getLocationOnScreen(pos);
                width = mSurfaceView.getMeasuredWidth();
                height = mSurfaceView.getMeasuredHeight();
                Log.d(TAG, "coordinates: " + pos[0] + "," + pos[1] + "," + width + "," + height);
                if (!start_file_player(pos[0], pos[1], width, height, mFilePath.getText().toString())) {
                    buttonView.setChecked(false);
                }
            } else {
                stop_player();
                enableFilePlaybackButton();
            }
        } else if (buttonView == mButtonFullscreen) {
            Log.d(TAG, "onCheckedChanged (FullScreen) isChecked=" + isChecked);
            FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
            if (isChecked) {
                frameLayoutParams.width = FrameLayout.LayoutParams.MATCH_PARENT;
                frameLayoutParams.height = FrameLayout.LayoutParams.MATCH_PARENT;
            } else {
                frameLayoutParams.width = mSurfaceView.getMeasuredWidth()/2;
                frameLayoutParams.height = mSurfaceView.getMeasuredHeight()/2;
                frameLayoutParams.gravity = Gravity.CENTER;
            }
            mSurfaceView.setLayoutParams(frameLayoutParams);
            // Show/Hide buttons as necessary
            findViewById(R.id.menu_row2).setVisibility(isChecked?View.GONE:View.VISIBLE);
        }
    }

    /**
     * ---------------------------------------------------------------------
     * onClick handlers for our other buttons (see main.xml)
     * ---------------------------------------------------------------------
     */

    public void handleLeftBtn(View view) {
        Log.d(TAG, "handleLeftBtn");
        FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
        frameLayoutParams.gravity = Gravity.LEFT | Gravity.CENTER_VERTICAL;
        mSurfaceView.setLayoutParams(frameLayoutParams);
    }

    public void handleTopBtn(View view) {
        Log.d(TAG, "handleTopBtn");
        FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
        frameLayoutParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.TOP;
        mSurfaceView.setLayoutParams(frameLayoutParams);
    }

    public void handleCenterBtn(View view) {
        Log.d(TAG, "handleCenterBtn");
        FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
        frameLayoutParams.gravity = Gravity.CENTER;
        mSurfaceView.setLayoutParams(frameLayoutParams);
    }

    public void handleBottomBtn(View view) {
        Log.d(TAG, "handleBottomBtn");
        FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
        frameLayoutParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
        mSurfaceView.setLayoutParams(frameLayoutParams);
    }

    public void handleRightBtn(View view) {
        Log.d(TAG, "handleRightBtn");
        FrameLayout.LayoutParams frameLayoutParams = (FrameLayout.LayoutParams) mSurfaceView.getLayoutParams();
        frameLayoutParams.gravity = Gravity.RIGHT | Gravity.CENTER_VERTICAL;
        mSurfaceView.setLayoutParams(frameLayoutParams);
    }



    /**
     * ---------------------------------------------------------------------
     * Monitor file path changes
     * ---------------------------------------------------------------------
     */
    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (actionId == EditorInfo.IME_ACTION_DONE ||
            actionId == EditorInfo.IME_ACTION_UNSPECIFIED) {
            enableFilePlaybackButton();
        }
        return false;
    }

    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        if (v == mFilePath && !hasFocus) {
            enableFilePlaybackButton();
        }
    }

    /**
     * ---------------------------------------------------------------------
     * SurfaceHolder.Callback implementation
     * ---------------------------------------------------------------------
     */

    /**
     * This is called immediately after the surface is first created.
     * Implementations of this should start up whatever rendering code
     * they desire.  Note that only one thread can ever draw into
     * a {@link Surface}, so you should not draw into the Surface here
     * if your normal rendering will be in another thread.
     *
     * @param holder The SurfaceHolder whose surface is being created.
     */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated");
        tryDrawing(holder);
        if (!start_sideband(holder.getSurface())) {
            Log.w(TAG, "Unable to register the sideband");
        }
        enableFilePlaybackButton();
    }

    /**
     * This is called immediately after any structural changes (format or
     * size) have been made to the surface.  You should at this point update
     * the imagery in the surface.  This method is always called at least
     * once, after {@link #surfaceCreated}.
     *
     * @param holder The SurfaceHolder whose surface has changed.
     * @param format The new PixelFormat of the surface.
     * @param width The new width of the surface.
     * @param height The new height of the surface.
     */
    @Override
    public void surfaceChanged(SurfaceHolder holder, int frmt, int w, int h) {
        Log.d(TAG, "surfaceChanged: w=" + w + " h=" + h);
        tryDrawing(holder);
    }

    private void tryDrawing(SurfaceHolder holder) {
        Log.d(TAG, "tryDrawing");
        Canvas canvas = holder.lockCanvas();
        if (canvas == null) {
            Log.e(TAG, "Cannot draw onto the canvas as it's null");
        } else {
            Log.d(TAG, "Drawing an alpha hole");
            canvas.drawColor(Color.argb(0, 255, 255, 255));
            holder.unlockCanvasAndPost(canvas);
        }
    }

    /**
     * This is called immediately before a surface is being destroyed. After
     * returning from this call, you should no longer try to access this
     * surface.  If you have a rendering thread that directly accesses
     * the surface, you must ensure that thread is no longer touching the
     * Surface before returning from this function.
     *
     * @param holder The SurfaceHolder whose surface is being destroyed.
     */
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed");
        mButtonStartFile.setChecked(false);
        mButtonStartFile.setEnabled(false);
        stop_sideband();
    }

    /**
     * ---------------------------------------------------------------------
     * Auto-load the JNI library
     * ---------------------------------------------------------------------
     */
    static {
        System.loadLibrary("bcmsidebandviewer_jni");
    }
}
