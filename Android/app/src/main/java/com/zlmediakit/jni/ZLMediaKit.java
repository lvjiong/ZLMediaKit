package com.zlmediakit.jni;

public class ZLMediaKit {
    static public class MediaFrame{

        /**
         * 返回解码时间戳，单位毫秒
         */
        public int dts;

        /**
         * 返回显示时间戳，单位毫秒
         */
        public int pts;

        /**
         * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
         * aac前缀则为7个字节
         */
        public int prefixSize;

        /**
         * 返回是否为关键帧
         */
        public boolean keyFrame;

        /**
         * 音视频数据
         */
        public byte[] data;

        /**
         * 是音频还是视频
         * typedef enum {
         *     TrackInvalid = -1,
         *     TrackVideo = 0,
         *     TrackAudio,
         *     TrackTitle,
         *     TrackMax = 0x7FFF
         * } TrackType;
         */
        public int trackType;


        /**
         * 编码类型
         * typedef enum {
         *     CodecInvalid = -1,
         *     CodecH264 = 0,
         *     CodecH265,
         *     CodecAAC,
         *     CodecMax = 0x7FFF
         * } CodecId;
         */
        public int codecId;
    }
	//流媒体播放
    static public interface MediaPlayerCallBack{
        void onPlayResult(int code,String msg);
        void onPlayShutdown(int code,String msg);
        void onData(MediaFrame frame);
    };


    static public class MediaPlayer{
        private long _ptr;
        private MediaPlayerCallBack _callback;
        public MediaPlayer(String url,MediaPlayerCallBack callBack){
            _callback = callBack;
            _ptr = createMediaPlayer(url,callBack);
        }
        public void release(){
            if(_ptr != 0){
                releaseMediaPlayer(_ptr);
                _ptr = 0;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
            release();
        }
    }
	//本地推流
	static public interface MediaPusherCallBack{
        void onPushResult(int code,String msg);
        void onPushShutdown(int code,String msg);
        //void onPlayUrlResult(int code,String msg);
		//void onPlayUrlShutdown(int code,String msg);
    };
	
	static public class MediaPusher{
        private long _ptr;
        private MediaPusherCallBack _callback;
        public MediaPusher(String filePath,String pushUrl,MediaPusherCallBack callBack){
            _callback = callBack;
            _ptr = createMediaPusher(filePath,pushUrl,callBack);
        }
        public void release(){
            if(_ptr != 0){
                releaseMediaPusher(_ptr);
                _ptr = 0;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
            release();
        }
    }
	//直播流代理
	static public interface MediaProxyCallBack{
        //void onPushResult(int code,String msg);
        //void onPushShutdown(int code,String msg);
        void onProxyResult(int code,String msg);
		void onProxyShutdown(int code,String msg);
    };
	
	static public class MediaProxy{
        private long _ptr;
        private MediaProxyCallBack _callback;
        public MediaProxy(String pollUrl,String pushUrl,MediaProxyCallBack callBack){
            _callback = callBack;
            _ptr = createMediaProxy(pollUrl,pushUrl,callBack);
        }
        public void release(){
            if(_ptr != 0){
                releaseMediaProxy(_ptr);
                _ptr = 0;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            super.finalize();
            release();
        }
    }
	
    static public native boolean startDemo(String sd_path);
    static public native void releaseMediaPlayer(long ptr);
    static public native long createMediaPlayer(String url,MediaPlayerCallBack callback);
	static public native void releaseMediaPusher(long ptr);
    static public native long createMediaPusher(String filePath,String pushUrl,MediaPusherCallBack callback);
	static public native void releaseMediaProxy(long ptr);
    static public native long createMediaProxy(String pollUrl,String pushUrl,MediaProxyCallBack callback);

    static {
        System.loadLibrary("zlmediakit_jni");
    }
}
