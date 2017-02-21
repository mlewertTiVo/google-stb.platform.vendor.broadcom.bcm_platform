/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.broadcom.tvinput.hdmiservice;

import static android.media.tv.TvInputManager.VIDEO_UNAVAILABLE_REASON_TUNING;

import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.Context;
import android.content.Intent;
import android.media.tv.TvInputHardwareInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;
import android.media.tv.TvInputService;
import android.media.tv.TvStreamConfig;
import android.net.Uri;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.Surface;
import android.view.KeyEvent;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * Reference implementation of TvInputService which handles both hardware/logical
 * HDMI input sessions.
 */
public class HdmiTvInputService extends TvInputService {
    private static final boolean DEBUG = false;
    private static final String TAG = "HdmiTvInputService";

    private static final TvStreamConfig[] EMPTY_STREAM_CONFIGS = {};

    // hardware device id -> inputId
    private final SparseArray<String> mHardwareInputIdMap = new SparseArray<String>();

    // inputId -> TvInputInfo
    private final Map<String, TvInputInfo> mInputMap = new HashMap<>();

    // inputId -> portId
    private final Map<String, Integer> mPortIdMap = new HashMap<>();

    // inputId -> TvInputService.Session
    private final Map<String, HdmiInputSessionImpl> mSessionMap = new HashMap<>();

    // portId -> hardware device Id
    private final SparseIntArray mHardwareIdMap = new SparseIntArray();

    private TvInputManager mManager = null;
    private ResolveInfo mResolveInfo;

    @Override
    public void onCreate() {
        super.onCreate();
        mResolveInfo = getPackageManager().resolveService(
                new Intent(SERVICE_INTERFACE).setClass(this, getClass()),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);
        mManager = (TvInputManager) getSystemService(Context.TV_INPUT_SERVICE);
    }

    @Override
    public TvInputService.Session onCreateSession(String inputId) {
        if (DEBUG) Log.e(TAG, "onCreateSession");
        TvInputInfo info = mInputMap.get(inputId);
        if (info == null) {
            throw new IllegalArgumentException("Unknown inputId: " + inputId
                    + " ; this should not happen.");
        }
        HdmiInputSessionImpl session = new HdmiInputSessionImpl(this,info);
        mSessionMap.put(inputId, session);
        return session;
    }

    @Override
    public TvInputInfo onHardwareAdded(TvInputHardwareInfo hardwareInfo) {
        if (DEBUG) Log.e(TAG, "onHardwareAdded: " + hardwareInfo.toString());
        if (hardwareInfo.getType() != TvInputHardwareInfo.TV_INPUT_TYPE_HDMI) {
            return null;
        }
        int deviceId = hardwareInfo.getDeviceId();
        if (mHardwareInputIdMap.indexOfKey(deviceId) >= 0) {
            Log.e(TAG, "Already created TvInputInfo for deviceId=" + deviceId);
            return null;
        }
        int portId = hardwareInfo.getHdmiPortId();
        if (portId < 0) {
            Log.e(TAG, "Failed to get HDMI port for deviceId=" + deviceId);
            return null;
        }
        if (mHardwareIdMap.indexOfKey(portId) >= 0) {
            Log.e(TAG, "Already have port " + portId + " for deviceId=" + deviceId);
            return null;
        }
        TvInputInfo info = null;
        try {
            info = TvInputInfo.createTvInputInfo(this, mResolveInfo, hardwareInfo,
                    "HDMI " + hardwareInfo.getHdmiPortId(), null);
        } catch (XmlPullParserException | IOException e) {
            Log.e(TAG, "Error while creating TvInputInfo", e);
            return null;
        }
        mHardwareInputIdMap.put(deviceId, info.getId());
        mPortIdMap.put(info.getId(), portId);
        mInputMap.put(info.getId(), info);
        mHardwareIdMap.put(portId, deviceId);
        if (DEBUG) Log.d(TAG, "onHardwareAdded returns " + info);
        return info;
    }

