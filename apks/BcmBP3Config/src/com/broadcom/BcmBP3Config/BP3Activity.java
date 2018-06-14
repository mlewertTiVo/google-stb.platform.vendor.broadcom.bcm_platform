package com.broadcom.BcmBP3Config;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.util.Log;
import android.os.RemoteException;
import android.os.Handler;
import android.os.Looper;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.StringTokenizer;

import static com.broadcom.BcmBP3Config.BP3Activity.State.INVALID;
import static com.broadcom.BcmBP3Config.BP3Activity.State.INIT;
import static com.broadcom.BcmBP3Config.BP3Activity.State.OPTION_STATUS;
import static com.broadcom.BcmBP3Config.BP3Activity.State.SERVICE_SEARCH;
import static com.broadcom.BcmBP3Config.BP3Activity.State.SERVICE_FOUND;
import static com.broadcom.BcmBP3Config.BP3Activity.State.SERVICE_FAILED;
import static com.broadcom.BcmBP3Config.BP3Activity.State.INTERFACE_VALID;
import static com.broadcom.BcmBP3Config.BP3Activity.State.INTERFACE_INVALID;
import static com.broadcom.BcmBP3Config.BP3Activity.State.STATUS_INVALID;
import static com.broadcom.BcmBP3Config.BP3Activity.State.STATUS_RETRIEVED;
import static com.broadcom.BcmBP3Config.BP3Activity.State.OPTION_SERVER;
import static com.broadcom.BcmBP3Config.BP3Activity.State.SERVER_RUNNING;
import static com.broadcom.BcmBP3Config.BP3Activity.State.SERVER_STOPPED;
import static com.broadcom.BcmBP3Config.BP3Activity.State.OPTION_PROVISION;
import static com.broadcom.BcmBP3Config.BP3Activity.State.PROVISIONED;
import static com.broadcom.BcmBP3Config.BP3Activity.State.PROVISION_FAILED;

import bcm.hardware.bp3.V1_0.Ibp3;

public class BP3Activity extends Activity
{
    private static final String TAG = "BcmBP3Config";
    enum Mode
    {
        STATUS, SERVICE, PROVISION, INVALID
    }

    enum State
    {
	    INVALID, INIT, SERVICE_SEARCH, SERVICE_FOUND, SERVICE_FAILED, INTERFACE_VALID, INTERFACE_INVALID, OPTION_STATUS,
	    OPTION_SERVER, OPTION_PROVISION, STATUS_INVALID, STATUS_RETRIEVED, SERVER_RUNNING, SERVER_STOPPED, PROVISIONED, PROVISION_FAILED
    }
    Context mContext;
    private RadioGroup mRadioGroup;
    private RadioButton mStatusButton, mServerButton, mProvisionButton;
    private TextView mInfo1, mInfo2;
    private EditText mArgs1, mArgs2;
    private Button mRunButton;
    private String strStatusResult = "N/A";
    private String strStatusInfo = "Displays the currently enabled licenses\n";
    private String strServerInfo = "IP Address (xx.xx.xx.xx)";
    private String strKeyInfo = "Authentication Key:";
    private String strLicenseInfo = "Licenses (ex: 3,4):";
    private String strPortInfo = "Port (default is 80):";
    private String iMessage = "INVALID";
    private String strStatusArgs = "N/A";
    private String strServerArgs = "";
    private String strKey = "", strLicenses = "";
    private Mode iMode = Mode.STATUS;
    private State iState = INIT, iPrevState = INVALID;
    private String strRun = "Run App", strStop = "Stop App";
    private RelativeLayout.LayoutParams mParams1;
    private boolean bRun = true;
    SharedPreferences sharedPref;
    SharedPreferences.Editor editor;
    private String strPkgName = "com.broadcom.BcmBP3Config";
    private Ibp3 bp3Interface;

