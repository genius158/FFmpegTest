package com.yan.ffmpeg;

import android.view.Surface;

/**
 * Created by yan on 2018/1/18 0018
 */

public class FFMPEGControl {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("avutil");
        System.loadLibrary("avcodec");
        System.loadLibrary("avformat");
        System.loadLibrary("avfilter");
        System.loadLibrary("fdk-aac");
//        System.loadLibrary("avdevice");
        System.loadLibrary("swscale");
        System.loadLibrary("swresample");
        System.loadLibrary("native-lib");
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String configurationInfo();

    public native int renderFrame(String input, Surface surface);

    public native int decode(String input, String output);

    public native int ffmpegRun(String[] commends);

    public native int addOverlay(String videoPath, String impPath, String outputPath);

    public native int videoEncodeTest(String input,String output,String mp4File);

    // -------------------------- line --------------------------

    public native int prepareEncode(String mediaFullPath, int filter,int inWidth, int inHeight, int outWidth, int  outHeight, int frameRate, long bitRate);

    public native int videoEncode(String[] commends);

}
