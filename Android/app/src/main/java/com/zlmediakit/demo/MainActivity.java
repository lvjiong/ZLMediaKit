package com.zlmediakit.demo;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;


import java.util.concurrent.TimeUnit;

import com.zlmediakit.jni.ZLMediaKit;

public class MainActivity extends AppCompatActivity {
    public static final String TAG = "ZLMediaKit";
    private static String[] PERMISSIONS_STORAGE = {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE",
            "android.permission.INTERNET"};
			
			
	private RtspPushThread mRtspPush;	
	private RtspPlayThread mRtspPlay;
	private class RtspPushThread extends Thread {
		private int sleepTime;
		public RtspPushThread(int sleepTime){
			this.sleepTime = sleepTime;
		}
        @Override
        public void run() {
            super.run();
            try {
				  if(sleepTime > 0){
					  Thread.sleep(sleepTime);
				  }
		          test_pusher();
            } catch (Exception e) {
	 	          e.printStackTrace();
            }
        }
    }
	private class RtspPlayThread extends Thread {
		private int sleepTime;
		public RtspPlayThread(int sleepTime){
			this.sleepTime = sleepTime;
		}
        @Override
        public void run() {
            super.run();
            try {
                  if(sleepTime > 0){
					  Thread.sleep(sleepTime);
				  }
                  test_player();
            } catch (Exception e) {
                  e.printStackTrace();
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        boolean permissionSuccess = true;
        for(String str : PERMISSIONS_STORAGE){
            int permission = ActivityCompat.checkSelfPermission(this, str);
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE,1);
                permissionSuccess = false;
                break;
            }
        }

        String sd_dir = Environment.getExternalStoragePublicDirectory("").toString();
        if(permissionSuccess){
            Toast.makeText(this,"你可以修改配置文件再启动：" + sd_dir + "/zlmediakit.ini" ,Toast.LENGTH_LONG).show();
            Toast.makeText(this,"SSL证书请放置在：" + sd_dir + "/zlmediakit.pem" ,Toast.LENGTH_LONG).show();
        }else{
            Toast.makeText(this,"请给予我权限，否则无法启动测试！" ,Toast.LENGTH_LONG).show();
        }
        ZLMediaKit.startDemo(sd_dir);
		// try{
			// Toast.makeText(this,"等待5秒，客户端推流后再拉流播放!",Toast.LENGTH_LONG).show();
			// TimeUnit.SECONDS.sleep(5);
			// test_pusher();
		// } catch (InterruptedException e) {
			// System.err.format("IOException: %s%n", e);
		// }
		// try{
			// TimeUnit.SECONDS.sleep(2);
			// test_player();
		// } catch (InterruptedException e) {
			// System.err.format("IOException: %s%n", e);
		// }
		mRtspPush = new RtspPushThread(2000);
		mRtspPush.start();
		
		mRtspPlay = new RtspPlayThread(5000);
		mRtspPlay.start();
		test_proxy();
    }

    private ZLMediaKit.MediaPlayer _player;
    private void test_player(){
        _player = new ZLMediaKit.MediaPlayer("rtsp://10.138.48.55:8554/live/mystream", new ZLMediaKit.MediaPlayerCallBack() {
            @Override
            public void onPlayResult(int code, String msg) {
                Log.d(TAG,"onPlayResult:" + code + "," + msg);
            }

            @Override
            public void onPlayShutdown(int code, String msg) {
                Log.d(TAG,"onPlayShutdown:" + code + "," + msg);
            }

            @Override
            public void onData(ZLMediaKit.MediaFrame frame) {
                Log.d(TAG,"onData:"
                        + frame.trackType + ","
                        + frame.codecId + ","
                        + frame.dts + ","
                        + frame.pts + ","
                        + frame.keyFrame + ","
                        + frame.prefixSize + ","
                        + frame.data.length);
            }
        });
    }
	private ZLMediaKit.MediaPusher mp4_pusher;
    private void test_pusher(){
        mp4_pusher = new ZLMediaKit.MediaPusher("/sdcard/test.mp4","rtsp://10.138.48.55:8554/live/mystream", new ZLMediaKit.MediaPusherCallBack() {
            @Override
            public void onPushResult(int code, String msg) {
                Log.d(TAG,"onMp4PushResult:" + code + "," + msg);
            }

            @Override
            public void onPushShutdown(int code, String msg) {
                Log.d(TAG,"onUrlPushShutdown:" + code + "," + msg);
				//Log.d(TAG,"onPushShutdown:重新发布");
				//mRtspPush = new RtspPushThread(0);
				//mRtspPush.start();
            }

        });
    }
	private ZLMediaKit.MediaProxy proxyer;
	private void test_proxy(){
			proxyer = new ZLMediaKit.MediaProxy("rtsp://10.138.80.2:8554/live","rtsp://10.138.48.55:8554/live/mystream", new ZLMediaKit.MediaProxyCallBack() {
			@Override
            public void onProxyResult(int code, String msg) {
                Log.d(TAG,"onProxyResult:" + code + "," + msg);
            }
			@Override
            public void onProxyShutdown(int code, String msg) {
                Log.d(TAG,"onProxyShutdown:" + code + "," + msg);
            }
			});
	}
}
