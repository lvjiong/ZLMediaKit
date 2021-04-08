/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>
#include <string>
#include "Util/logger.h"
#include "Thread/semaphore.h"
#include "Common/config.h"
#include "Player/MediaPlayer.h"
#include "Extension/Frame.h"
//add by lvjiong for MediaPusher
#include "Pusher/MediaPusher.h"
#include "Player/PlayerProxy.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#define JNI_API(retType,funName,...) extern "C"  JNIEXPORT retType Java_com_zlmediakit_jni_ZLMediaKit_##funName(JNIEnv* env, jclass cls,##__VA_ARGS__)
#define MediaPlayerCallBackSign "com/zlmediakit/jni/ZLMediaKit$MediaPlayerCallBack"
#define MediaFrameSign "com/zlmediakit/jni/ZLMediaKit$MediaFrame"
#define MediaPusherCallBackSign "com/zlmediakit/jni/ZLMediaKit$MediaPusherCallBack"


string stringFromJstring(JNIEnv *env,jstring jstr){
    if(!env || !jstr){
        WarnL << "invalid args";
        return "";
    }
    const char *field_char = env->GetStringUTFChars(jstr, 0);
    string ret(field_char,env->GetStringUTFLength(jstr));
    env->ReleaseStringUTFChars(jstr, field_char);
    return ret;
}
string stringFromJbytes(JNIEnv *env,jbyteArray jbytes){
    if(!env || !jbytes){
        WarnL << "invalid args";
        return "";
    }
    jbyte *bytes = env->GetByteArrayElements(jbytes, 0);
    string ret((char *)bytes,env->GetArrayLength(jbytes));
    env->ReleaseByteArrayElements(jbytes,bytes,0);
    return ret;
}
string stringFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jstring field_str = (jstring)env->GetObjectField(jdata,jid);
    auto ret = stringFromJstring(env,field_str);
    env->DeleteLocalRef(field_str);
    return ret;
}

string bytesFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jbyteArray jbufArray = (jbyteArray)env->GetObjectField(jdata, jid);
    string ret = stringFromJbytes(env,jbufArray);
    env->DeleteLocalRef(jbufArray);
    return ret;
}

jstring jstringFromString(JNIEnv* env, const char* pat) {
    return (jstring)env->NewStringUTF(pat);
}

jbyteArray jbyteArrayFromString(JNIEnv* env, const char* pat,int len = 0){
    if(len <= 0){
        len = strlen(pat);
    }
    jbyteArray jarray = env->NewByteArray(len);
    env->SetByteArrayRegion(jarray, 0, len, (jbyte *)(pat));
    return jarray;
}

jobject makeJavaFrame(JNIEnv* env,const Frame::Ptr &frame){
    static jclass jclass_obj = (jclass)env->NewGlobalRef(env->FindClass(MediaFrameSign));
    static jmethodID jmethodID_init = env->GetMethodID(jclass_obj, "<init>", "()V");
    static jfieldID jfieldID_dts = env->GetFieldID(jclass_obj,"dts","I");
    static jfieldID jfieldID_pts = env->GetFieldID(jclass_obj,"pts","I");
    static jfieldID jfieldID_prefixSize = env->GetFieldID(jclass_obj,"prefixSize","I");
    static jfieldID jfieldID_keyFrame = env->GetFieldID(jclass_obj,"keyFrame","Z");
    static jfieldID jfieldID_data = env->GetFieldID(jclass_obj,"data","[B");
    static jfieldID jfieldID_trackType = env->GetFieldID(jclass_obj,"trackType","I");
    static jfieldID jfieldID_codecId = env->GetFieldID(jclass_obj,"codecId","I");

    if(!frame){
        return nullptr;
    }
    jobject ret = env->NewObject(jclass_obj, jmethodID_init);
    env->SetIntField(ret,jfieldID_dts,frame->dts());
    env->SetIntField(ret,jfieldID_pts,frame->pts());
    env->SetIntField(ret,jfieldID_prefixSize,frame->prefixSize());
    env->SetBooleanField(ret,jfieldID_keyFrame,frame->keyFrame());
    env->SetObjectField(ret,jfieldID_data,jbyteArrayFromString(env,frame->data(),frame->size()));
    env->SetIntField(ret,jfieldID_trackType,frame->getTrackType());
    env->SetIntField(ret,jfieldID_codecId,frame->getCodecId());
    return ret;
}

