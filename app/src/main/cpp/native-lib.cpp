#include <jni.h>
#include <string>
using namespace std;

extern "C"
{

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"


JNIEXPORT jstring JNICALL
Java_com_yan_ffmpeg_MainActivity_configurationInfo(JNIEnv *env, jobject /* this */) {
    av_register_all();
    string configuration = avcodec_configuration();
    return env->NewStringUTF(configuration.c_str());
}

}