    private void bp3Thread(final Context appContext)
    {
        new Thread()
        {
            public void run() 
            {
                while (bRun) 
                {
                    try 
                    {
                        runOnUiThread(new Runnable() 
                        {
                            @Override
                            public void run() 
                            {
                                if (iPrevState != iState)
                                {
                                    // Reset to null
                                    iMessage = "";

                                    if (iState == INIT)
                                        iMessage = "Initializing";

                                    else if (iState == SERVICE_SEARCH)
                                        iMessage = "Searching for the service";

                                    else if (iState == SERVICE_FAILED)
                                        iMessage = "Failed to find the service";

                                    else if (iState == SERVICE_FOUND)
                                        iMessage = "Found the service";

                                    else if (iState == INTERFACE_INVALID)
                                        iMessage = "Invalid interface";

                                    else if (iState == INTERFACE_VALID)
                                        iMessage = "The interface is valid";

                                    else if (iState == OPTION_STATUS)
                                        iMessage = "Will check for the status";

                                    else if (iState == STATUS_INVALID)
                                        iMessage = "Status invalid";

                                    else if (iState == STATUS_RETRIEVED)
                                        iMessage = "Status retrieved";

                                    else if (iState == OPTION_SERVER)
                                        iMessage = "Will run in server mode";

                                    else if (iState == SERVER_STOPPED)
                                        iMessage = "Server stopped";

                                    else if (iState == SERVER_RUNNING)
                                        iMessage = "Server running";

                                    else if (iState == OPTION_PROVISION)
                                        iMessage = "Will provision the box";

                                    else if (iState == PROVISION_FAILED)
                                        iMessage = "Failed to provision with the new licenses";

                                    else if (iState == PROVISIONED)
                                        iMessage = "Successfully provisioned the box with the new Licenses";

                                    // Display the message (if needed)
                                    if (!iMessage.isEmpty())
                                        Toast.makeText(appContext, iMessage, Toast.LENGTH_SHORT).show();

                                    iPrevState = iState;
                                }
                            } // run()
                        }); // runOnUiThread()
                    Thread.sleep(1000);
                    }
                    catch (InterruptedException e)
                    {
                        e.printStackTrace();
                    }
                }   // while
            }   // run()
        }.start();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_bp3);

        mContext= this.getApplicationContext();
        sharedPref = this.getSharedPreferences(strPkgName, mContext.MODE_PRIVATE);
        editor = sharedPref.edit();

        // Kick off the thread
        bp3Thread(mContext);

        iState = INIT;

        // Get the widgets reference from XML layout
        mRadioGroup = (RadioGroup) findViewById(R.id.idRadioGroup);
        mStatusButton = (RadioButton) findViewById(R.id.idStatusButton);
        mServerButton = (RadioButton) findViewById(R.id.idServerButton);
        mProvisionButton = (RadioButton) findViewById(R.id.idProvisionButton);

        // TextView with scrolling enabled
        mInfo1 = (TextView) findViewById(R.id.idInfo1);
        mInfo1.setMovementMethod(new ScrollingMovementMethod());

        // LayoutParams for mInfo1 TextView
        mParams1 = (RelativeLayout.LayoutParams) mInfo1.getLayoutParams();
        mParams1.height = getResources().getDimensionPixelSize(R.dimen.large_height);
        mInfo1.setLayoutParams(mParams1);

        mArgs1 = (EditText) findViewById(R.id.idArgs1);

        mInfo2 = (TextView) findViewById(R.id.idInfo2);
        mArgs2 = (EditText) findViewById(R.id.idArgs2);

        mRunButton = (Button) findViewById(R.id.idRunApp);

        // Enable only the status button to begin with
        mStatusButton.setChecked(true);
        mInfo1.setText(strStatusInfo);
        mArgs1.setVisibility(View.INVISIBLE);
        mArgs1.setText(strStatusArgs);
        mArgs1.setEnabled(false);

        mInfo2.setVisibility(View.INVISIBLE);
        mArgs2.setVisibility(View.INVISIBLE);

        iState = SERVICE_SEARCH;

        // Get the service handle from the HAL
        try {
            bp3Interface = Ibp3.getService();
        } catch (RemoteException e) {
            iState = SERVICE_FAILED;
            Log.w(TAG, "RemoteException trying to reach Ibp3", e);
            return;
        }

        iState = SERVICE_FOUND;

        if (bp3Interface == null) {
            iState = INTERFACE_INVALID;
            Log.w(TAG, "BP3 service extension is null");
            throw new IllegalArgumentException("BP3 service extension is null");
        }

        iState = INTERFACE_VALID;

        mRadioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                // checkedId is the RadioButton selected
                if (checkedId == R.id.idStatusButton) {
                    iState = OPTION_STATUS;

                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.large_height);
                    mInfo1.setLayoutParams(mParams1);
                    mInfo1.setText(strStatusInfo);

                    mArgs1.setText(strStatusArgs);
                    mArgs1.setEnabled(false);
                    mArgs1.setVisibility(View.INVISIBLE);
                    mArgs1.setInputType(InputType.TYPE_CLASS_TEXT);

