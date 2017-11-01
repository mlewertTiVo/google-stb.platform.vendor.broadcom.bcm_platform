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

public class EpgSourceChannelAudioInfo {
    public int pid;
    public String language;

    public EpgSourceChannelAudioInfo(String language, String pid) {
        this.language = language;
        this.pid = pid.equals("0") ? 0 : Integer.parseInt(pid.substring(2), 16);
    }
}
