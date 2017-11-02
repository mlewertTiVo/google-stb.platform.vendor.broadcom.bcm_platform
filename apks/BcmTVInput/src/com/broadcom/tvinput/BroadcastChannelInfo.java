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

import java.util.ArrayList;

/* The raw channel data from tuner hardware */
public class BroadcastChannelInfo {
    public BroadcastChannelType type;
    public long freqKHz;
    public String mode;
    public String source;
    public int transportId;
    public int channelNumber;
    public int programNumber;
    public int pcrPid;
    public int pmtPid;
    public BroadcastChannelVideoInfo video;
    public ArrayList<BroadcastChannelAudioInfo> audio;

    public BroadcastChannelInfo(String name, long freq, int tsid, int channelNum, int programNum, int pcr_pid, int pid, BroadcastChannelVideoInfo video, ArrayList<BroadcastChannelAudioInfo> audio) {
        this.channelNumber = channelNum;
        this.programNumber = programNum;
        this.transportId = tsid;
        this.freqKHz = freq;
        this.video = video;
        this.audio = audio;
        String[] nameArg = name.split(" ");
        this.source = nameArg[0];
        this.mode = nameArg[1];
        this.pcrPid = pcr_pid;
        this.pmtPid = pid;
    }

    @Override
    public String toString() {
        return "BroadcastChannelInfo{"
                + "freqKHz=" + freqKHz
                + ", mode=" + mode
                + ", source=" + source
                + ", transportId=" + transportId
                + ", channelNumber=" + channelNumber
                + ", programNumber=" + programNumber
                + ", pcrPid=" + pcrPid
                + ", pmtPid=" + pmtPid + "}";
    }
}