package com.broadcom.tvinput;

import android.os.Parcelable;
import android.os.Parcel;

public class ScanParams implements Parcelable {
    public enum FrontendType {
        Undefined,
        Terrestrial,
        Satellite,
        Cable
    };
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
        Sat8pskLdpc,
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
        Ofdm1k,
        Ofdm2k,
        Ofdm4k,
        Ofdm8k,
        Ofdm16k,
        Ofdm32k
    };
    public enum OfdmMode {
        Undefined,
        Auto,
        OfdmDvbt,
        OfdmDvbt2
    };
    /* All */
    public FrontendType frontendType;
    public DeliverySystem deliverySystem;
    public ScanMode scanMode;
    public int freqKHz;
    public boolean encrypted;
    /* DVB_S */
    public SatellitePolarity satellitePolarity;
    public short codeRateNumerator;
    public short codeRateDenominator;
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
        frontendType = frontendType.Undefined;
        deliverySystem = DeliverySystem.Undefined;
        scanMode = ScanMode.Undefined;
        freqKHz = 0;
        encrypted = false;
        satellitePolarity = SatellitePolarity.Undefined;
        codeRateNumerator = 0;
        codeRateDenominator = 0;
        satelliteMode = SatelliteMode.Undefined;
        symK = 0;
        qamMode = QamMode.Undefined;
        bandwidthKHz = 0;
        ofdmTransmissionMode = OfdmTransmissionMode.Undefined;
        ofdmMode = OfdmMode.Undefined;
        plpId = 0;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeSerializable(frontendType);
        out.writeSerializable(deliverySystem);
        out.writeSerializable(scanMode);
        out.writeInt(freqKHz);
        out.writeInt(encrypted ? 1 : 0);
        out.writeSerializable(satellitePolarity);
        out.writeInt(codeRateNumerator);
        out.writeInt(codeRateDenominator);
        out.writeSerializable(satelliteMode);
        out.writeInt(symK);
        out.writeSerializable(qamMode);
        out.writeInt(bandwidthKHz);
        out.writeSerializable(ofdmTransmissionMode);
        out.writeSerializable(ofdmMode);
        out.writeInt(plpId);
    }

    private ScanParams(Parcel in) {
        frontendType = (FrontendType)in.readSerializable();
        deliverySystem = (DeliverySystem)in.readSerializable();
        scanMode = (ScanMode)in.readSerializable();
        freqKHz = in.readInt();
        encrypted = in.readInt() != 0;
        satellitePolarity = (SatellitePolarity)in.readSerializable();
        codeRateNumerator = (short)in.readInt();
        codeRateDenominator = (short)in.readInt();
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
            @Override
            public ScanParams createFromParcel(Parcel in) {
                return new ScanParams(in);
            }

            @Override
            public ScanParams[] newArray(int size) {
                return new ScanParams[size];
            }
        };

}
