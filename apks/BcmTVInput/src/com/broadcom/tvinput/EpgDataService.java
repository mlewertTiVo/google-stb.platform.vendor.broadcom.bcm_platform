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

import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;
import java.util.List;

public class EpgDataService {

    public static String rawEpgChannelData = "[[0,\"TYPE_DVB_T\",\"8\",\"8madrid\",\"0x22d4\",\"0x0027\",\"0x0f3d\",618000000,\"0x100\",[[\"0x101\",\"spa\"],[\"0x102\",\"eng\"]],\"http://static.programacion-tdt.com/imgAPP/8madrid.min.png\"],[1,\"TYPE_DVB_T\",\"13\",\"13tvMadrid\",\"0x22d4\",\"0x0027\",\"0x0f3e\",618000000,\"0x200\",[[\"0x201\",\"spa\"],[\"0x202\",\"eng\"]],\"http://static.programacion-tdt.com/imgAPP/13_TV.min.png\"],[2,\"TYPE_DVB_T\",\"800\",\"ASTROCANALSHOP\",\"0x22d4\",\"0x0027\",\"0x0f43\",577000,\"0x700\",[[\"0x701\",\"spa\"]],\"\"],[3,\"TYPE_DVB_T\",\"801\",\"KissTV\",\"0x22d4\",\"0x0027\",\"0x0f40\",618000000,\"0x401\",[[\"0x400\",\"spa\"]],\"http://www.ranklogos.com/wp-content/uploads/2012/04/kiss-tv-logo-1.jpg\"],[4,\"TYPE_DVB_T\",\"802\",\"INTERTV\",\"0x22d4\",\"0x0027\",\"0x0f3f\",618000000,\"0x300\",[[\"0x301\",\"spa\"]],\"\"],[5,\"TYPE_DVB_T\",\"803\",\"MGustaTV\",\"0x22d4\",\"0x0027\",\"0x1392\",618000000,\"0x1000\",[[\"0x1001\",\"spa\"]],\"\"],[-1,\"TYPE_OTHER\",\"\",\"\",\"0\",\"0\",\"0\",0,\"0\",[],\"\"]]";
    public static String rawTunerChannelData = "[{\"name\":\"ofdm dvbt 618Mhz\",\"freq\":618000000,\"version\":20,\"tsid\":39,\"program_list\":[{\"channel_number\":0,\"program_number\":3901,\"program_pid\":272,\"program_version\":10,\"pcr_pid\":256,\"ca_pid\":0,\"video_pids\":[{\"pid\":256,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":257,\"codec\":\"mpeg\",\"ca_pid\":0},{\"pid\":258,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":1,\"program_number\":3902,\"program_pid\":528,\"program_version\":10,\"pcr_pid\":512,\"ca_pid\":0,\"video_pids\":[{\"pid\":512,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":513,\"codec\":\"mpeg\",\"ca_pid\":0},{\"pid\":514,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":2,\"program_number\":3903,\"program_pid\":784,\"program_version\":18,\"pcr_pid\":768,\"ca_pid\":0,\"video_pids\":[{\"pid\":768,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":769,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":3,\"program_number\":3904,\"program_pid\":1040,\"program_version\":10,\"pcr_pid\":1025,\"ca_pid\":0,\"video_pids\":[{\"pid\":1025,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":1024,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":4,\"program_number\":3905,\"program_pid\":1296,\"program_version\":9,\"pcr_pid\":1281,\"ca_pid\":0,\"video_pids\":[],\"audio_pids\":[{\"pid\":1281,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":5,\"program_number\":3906,\"program_pid\":1552,\"program_version\":10,\"pcr_pid\":1536,\"ca_pid\":0,\"video_pids\":[],\"audio_pids\":[{\"pid\":1536,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":6,\"program_number\":3907,\"program_pid\":1808,\"program_version\":10,\"pcr_pid\":1792,\"ca_pid\":0,\"video_pids\":[{\"pid\":1792,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":1793,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":7,\"program_number\":3908,\"program_pid\":1072,\"program_version\":9,\"pcr_pid\":1056,\"ca_pid\":0,\"video_pids\":[],\"audio_pids\":[{\"pid\":1056,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":8,\"program_number\":3909,\"program_pid\":1328,\"program_version\":9,\"pcr_pid\":1312,\"ca_pid\":0,\"video_pids\":[],\"audio_pids\":[{\"pid\":1312,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]},{\"channel_number\":9,\"program_number\":5010,\"program_pid\":4112,\"program_version\":9,\"pcr_pid\":4096,\"ca_pid\":0,\"video_pids\":[{\"pid\":4096,\"codec\":\"mpeg2\",\"ca_pid\":0}],\"audio_pids\":[{\"pid\":4097,\"codec\":\"mpeg\",\"ca_pid\":0}],\"other_pids\":[]}]}]";

    public static ArrayList<EpgSourceChannelInfo> getEpgChannelData() {

        ArrayList<EpgSourceChannelInfo> infoList = new ArrayList<>(7);
        try {
            JSONArray jsonEpgData = new JSONArray(EpgDataService.rawEpgChannelData);
            int bSize = jsonEpgData.length();

            for (int i = 0; i < bSize; i++) {
                JSONArray channelInfo = jsonEpgData.getJSONArray(i);
                JSONArray audioInfo = channelInfo.getJSONArray(9);
                ArrayList<EpgSourceChannelAudioInfo> audioList = new ArrayList<>();
                int audioInfoLength = audioInfo.length();

                for (int j = 0; j < audioInfoLength; j++) {
                    audioList.add(new EpgSourceChannelAudioInfo(audioInfo.getJSONArray(j).getString(1), audioInfo.getJSONArray(j).getString(0)));
                }
                EpgSourceChannelInfo info = new EpgSourceChannelInfo(channelInfo.getInt(0),
                        channelInfo.getString(1),
                        channelInfo.getString(2),
                        channelInfo.getString(3),
                        channelInfo.getString(4),
                        channelInfo.getString(5),
                        channelInfo.getString(6),
                        channelInfo.getInt(7),
                        channelInfo.getString(8),
                        audioList,
                        channelInfo.getString(10));
                infoList.add(info);
            }

        } catch (JSONException e) {
            e.printStackTrace();
        }

        return infoList;
    }

    public static List<Program> getEpgProgramsData(Channel channel, long startMs, long endMs) {
        List<Program> emptyProgram = new ArrayList<>();
        emptyProgram.add(new Program.Builder()
                .setTitle("Tv Program")
                .setStartTimeUtcMillis(0)
                .setEndTimeUtcMillis(1800000)
                .setDescription("Sample program description")
                .setPosterArtUri("https://storage.googleapis.com/gtv-videos-bucket/sample/images/tears.jpg")
                .setThumbnailUri("https://storage.googleapis.com/gtv-videos-bucket/sample/images/tears.jpg")
                .build());
        return emptyProgram;
    }
}
