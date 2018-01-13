package com.yan.ffmpeg;

import android.Manifest;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceView;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    String[] permissionManifest = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE
            , Manifest.permission.READ_EXTERNAL_STORAGE
    };
    private static final int REQUEST_CODE = 0X001;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("avutil");
        System.loadLibrary("avcodec");
        System.loadLibrary("avformat");
        System.loadLibrary("avfilter");
        System.loadLibrary("avdevice");
        System.loadLibrary("swscale");
        System.loadLibrary("swresample");
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(configurationInfo());


        if (Build.VERSION.SDK_INT > 23) {
            for (String permission : permissionManifest) {
                if (ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_DENIED) {
                    ActivityCompat.requestPermissions(this, permissionManifest, REQUEST_CODE);
                    return;
                }
            }
        }
        render();
    }

    private void render() {
        new Thread() {
            @Override
            public void run() {
                String inputVideo = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "input.mp4";
                String output = Environment.getExternalStorageDirectory().getAbsolutePath() + File.separatorChar + "output.yuv";
                decode(inputVideo, output);

                //        SurfaceView surfaceView = findViewById(R.id.sv);
                //        renderFrame(inputVideo, surfaceView.getHolder().getSurface());
            }
        }.start();

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
                                        ActivityCompat.requestPermissions(MainActivity.this, permissionManifest, REQUEST_CODE);
                                    }
                                }).show();
                    }
                    return;
                }
            }
            render();
        }
    }

    @Override
    public boolean shouldShowRequestPermissionRationale(@NonNull String permission) {
        return permission.equals(permissionManifest[0]) || permission.equals(permissionManifest[1]) || super.shouldShowRequestPermissionRationale(permission);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String configurationInfo();

    public native int renderFrame(String input, Surface surface);

    public native int decode(String input, String output);
}
