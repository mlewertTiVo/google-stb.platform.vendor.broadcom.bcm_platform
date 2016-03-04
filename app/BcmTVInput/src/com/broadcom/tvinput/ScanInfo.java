package com.broadcom.tvinput;

import android.os.Parcelable;
import android.os.Parcel;

public class ScanInfo implements Parcelable {
    boolean busy;
    boolean valid;
    short progress;
    short channels;
    short TVChannels;
    short dataChannels;
    short radioChannels;
    short signalStrengthPercent;
    short signalQualityPercent;
    public ScanInfo() {
        busy = false;
        valid = false;
        progress = 0;
        channels = 0;
        TVChannels = 0;
        dataChannels = 0;
        radioChannels = 0;
        signalStrengthPercent = 0;
        signalQualityPercent = 0;
    }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(busy ? 1 : 0);
        out.writeInt(valid ? 1 : 0);
        out.writeInt(progress);
        out.writeInt(channels);
        out.writeInt(TVChannels);
        out.writeInt(dataChannels);
        out.writeInt(radioChannels);
        out.writeInt(signalStrengthPercent);
        out.writeInt(signalQualityPercent);
    }

    private ScanInfo(Parcel in) {
        busy = in.readInt() == 1;
        valid = in.readInt() == 1;
        progress = (short)in.readInt();
        channels = (short)in.readInt();
        TVChannels = (short)in.readInt();
        dataChannels = (short)in.readInt();
        radioChannels = (short)in.readInt();
        signalStrengthPercent = (short)in.readInt();
        signalQualityPercent = (short)in.readInt();
    }

    public static final Parcelable.Creator<ScanInfo> CREATOR
        = new Parcelable.Creator<ScanInfo>() {
            public ScanInfo createFromParcel(Parcel in) {
                return new ScanInfo(in);
            }

            public ScanInfo[] newArray(int size) {
                return new ScanInfo[size];
            }
        };

}



