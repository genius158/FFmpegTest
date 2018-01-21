#include <jni.h>
#include <string>
#include "ffmpeg_control.h"

FFMpegControl *ffMpegControl;

extern "C"
JNIEXPORT void JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_encodeFlagEnd(JNIEnv *env, jobject instance) {
    ffMpegControl->encodeFlagEnd();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_putOneFrame(JNIEnv *env, jobject instance, jbyteArray data_) {
    jbyte *elements = env->GetByteArrayElements(data_, 0);
    int in_y_size = ffMpegControl->getYUVSize();
    uint8_t *new_buf = (uint8_t *) malloc(in_y_size * 3 / 2);
    memcpy(new_buf, (uint8_t *) elements, in_y_size * 3 / 2);
    ffMpegControl->putOneFrame(new_buf);
    env->ReleaseByteArrayElements(data_, elements, 0);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_prepareEncode(JNIEnv *env, jobject instance,
                                                jstring mediaBasePath_, jstring mediaName_,
                                                jint filter, jint inWidth,
                                                jint inHeight, jint outWidth, jint outHeight,
                                                jint frameRate, jlong bitRate) {

    const char *bp = env->GetStringUTFChars(mediaBasePath_, 0);
    char *media_base_path = (char *) malloc(strlen(bp) + 1);
    strcpy(media_base_path, bp);
    const char *n = env->GetStringUTFChars(mediaName_, 0);
    char *media_name = (char *) malloc(strlen(n) + 1);
    strcpy(media_name, n);

    ffMpegControl = new FFMpegControl();
    int ret = ffMpegControl->prepareEncode(media_base_path, media_name, filter, inWidth, inHeight,
                                           outWidth,
                                           outHeight, frameRate, bitRate);

    env->ReleaseStringUTFChars(mediaBasePath_, bp);
    env->ReleaseStringUTFChars(mediaName_, n);
    return ret;
}