    @Override
    public String onHardwareRemoved(TvInputHardwareInfo hardwareInfo) {
        int deviceId = hardwareInfo.getDeviceId();
        String inputId = mHardwareInputIdMap.get(deviceId);
        if (inputId == null) {
            if (DEBUG) Log.d(TAG, "TvInputInfo for deviceId=" + deviceId + " does not exist.");
            return null;
        }
        if (mPortIdMap.containsKey(inputId)) {
            int portId = mPortIdMap.get(inputId);
            mHardwareIdMap.delete(portId);
        } else {
            Log.w(TAG, "Port does not exists for deviceId=" + deviceId);
        }
        mHardwareInputIdMap.remove(deviceId);
        mPortIdMap.remove(inputId);
        mInputMap.remove(inputId);
        if (DEBUG) Log.d(TAG, "onHardwareRemoved returns " + inputId);
        return inputId;
    }

    private class HdmiInputSessionImpl extends TvInputService.Session{
        protected final TvInputInfo mInfo;
        protected final int mHardwareDeviceId;
        protected final int mPortId;
        private TvInputManager.Hardware mHardware;
        private TvStreamConfig[] mStreamConfigs = EMPTY_STREAM_CONFIGS;
        private boolean mWillNotifyVideoAvailable;

        HdmiInputSessionImpl(Context context, TvInputInfo info) {
            super(context);
            mInfo = info;
            mPortId = mPortIdMap.get(info.getId());
            mHardwareDeviceId = mHardwareIdMap.get(mPortId);
            acquireHardware();
        }

        private void acquireHardware() {
            if (DEBUG) Log.d(TAG, "acquireHardware");
            if (mHardware != null) {
                return;
            }
            TvInputManager.HardwareCallback callback = new TvInputManager.HardwareCallback() {
                @Override
                public void onReleased() {
                    mHardware = null;
                }

                @Override
                public void onStreamConfigChanged(TvStreamConfig[] configs) {
                    if (DEBUG) Log.d(TAG, "onStreamConfigChanged "+configs.length);
                    if (configs != null)
                        mStreamConfigs = configs;
                }
            };
            mHardware = mManager.acquireTvInputHardware(mHardwareDeviceId, mInfo, callback);
            if (DEBUG) Log.d(TAG, "acquireHardware() mHardware=" + mHardware);
        }

        @Override
        public boolean onSetSurface(Surface surface) {
            if (DEBUG) Log.d(TAG, "onSetSurface surface:" + surface);
            if (surface == null) {
                mStreamConfigs = EMPTY_STREAM_CONFIGS;
            }
            if (mHardware == null) {
                acquireHardware();
                if (mHardware == null) {
                    Log.e(TAG, "acquireHardware() failed");
                    return false;
                }
            }
            TvStreamConfig config = null;
            if (surface != null) {
                config = getStreamConfig();
                if (config == null) {
                    Log.e(TAG, "getStreamConfig() failed");
                    return false;
                }
            }
            if (DEBUG) Log.d(TAG, "calling mHardware.setSurface()");
            return mHardware.setSurface(surface, config);
        }

        @Override
        public void onRelease() {
            if (DEBUG) Log.d(TAG, "onRelease()");
            if (mHardware != null) {
                mManager.releaseTvInputHardware(mHardwareDeviceId, mHardware);
                mHardware = null;
            }
            mSessionMap.remove(mInfo.getId());
        }

        private TvStreamConfig getStreamConfig() {
            for (TvStreamConfig config : mStreamConfigs) {
                if (config.getType() == TvStreamConfig.STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE) {
                    return config;
                }
            }
            return null;
        }

        @Override
        public void onSetStreamVolume(float volume) {
            if (DEBUG) Log.d(TAG, "onSetStreamVolume volume=" + volume);
            if (mWillNotifyVideoAvailable) {
                /* I know, not the most intuitive place fo this, but when
                   launching from "am start", this is the only callback we
                   get after onTune() */
                notifyVideoAvailable();
                mWillNotifyVideoAvailable = false;
            }
        }

        @Override
        public boolean onTune(Uri channelUri) {
            if (DEBUG) Log.d(TAG, "onTune channelUri=" + channelUri);
            notifyVideoUnavailable(VIDEO_UNAVAILABLE_REASON_TUNING);
            mWillNotifyVideoAvailable = true;
            return true;
        }

        @Override
        public void onSetCaptionEnabled(boolean enabled) {
            // No-op
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE) {
                Intent i = new Intent(Intent.ACTION_MAIN);
                i.addCategory(Intent.CATEGORY_HOME);
                startActivity(i);
                return true;
            }
            return false;
        }
    }
}
