package com.broadcom.tvinput;

import android.app.Activity;
import android.app.FragmentManager;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v17.leanback.app.GuidedStepFragment;
import android.support.v17.leanback.widget.GuidanceStylist;
import android.support.v17.leanback.widget.GuidedAction;
import android.util.Log;
import android.content.Intent;

import java.util.List;

public class MainActivity extends Activity {

    public enum FrontendType {
        Undefined,
        Satellite,
        Terrestrial,
        Cable,
        Analog
    };
    private FrontendType frontendType;

    private static final String TAG = MainActivity.class.getSimpleName();

    public static final String SCAN_TYPE = "com.broadcom.tvinput.scan_type";
    public static final String FRONTEND_TYPE = "com.broadcom.tvinput.frontend_type";

    /* Action ID definition */
    public static final int ACTION_NETWORK_SCAN = 0;
    public static final int ACTION_FREQUENCY_SCAN = 1;

    public static final int TUNE_SIGNAL_NONE = 0;
    public static final int TUNE_SIGNAL_QPSK = 1;
    public static final int TUNE_SIGNAL_COFDM = 2;
    public static final int TUNE_SIGNAL_QAM = 3;
    public static final int TUNE_SIGNAL_ANALOG = 4;

    private native int detectFrontendType();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (null == savedInstanceState) {
            Log.d(TAG, "onCreate");
            switch(TunerService.frontendType) {
                case TUNE_SIGNAL_NONE:
                    frontendType = FrontendType.Undefined;
                    break;
                case TUNE_SIGNAL_QPSK:
                    frontendType = FrontendType.Satellite;
                    break;
                case TUNE_SIGNAL_COFDM:
                    frontendType = FrontendType.Terrestrial;
                    break;
                case TUNE_SIGNAL_QAM:
                    frontendType = FrontendType.Cable;
                    break;
                case TUNE_SIGNAL_ANALOG:
                    frontendType = FrontendType.Analog;
                    break;
                default:
                    frontendType = FrontendType.Undefined;
            }
            Log.d(TAG, "frontendType: " + frontendType);
            GuidedStepFragment.add(getFragmentManager(), new FirstStepFragment());
        }
    }

    private static void addAction(List actions, long id, String title, String desc) {
        actions.add(new GuidedAction.Builder()
                .id(id)
                .title(title)
                .description(desc)
                .build());
    }

    public class FirstStepFragment extends GuidedStepFragment {
        @NonNull
        @Override
        public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {

            String title = null;
            Drawable icon = null;

            switch(frontendType) {
                case Undefined:
                    title = "None Detected";
                    break;
                case Satellite:
                    title = "Satellite";
                    icon = getActivity().getDrawable(R.drawable.satellite);
                    break;
                case Terrestrial:
                    title = "Terrestrial";
                    icon = getActivity().getDrawable(R.drawable.terrestrial);
                    break;
                default:
                    title = "Unsupported type";

            }
            String breadcrumb = "Tuner";

            return new GuidanceStylist.Guidance(title, null, breadcrumb, icon);
        }

        @Override
        public void onCreateActions(@NonNull List actions, Bundle savedInstanceState) {
            if(frontendType == FrontendType.Satellite) {
                addAction(actions, ACTION_NETWORK_SCAN, "Network Scan", null);
                addAction(actions, ACTION_FREQUENCY_SCAN, "Single Scan", null);
            } else if(frontendType == FrontendType.Terrestrial) {
                addAction(actions, ACTION_NETWORK_SCAN, "Network Scan", null);
            }

        }

        @Override
        public void onGuidedActionClicked(GuidedAction action) {

            Intent intent;
            switch ((int) action.getId()){
                case ACTION_NETWORK_SCAN:
                    intent = new Intent(getActivity(),SingleScan.class);
                    intent.putExtra(SCAN_TYPE,ACTION_NETWORK_SCAN);
                    intent.putExtra(FRONTEND_TYPE,frontendType.ordinal());
                    startActivity(intent);
                    break;
                case ACTION_FREQUENCY_SCAN:
                    intent = new Intent(getActivity(),SingleScan.class);
                    intent.putExtra(SCAN_TYPE,ACTION_FREQUENCY_SCAN);
                    intent.putExtra(FRONTEND_TYPE,frontendType.ordinal());
                    startActivity(intent);
                    break;
                default:
                    Log.w(TAG, "Action is not defined");
                    break;
            }
        }
    }
}
