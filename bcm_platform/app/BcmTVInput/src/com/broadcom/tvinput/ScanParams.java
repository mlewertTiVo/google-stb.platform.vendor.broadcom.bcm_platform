package com.broadcom.tvinput;

import android.os.Parcelable;
import android.os.Parcel;

public class ScanParams implements Parcelable {
    public enum DeliverySystem {
        Undefined,
        Dvbt,
        Dvbs,
        Dvbc
    };
    public enum ScanMode {
        Undefined,
        Blind,
        Single,
        Home
    };
    public enum SatellitePolarity {
        Undefined,
        Horizontal,
        Vertical
    };
    public enum SatelliteMode {
        Undefined,
        Auto,
        SatQpskLdpc,
        Sat8pkLdpc,
        SatDvb
    };
    public enum QamMode {
        Undefined,
        Auto,
        Qam16,
        Qam32,
        Qam64,
        Qam128,
        Qam256
    };
    public enum OfdmTransmissionMode {
        Undefined,
        Auto,
        Ofdm_1k,
        Ofdm_2k,
        Ofdm_4k,
        Ofdm_8k,
        Ofdm_16k,
        Ofdm_32k
    };
    public enum OfdmMode {
        Undefined,
        Auto,
        Dvbt,
        Dvbt2
    };
    public class CodeRate {
        public short numerator;
        public short denominator;
    };
    /* All */
    public DeliverySystem deliverySystem;
    public ScanMode mode;
    public int freqKHz;
    /* DVB_S */
    public SatellitePolarity polarity;
    public CodeRate codeRate;
    public SatelliteMode satelliteMode;
    /* DVB_C and DVB_S */
    public int symK;
    /* DVB_C */
    public QamMode qamMode;
    /* DVB-T */
    public int bandwidthKHz;
    public OfdmTransmissionMode ofdmTransmissionMode;
    public OfdmMode ofdmMode;
    public short plpId;
    public ScanParams() {
        deliverySystem = DeliverySystem.Undefined;
        mode = ScanMode.Undefined;
        freqKHz = 0;
        polarity = SatellitePolarity.Undefined;
        codeRate = new CodeRate(); codeRate.numerator = 0; codeRate.denominator = 0;
        satelliteMode = SatelliteMode.Undefined;
        symK = 0;
        qamMode = QamMode.Undefined;
        bandwidthKHz = 0;
        ofdmTransmissionMode = OfdmTransmissionMode.Undefined;
        ofdmMode = OfdmMode.Undefined;
        plpId = 0;
    }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel out, int flags) {
        out.writeSerializable(deliverySystem);
        out.writeSerializable(mode);
        out.writeInt(freqKHz);
        out.writeSerializable(polarity);
        out.writeInt(codeRate.numerator); out.writeInt(codeRate.denominator);
        out.writeSerializable(satelliteMode);
        out.writeInt(symK);
        out.writeSerializable(qamMode);
        out.writeInt(bandwidthKHz);
        out.writeSerializable(ofdmTransmissionMode);
        out.writeSerializable(ofdmMode);
        out.writeInt(plpId);
    }

    private ScanParams(Parcel in) {
        deliverySystem = (DeliverySystem)in.readSerializable();
        mode = (ScanMode)in.readSerializable();
        freqKHz = in.readInt();
        polarity = (SatellitePolarity)in.readSerializable();
        codeRate = new CodeRate(); codeRate.numerator = (short)in.readInt(); codeRate.denominator = (short)in.readInt();
        satelliteMode = (SatelliteMode)in.readSerializable();
        symK = in.readInt();
        qamMode = (QamMode)in.readSerializable();
        bandwidthKHz = in.readInt();
        ofdmTransmissionMode = (OfdmTransmissionMode)in.readSerializable();
        ofdmMode = (OfdmMode)in.readSerializable();
        plpId = (short)in.readInt();
   }

    public static final Parcelable.Creator<ScanParams> CREATOR
        = new Parcelable.Creator<ScanParams>() {
            public ScanParams createFromParcel(Parcel in) {
                return new ScanParams(in);
            }

            public ScanParams[] newArray(int size) {
                return new ScanParams[size];
            }
        };

}



