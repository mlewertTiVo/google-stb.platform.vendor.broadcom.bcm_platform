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

/* The raw channel data from an epg source */
public class EpgSourceChannelInfo {
    public String id;
    public BroadcastChannelType type;
    public String number;
    public String name;
    public int onid;
    public int tsid;
    public int sid;
    public long freqKHz;
    public int vpid;
    public ArrayList<EpgSourceChannelAudioInfo> audio;
    public String logoUrl;

    public EpgSourceChannelInfo(int id, String type, String number, String name, String onid, String tsid, String sid, long freqKHz, String vpid, ArrayList<EpgSourceChannelAudioInfo> audio, String logoUrl) {
        this.id = String.valueOf(id);
        this.type = BroadcastChannelType.valueOf(type);
        this.number = number;
        this.name = name;
        this.onid = onid.equals("0") ? 0 : Integer.parseInt(onid.substring(2), 16);
        this.tsid = tsid.equals("0") ? 0 : Integer.parseInt(tsid.substring(2), 16);
        this.sid = sid.equals("0") ? 0 : Integer.parseInt(sid.substring(2), 16);
        this.freqKHz = freqKHz;
        this.vpid = vpid.equals("0") ? 0 : Integer.parseInt(vpid.substring(2), 16);
        this.audio = audio;
    }

    @Override
    public String toString() {
        return "EpgSourceChannelInfo{"
                + "id=" + id
                + ", type=" + type
                + ", number=" + number
                + ", name=" + name
                + ", onid=" + onid
                + ", tsid=" + tsid
                + ", sid=" + sid
                + ", freqKHz=" + freqKHz
                + ", logoUrl=" + logoUrl + "}";
    }
}
