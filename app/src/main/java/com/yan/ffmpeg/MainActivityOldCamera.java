package com.yan.ffmpeg;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;

import java.io.File;
import java.io.IOException;

public class MainActivityOldCamera extends AppCompatActivity implements SurfaceHolder.Callback, Camera.PreviewCallback {
    private String[] permissionManifest = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE
            , Manifest.permission.READ_EXTERNAL_STORAGE
            , Manifest.permission.CAMERA
    };
    private static final int REQUEST_CODE = 0X001;

    private SurfaceView surfaceView;

    public int svWidth = 1280;
    public int svHeight = 720;

    private boolean isEncode;

    Camera camera;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final Button btn =(Button) findViewById(R.id.btn_r);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (v.getTag() == null) {
                    v.setTag(1);
                    String basePath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/";
                    String fileName = "test.mp4";
                    File mp4file = new File(basePath + fileName);
                    if (mp4file.exists()) {
                        mp4file.delete();
                    }
                    FFMPEGControl.prepareEncode(basePath, fileName,  svWidth, svHeight, svWidth, svHeight, 25, 400000);
                    btn.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            isEncode = false;
                            FFMPEGControl.encodeFlagEnd();
                        }
                    }, 5000);

                    isEncode = true;
                }
            }
        });

        surfaceView = (SurfaceView) findViewById(R.id.sv);
        surfaceView.getHolder().addCallback(this);

    }

    @SuppressLint("MissingPermission")
    private void stepContinue() {
        camera = Camera.open();
        camera.setDisplayOrientation(90);
        try {
            Camera.Parameters mParameters = camera.getParameters();
            mParameters.setPreviewSize(svWidth, svHeight);
            mParameters.setPreviewFormat(ImageFormat.YV12);
            camera.setParameters(mParameters);
            camera.setPreviewDisplay(surfaceView.getHolder());
            camera.startPreview();
            camera.setPreviewCallback(this);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }


    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE) {
            for (int i = 0; i < permissions.length; i++) {
                if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                    if (shouldShowRequestPermissionRationale(permissions[i])) {
                        new AlertDialog.Builder(this)
                                .setMessage("need permission to continue")
                                .setPositiveButton("sure", new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        ActivityCompat.requestPermissions(MainActivityOldCamera.this, permissionManifest, REQUEST_CODE);
                                    }
                                }).show();
                    }
                    return;
                }
            }
            stepContinue();
        }
    }

    @Override
    public boolean shouldShowRequestPermissionRationale(@NonNull String permission) {
        return permission.equals(permissionManifest[0]) || permission.equals(permissionManifest[1]) || super.shouldShowRequestPermissionRationale(permission);
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        camera.stopPreview();
        camera.setPreviewCallback(null);
        camera.release();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (Build.VERSION.SDK_INT > 23) {
            for (String permission : permissionManifest) {
                if (ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_DENIED) {
                    ActivityCompat.requestPermissions(this, permissionManifest, REQUEST_CODE);
                    return;
                }
            }
        }
        stepContinue();
    }



    int count = 0;

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (isEncode) {
            Log.e("count", "onPreviewFrame: " + count++);
            FFMPEGControl.putOneFrame(data);
        }
        camera.addCallbackBuffer(data);
    }




    void test() {
        //        new Thread() {
//            @Override
//            public void run() {
//                String inputVideo = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "input.mp4";
//                String output = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "output.yuv";
////                ffmpegControl.decode(inputVideo, output);
//
//                String encodeH264Path = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "input.h264";
//                String encodeMp4Path = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "input1.mp4";
//
//                File mp4File = new File(encodeMp4Path);
//                if (mp4File.exists()) {
//                    mp4File.delete();
//                }
//                ffmpegControl.videoEncodeTest(output, encodeH264Path, encodeMp4Path);
//
//                // ffmpeg -i sample.h264 output.mp4
////                String toMp4 = "ffmpeg -i " + encodeH264Path + " -c:v copy " + encodeMp4Path;
////                ffmpegControl.ffmpegRun(toMp4.split(" "));
//            }
//        }.start();
    }
}