                    mInfo2.setVisibility(View.INVISIBLE);
                    mArgs2.setVisibility(View.INVISIBLE);
                    iMode = Mode.STATUS;
                } else if (checkedId == R.id.idServerButton) {
                    iState = OPTION_SERVER;

                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.small_height);
                    mInfo1.setText(strServerInfo);

                    mArgs1.setVisibility(View.VISIBLE);
                    mArgs1.setText(strServerArgs);
                    mArgs1.setEnabled(true);
                    mArgs1.setInputType(InputType.TYPE_CLASS_TEXT);

                    mInfo2.setVisibility(View.VISIBLE);
                    mArgs2.setVisibility(View.VISIBLE);

                    mInfo2.setText(strPortInfo);
                    mArgs2.setText(strLicenses);
                    mArgs2.setEnabled(true);

                    iMode = Mode.SERVICE;
                } else if (checkedId == R.id.idProvisionButton) {
                    iState = OPTION_PROVISION;

                    mParams1.height = getResources().getDimensionPixelSize(R.dimen.small_height);
                    mInfo1.setLayoutParams(mParams1);

                    mInfo1.setText(strKeyInfo);
                    mArgs1.setVisibility(View.VISIBLE);
                    mArgs1.setEnabled(true);

                    String strPrevKey = sharedPref.getString("AuthKey", "invalid");

                    // Check if we have a previously entered valid authentication key
                    if (strPrevKey.equals("invalid"))
                        mArgs1.setText(strKey);

                    // Save the valid key
                    else
                    {
                        mArgs1.setText(strPrevKey);
                        editor.putString("AuthKey", strPrevKey).apply();
                    }

                    // Enable this to hide the authentication keys
                    // mArgs1.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
                    // mArgs1.setTransformationMethod(PasswordTransformationMethod.getInstance());

                    mInfo2.setVisibility(View.VISIBLE);
                    mArgs2.setVisibility(View.VISIBLE);
                    mInfo2.setText(strLicenseInfo);

                    mArgs2.setEnabled(true);
                    mArgs2.setText(strLicenses);
                    iMode = Mode.PROVISION;
                }
            }
        });

        mRunButton.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                String buttonStr = mRunButton.getText().toString();

                switch (iMode) {
                    // Retrieving the currently enabled licenses
                    case STATUS:
                    default:
                    try {
                        strStatusResult = bp3Interface.status();
                    } catch (RemoteException e) {
                        iState = STATUS_INVALID;
                        Log.w(TAG, "RemoteException calling Ibp3.status ", e);
                        return;
                    }

                    iState = STATUS_RETRIEVED;
                    Log.w(TAG, "strStatusResult = " + strStatusResult);

                    if (!strStatusResult.isEmpty())
                        mInfo1.setText(strStatusResult);
                    else
                        mInfo1.setText("N/A");
                    break;

                    // Run as a service
                    case SERVICE:
                        // Check if we need to stop the service
                        if (buttonStr.equals(strStop)) 
                        {
                            // Stop the service now

                            // Change the button text to Run
                            mRunButton.setText(strRun);

                            // Enable the other buttons
                            mStatusButton.setEnabled(true);
                            mProvisionButton.setEnabled(true);
                            mArgs1.setEnabled(true);

                            iState = SERVER_STOPPED;
                        }

                        // Or, if we need to run in Service mode
                        else //if (iMode == Mode.SERVICE)
                        {
                            // Change the button text to Stop
                            mRunButton.setText(strStop);

                            // Also, disable the other 2 radio buttons. Can only be enabled once the Service is stopped
                            mStatusButton.setEnabled(false);
                            mProvisionButton.setEnabled(false);
                            mArgs1.setEnabled(false);

                            iState = SERVER_RUNNING;
                        }
                    break;

                    // Provisioning the box with new licenses
                    case PROVISION:
                        int iResult;

                        // Save the valid key
                        editor.putString("AuthKey", mArgs1.getText().toString()).apply();

                        // Parse and extract the licenses
                        StringTokenizer st = new StringTokenizer(mArgs2.getText().toString(), ",");
                        ArrayList<Integer> licList = new ArrayList<Integer>();

                        while (st.hasMoreTokens())
                            licList.add(Integer.parseInt(st.nextToken()));

                        try {
                            iResult = bp3Interface.provision("bp3.broadcom.com", mArgs1.getText().toString(), licList);
                        } catch (RemoteException e) {
                            iState = PROVISION_FAILED;
                            Log.w(TAG, "RemoteException calling Ibp3.status ", e);
                            return;
                        }

                        iState = PROVISIONED;
                    break;
                    }
                }
            });
        }
}
