/*
 * Copyright 2016-2017 The Android Open Source Project
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
package com.broadcom.tvinput;

import android.content.Context;
import android.media.tv.TvContract;
import android.net.Uri;
import android.os.AsyncTask;
import android.util.Log;
import android.util.LongSparseArray;

import java.util.List;

public class EpgController {
    public static String BCM_TVINPUT_ID = "com.broadcom.tvinput/.BcmTvTunerService";
    public Context mContext;
    public String tunerChannelData;

    public EpgController(Context context) {
        this.mContext = context;
    }

    public void updateEpgData(String tunerChannelData){
        this.tunerChannelData = tunerChannelData;
        UpdateChannelAndProgramTask job = new UpdateChannelAndProgramTask(mContext);
        job.execute();
    }

    private class UpdateChannelAndProgramTask extends AsyncTask<Void, Void, Void> {
        Context context;

        public UpdateChannelAndProgramTask(Context c) {
            context = c;
        }

        @Override
        public Void doInBackground(Void... params) {
            //Update Channels
            List<Channel> tvChannels = EpgDataUtil.getChannelsFromEpgSource(tunerChannelData);
            EpgDataUtil.updateChannels(context, EpgController.BCM_TVINPUT_ID, tvChannels);
            LongSparseArray<Channel> channelMap = EpgDataUtil.buildChannelMap(context.getContentResolver(), EpgController.BCM_TVINPUT_ID);

            if (channelMap == null) {
                return null;
            }
            //Update programs
            long startMs = System.currentTimeMillis();
            long endMs = startMs + 7200000;

            for (int i = 0; i < channelMap.size(); ++i) {
                Uri channelUri = TvContract.buildChannelUri(channelMap.keyAt(i));
                List<Program> programs = EpgDataService.getEpgProgramsData(channelMap.valueAt(i), startMs, endMs);

                for (int index = 0; index < programs.size(); index++) {
                    if (programs.get(index).getChannelId() == -1) {
                        // Automatically set the channel id if not set
                        programs.set(index, new Program.Builder(programs.get(index)).setChannelId(channelMap.valueAt(i).getId()).build());
                    }
                }

                if (BcmTvTunerService.DEBUG) Log.d(BcmTvTunerService.TAG, programs.toString());
                EpgDataUtil.updatePrograms(context.getContentResolver(), channelUri, EpgDataUtil.getPrograms(channelMap.valueAt(i), programs, startMs, endMs));
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            EpgDataUtil.dumpChanneAndProgramlDb(mContext.getContentResolver());
        }
    }
}