static JavaVM *s_jvm = nullptr;

template <typename FUN>
void doInJavaThread(FUN &&fun){
    JNIEnv *env;
    int status = s_jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        if (s_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
            return;
        }
    }
    fun(env);
    if (status != JNI_OK) {
        //Detach线程
        s_jvm->DetachCurrentThread();
    }
}

#define emitEvent(delegate,method,argFmt,...) \
{ \
    doInJavaThread([&](JNIEnv* env) { \
        static jclass cls = env->GetObjectClass(delegate); \
        static jmethodID jmid = env->GetMethodID(cls, method, argFmt); \
        jobject localRef = env->NewLocalRef(delegate); \
        if(localRef){ \
            env->CallVoidMethod(localRef, jmid, ##__VA_ARGS__); \
        }else{ \
            WarnL << "弱引用已经释放:" << method << " " << argFmt; \
        }\
    }); \
}

/*
 * 加载动态库
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    //设置日志
    s_jvm = vm;
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    InfoL;
    return JNI_VERSION_1_6;
}

static pthread_t s_tread_id = 0;
/*
 * 卸载动态库
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved){
    InfoL;
    if(s_tread_id){
        pthread_kill(s_tread_id,SIGINT);
    }
}

extern int start_main(int argc,char *argv[]);
JNI_API(jboolean,startDemo,jstring ini_dir){
    string sd_path = stringFromJstring(env,ini_dir);
    string ini_file = sd_path +  "/zlmediakit.ini";

    //可以在sd卡根目录下放置ssl证书以便支持https服务器，证书支持p12或pem格式
    string pem_file = sd_path +  "/zlmediakit.pem";

    DebugL << "sd_path:" << sd_path;
    DebugL << "ini file:" << ini_file;

    static thread s_th([sd_path,ini_file,pem_file](){
        s_tread_id = pthread_self();
        try {
            //http根目录修改默认路径
            mINI::Instance()[Http::kRootPath] = mINI::Instance()[Hls::kFilePath] = sd_path + "/httpRoot";
            //mp4录制点播根目录修改默认路径
            mINI::Instance()[Record::kFilePath] = mINI::Instance()[Hls::kFilePath] = sd_path + "/httpRoot";
			mINI::Instance()[Record::kFileRepeat] = true;//add by lvjiong 默认开启
			mINI::Instance()[General::kModifyStamp] = true;//add by lvjiong 默认开启
            //hls根目录修改默认路径
            mINI::Instance()[Hls::kFilePath] = mINI::Instance()[Hls::kFilePath] = sd_path + "/httpRoot";
            //替换默认端口号(在配置文件未生成时有效)
            mINI::Instance()["http.port"] = 8080;
            mINI::Instance()["http.sslport"] = 8443;
            mINI::Instance()["rtsp.port"] = 8554;
            mINI::Instance()["rtsp.sslport"] = 8332;
            mINI::Instance()["general.enableVhost"] = 0;
            for(auto &pr : mINI::Instance()){
                //替换hook默认地址
                replace(pr.second,"https://127.0.0.1/","http://127.0.0.1:8080/");
            }
            //默认打开hook
            mINI::Instance()["hook.enable"] = 0;
            //默认打开http api调试
            mINI::Instance()["api.apiDebug"] = 1;

            int argc = 5;
            const char *argv[] = {"","-c",ini_file.data(),"-s",pem_file.data()};

            start_main(argc,(char **)argv);
        }catch (std::exception &ex){
            WarnL << ex.what();
        }
    });

    static onceToken s_token([]{
        s_th.detach();
    });

    return true;
};

JNI_API(jlong,createMediaPlayer,jstring url,jobject callback){
    static auto loadFrameClass = makeJavaFrame(env,nullptr);
    MediaPlayer::Ptr *ret = new MediaPlayer::Ptr(new MediaPlayer());
    MediaPlayer::Ptr &player = *ret;

    weak_ptr<MediaPlayer> weakPlayer = player;
    jobject globalWeakRef = env->NewWeakGlobalRef(callback);
    player->setOnPlayResult([weakPlayer,globalWeakRef](const SockException &ex) {
        auto strongPlayer = weakPlayer.lock();
        if (!strongPlayer) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPlayResult","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));

        if(ex){
            return;
        }

        auto viedoTrack = strongPlayer->getTrack(TrackVideo);
        if (viedoTrack) {
            viedoTrack->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([globalWeakRef](const Frame::Ptr &frame) {
                emitEvent((jobject)globalWeakRef,"onData","(L" MediaFrameSign ";)V",makeJavaFrame(env,frame));
            }));
        }

        auto audioTrack = strongPlayer->getTrack(TrackAudio);
        if (audioTrack) {
            audioTrack->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([globalWeakRef](const Frame::Ptr &frame) {
                emitEvent((jobject)globalWeakRef,"onData","(L" MediaFrameSign ";)V",makeJavaFrame(env,frame));
            }));
        }

    });

    player->setOnShutdown([globalWeakRef,weakPlayer](const SockException &ex) {
        auto strongPlayer = weakPlayer.lock();
        if (!strongPlayer) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPlayShutdown","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));
    });

    (*player)[Client::kRtpType] = Rtsp::RTP_TCP;
    player->play(stringFromJstring(env,url));

    return (jlong)(ret);
}



JNI_API(void,releaseMediaPlayer,jlong ptr){
    MediaPlayer::Ptr *player = (MediaPlayer::Ptr *)ptr;
    delete player;
}


JNI_API(jlong,createMediaPusher,jstring filePath,jstring pushUrl,jobject callback){
	string pushUrl_str = stringFromJstring(env,pushUrl);
	const string filePath_str = stringFromJstring(env,filePath);
	string schema = FindField(pushUrl_str.data(), nullptr,"://").substr(0,4);
	string vhost = DEFAULT_VHOST;
	string app = "live";
	string stream = "stream";
	auto poller = EventPollerPool::Instance().getPoller();
	//推流器，保持强引用
	MediaPusher::Ptr pusher,*ret;
	
	weak_ptr<MediaPusher> weakPusher = pusher;
	jobject globalWeakRef = env->NewWeakGlobalRef(callback);
	
	auto src = MediaSource::createFromMP4(schema,vhost,app,stream,filePath_str,false);
	if(!src){
		//文件不存在
		WarnL << "MP4文件不存在:" << filePath_str;
		return (jlong)NULL;
	}
	//创建推流器并绑定一个MediaSource
	pusher.reset(new MediaPusher(src,poller));	
	
	//设置推流中断处理逻辑
    pusher->setOnShutdown([weakPusher,globalWeakRef](const SockException &ex) {
        auto strongPusher = weakPusher.lock();
        if (!strongPusher) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPushShutdown","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));
    });
	 //设置发布结果处理逻辑
    pusher->setOnPublished([weakPusher,globalWeakRef](const SockException &ex) {
		auto strongPusher = weakPusher.lock();
        if (!strongPusher) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPushResult","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));

        if(ex){
            return;
        }
    });
    pusher->publish(pushUrl_str);
	ret = &pusher;
	
	//设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    pusher.reset();
	
	return (jlong)(ret);
}
JNI_API(void,releaseMediaPusher,jlong ptr){
    MediaPusher::Ptr *pusher = (MediaPusher::Ptr *)ptr;
    delete pusher;
}
JNI_API(jlong,createMediaProxy,jstring pollUrl,jstring pushUrl,jobject callback){
	string pushUrl_str = stringFromJstring(env,pushUrl);
	string playUrl = stringFromJstring(env,pollUrl);
	string schema = FindField(pushUrl_str.data(), nullptr,"://").substr(0,4);
	string vhost = DEFAULT_VHOST;
	string app = "live";
	string stream = "chn";
	auto poller = EventPollerPool::Instance().getPoller();
	//推流器，保持强引用
	//MediaPusher::Ptr pusher,*ret;
	PlayerProxy::Ptr player,*ret;
	//weak_ptr<MediaPusher> weakPusher = pusher;
	jobject globalWeakRef = env->NewWeakGlobalRef(callback);
	
	map<string, PlayerProxy::Ptr> proxyMap;
	player.reset(new PlayerProxy(vhost, app, "chn0",false,false,-1 , poller));
	//可以指定rtsp拉流方式，支持tcp和udp方式，默认tcp
	//(*player)[Client::kRtpType] = Rtsp::RTP_UDP;
	(*player)[Client::kRtpType] = Rtsp::RTP_TCP;
	
	weak_ptr<PlayerProxy> weakPlayer = player;
	//开始播放，如果播放失败或者播放中止，将会自动重试若干次，默认一直重试
	player->setPlayCallbackOnce([weakPlayer,globalWeakRef](const SockException &ex){
		auto strongPlayer = weakPlayer.lock();
		if (!strongPlayer) {
			return;
		}
		
		emitEvent((jobject)globalWeakRef,"onProxyResult","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));
		
		if(ex){
			return;
		}
	});

	//被主动关闭拉流
	player->setOnClose([weakPlayer,globalWeakRef](){
		emitEvent((jobject)globalWeakRef,"onProxyShutdown","(ILjava/lang/String;)V",0,"stream url Closed");
	});
	player->play(playUrl.data());
	proxyMap.emplace("0", player);
	 
	/* pusher.reset(new MediaPusher(schema,vhost, app, stream,poller));
	//监听RtmpMediaSource注册事件,在PlayerProxy播放成功后触发
	NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaChanged,
										 [pusher,schema,vhost,app, stream,pushUrl_str,poller](BroadcastMediaChangedArgs) {
											 //媒体源"live/stream"已经注册，这时方可新建一个RtmpPusher对象并绑定该媒体源
											 if(bRegist && pushUrl_str.find(sender.getSchema()) == 0){
												 //pusher.reset(new MediaPusher(schema,vhost, app, stream,poller));
												 pusher->publish(pushUrl_str);
											 }
										 });
	
	//设置推流中断处理逻辑
    pusher->setOnShutdown([weakPusher,globalWeakRef](const SockException &ex) {
        auto strongPusher = weakPusher.lock();
        if (!strongPusher) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPushShutdown","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));
    });
	 //设置发布结果处理逻辑
    pusher->setOnPublished([weakPusher,globalWeakRef](const SockException &ex) {
		auto strongPusher = weakPusher.lock();
        if (!strongPusher) {
            return;
        }
        emitEvent((jobject)globalWeakRef,"onPushResult","(ILjava/lang/String;)V",(jint)ex.getErrCode(),env->NewStringUTF(ex.what()));

        if(ex){
            return;
        }
    }); */
    //pusher->publish(pushUrl_str);
	ret = &player;
	
	//设置退出信号处理函数
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    player.reset();
	
	return (jlong)(ret);
}
JNI_API(void,releaseMediaProxy,jlong ptr){
    PlayerProxy::Ptr *proxyer = (PlayerProxy::Ptr *)ptr;
    delete proxyer;
}
