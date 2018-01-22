package com.yan.ffmpeg;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.util.Size;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback {
    private String[] permissionManifest = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE
            , Manifest.permission.READ_EXTERNAL_STORAGE
            , Manifest.permission.CAMERA
    };
    private static final int REQUEST_CODE = 0X001;

    private CameraManager cameraManager;
    private SurfaceView surfaceView;
    private HandlerThread mThreadHandler;
    private Handler mHandler;
    private CaptureRequest.Builder mPreviewBuilder;
    private ImageReader mImageReader;

    public int svWidth = 640;
    public int svHeight = 360;

    private boolean isEncode;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final Button btn = findViewById(R.id.btn_r);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (v.getTag() == null) {
                    v.setTag(1);
                    String basePath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/";
                    String fileName = "test.mp4";
                    FFMPEGControl.prepareEncode(basePath, fileName, svWidth, svHeight, svWidth, svHeight, 20, 400000);
                    btn.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            isEncode = false;
                            FFMPEGControl.encodeFlagEnd();
                            Log.e("encodeFlagEnd", "run-------------------------------------------------------------");
                        }
                    }, 5000);

                    isEncode = true;
                }
            }
        });

        surfaceView = findViewById(R.id.sv);
        ViewGroup.LayoutParams params = surfaceView.getLayoutParams();
        params.width = getResources().getDisplayMetrics().widthPixels / 2;
        params.height = getResources().getDisplayMetrics().widthPixels * svWidth / svHeight / 2;
        surfaceView.getHolder().addCallback(this);

    }

    @SuppressLint("MissingPermission")
    private void stepContinue() {
        cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);

        mThreadHandler = new HandlerThread("CAMERA2");
        mThreadHandler.start();
        mHandler = new Handler(mThreadHandler.getLooper());

        try {
            //打开相机
            cameraManager.openCamera("0", mCameraDeviceStateCallback, mHandler);
        } catch (CameraAccessException e) {
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
                                        ActivityCompat.requestPermissions(MainActivity.this, permissionManifest, REQUEST_CODE);
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

    private CameraDevice.StateCallback mCameraDeviceStateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {
            try {
                startPreview(camera);
            } catch (CameraAccessException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onDisconnected(CameraDevice camera) {
            camera.close();
        }

        @Override
        public void onError(CameraDevice camera, int error) {
            camera.close();
        }
    };

    // 开始预览，主要是camera.createCaptureSession这段代码很重要，创建会话
    private void startPreview(CameraDevice mCameraDevice) throws CameraAccessException {
        try {
            String[] CameraIdList = cameraManager.getCameraIdList();
            //获取可用相机设备列表
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(CameraIdList[0]);
            //在这里可以通过CameraCharacteristics设置相机的功能,当然必须检查是否支持
            characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            //就像这样
            mPreviewBuilder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);

            StreamConfigurationMap map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            //获取所有可用的预览尺寸
            Size[] sizes = map.getOutputSizes(SurfaceHolder.class);
            for (Size s : sizes) {
                Log.e("Size", "startPreview: " + s.getWidth() + "   " + s.getHeight());
            }

        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

        //      就是在这里，通过这个set(key,value)方法，设置曝光啊，自动聚焦等参数！！ 如下举例：
//        mPreviewBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);
//        mPreviewBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
        mPreviewBuilder.set(CaptureRequest.JPEG_ORIENTATION, 90);

        mImageReader = ImageReader.newInstance(svWidth, svHeight, ImageFormat.YUV_420_888/*此处还有很多格式，比如我所用到YUV等*/, 2/*最大的图片数，mImageReader里能获取到图片数，但是实际中是2+1张图片，就是多一张*/);

        mImageReader.setOnImageAvailableListener(mOnImageAvailableListener, mHandler);
        // 这里一定分别add两个surface，一个Textureview的，一个ImageReader的，如果没add，会造成没摄像头预览，或者没有ImageReader的那个回调！！
        mPreviewBuilder.addTarget(surfaceView.getHolder().getSurface());
        mPreviewBuilder.addTarget(mImageReader.getSurface());
        mCameraDevice.createCaptureSession(Arrays.asList(surfaceView.getHolder().getSurface(), mImageReader.getSurface()), mSessionStateCallback, mHandler);
    }

    private CameraCaptureSession.StateCallback mSessionStateCallback = new CameraCaptureSession.StateCallback() {

        @Override
        public void onConfigured(CameraCaptureSession session) {
            try {
                updatePreview(session);
            } catch (CameraAccessException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onConfigureFailed(CameraCaptureSession session) {

        }
    };

    private void updatePreview(CameraCaptureSession session) throws CameraAccessException {
        session.setRepeatingRequest(mPreviewBuilder.build(), null, mHandler);
    }

    private ImageReader.OnImageAvailableListener mOnImageAvailableListener = new ImageReader.OnImageAvailableListener() {

        /**
         *  当有一张图片可用时会回调此方法，但有一点一定要注意：
         *  一定要调用 reader.acquireNextImage()和close()方法，否则画面就会卡住！！！！！我被这个坑坑了好久！！！
         *    很多人可能写Demo就在这里打一个Log，结果卡住了，或者方法不能一直被回调。
         **/
        @Override
        public void onImageAvailable(ImageReader reader) {
            final Image img = reader.acquireNextImage();
            if (img == null) {
                return;
            }
            if (isEncode) {
                ByteBuffer yBuffer = img.getPlanes()[0].getBuffer();
                ByteBuffer uBuffer = img.getPlanes()[1].getBuffer();
                ByteBuffer vBuffer = img.getPlanes()[2].getBuffer();

                int ySize = yBuffer.remaining();
                int uSize = uBuffer.remaining();
                int vSize = vBuffer.remaining();

                byte[] yuvs = new byte[ySize + uSize + vSize];
                yBuffer.get(yuvs, 0, ySize);
                vBuffer.get(yuvs, ySize, vSize);
                uBuffer.get(yuvs, ySize + vSize, uSize);

                FFMPEGControl.putOneFrame(yuvs);
            }
            img.close();
        }
    };

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
