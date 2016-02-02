package com.broadcom.tvinput;

public class ProgramUpdateInfo {

    public enum UpdateType {
        CLEAR_ALL,
        CLEAR_CHANNEL,
        DELETE,
        ADD,
        UPDATE,
        EXPIRE
    }

    public UpdateType type;
    public String id;
    public String channel_id;
    public String title;
    public String short_description;
    public long start_time_utc_millis;
    public long end_time_utc_millis;
    public ProgramUpdateInfo() {
        type = UpdateType.CLEAR_ALL;
        id = "";
        channel_id = "";
        title = "";
        short_description = "";
        start_time_utc_millis = 0;
        end_time_utc_millis = 0;
    }
}



