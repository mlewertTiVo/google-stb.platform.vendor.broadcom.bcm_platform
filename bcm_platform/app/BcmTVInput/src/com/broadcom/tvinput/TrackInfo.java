package com.broadcom.tvinput;

public class TrackInfo {
    // All
    public short type;
    public String id;
    // Audio (0) + Subtitles (2)
    public String lang;
    // Video (1) 
    public short squarePixelWidth;
    public short squarePixelHeight;
    public float frameRate;
    // Audio (0)
    public short channels;
    public int sampleRate;
    public TrackInfo() {
        type = 0;
        id = "";
        lang = "";
        squarePixelWidth = 0;
        squarePixelHeight = 0;
        frameRate = 0;
        channels = 0;
        sampleRate = 0;
    }
}



