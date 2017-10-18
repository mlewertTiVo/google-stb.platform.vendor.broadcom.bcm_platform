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

public enum BroadcastChannelType {
    TYPE_OTHER("other"),
    TYPE_DVB_T("dvbt"),
    TYPE_DVB_T2("dvbt2"),
    TYPE_DVB_S("dvbs"),
    TYPE_DVB_S2("dvbs2"),
    TYPE_DVB_C("dvbc"),
    TYPE_DVB_C2("dvbc2"),
    TYPE_DVB_H("dvbh"),
    TYPE_DVB_SH("dvbsh"),
    TYPE_ATSC_T("atsct"),
    TYPE_ATSC_C("atscc"),
    TYPE_ATSC_M_H("stscmh"),
    TYPE_ISDB_T("isdbt"),
    TYPE_ISDB_TB("isdbtb"),
    TYPE_ISDB_S("isdbs"),
    TYPE_ISDB_C("isdbc"),
    TYPE_1SEG("seg"),
    TYPE_DTMB("dtmb"),
    TYPE_CMMB("cmmb"),
    TYPE_T_DMB("dmbt"),
    TYPE_S_DMB("dmbs");

    private String name;

    private BroadcastChannelType(String name){
        this.name = name;
    }

    @Override
    public String toString() {
        return this.name;
    }
}