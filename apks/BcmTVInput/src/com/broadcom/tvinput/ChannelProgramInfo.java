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

public class ChannelProgramInfo {
    public String channelNumber; //COLUMN_DISPLAY_NUMBER, The channel this program belongs to
    public String icon; // (android:icon in the TV input's manifest)
    public String description; //(COLUMN_SHORT_DESCRIPTION, Program description
    public String title; //COLUMN_TITLE, Program title
    public String logo; //Channel logo (TvContract.Channels.Logo), Use the color #EEEEEE to match the surrounding text, Don't include padding
    public String poster; //COLUMN_POSTER_ART_URI, Poster art, Aspect ratio between 16:9 and 4:3

    //App link data is optional
    public String color; //COLUMN_APP_LINK_COLOR - The accent color of the app link for this channel
    public String iconUri; //COLUMN_APP_LINK_ICON_URI - The URI for the app badge icon of the app link for this channel
    public String intent; //COLUMN_APP_LINK_INTENT_URI - The intent URI of the app link for this channel. You can create the URI using toUri(int) with URI_INTENT_SCHEME and convert the URI back to the original intent with parseUri().
    public String posterArt; //COLUMN_APP_LINK_POSTER_ART_URI - The URI for the poster art used as the background of the app link for this channel
    public String text; //COLUMN_APP_LINK_TEXT - The descriptive link text of the app link for this channel
}
