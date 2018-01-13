#include <jni.h>
#include <string>
#include <stdio.h>
#include <android/log.h>

using namespace std;

extern "C"
{

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"FFMPEG",FORMAT,##__VA_ARGS__);


JNIEXPORT jstring JNICALL
Java_com_yan_ffmpeg_MainActivity_configurationInfo(JNIEnv *env, jobject /* this */) {
    av_register_all();
    string configuration = avcodec_configuration();
    return env->NewStringUTF(configuration.c_str());
}
JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_MainActivity_renderFrame(JNIEnv *env, jobject instance, jstring input_,
                                             jobject surface) {

}

JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_MainActivity_decode(JNIEnv *env, jobject instance, jstring input_jstr,
                                        jstring output_jstr) {
    AVFormatContext *pFormatCtx;
    int i, videoindex;
    AVCodecContext *pCodecCtx;
    AVCodecParameters *pParameters = NULL;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    uint8_t *out_buffer;
    AVPacket *packet;
    int y_size;
    struct SwsContext *img_convert_ctx;
    FILE *fp_yuv;
    int frame_cnt;
    clock_t time_start, time_finish;
    double time_duration = 0.0;
    const char *input_str = env->GetStringUTFChars(input_jstr, 0);
    const char *output_str = env->GetStringUTFChars(output_jstr, 0);

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, input_str, NULL, NULL) != 0) {
        LOGE("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("Couldn't find stream information.\n");
        return -1;
    }
    videoindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        pParameters = pFormatCtx->streams[i]->codecpar;
        if (pParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }
    if (videoindex == -1) {
        LOGE("Couldn't find a video stream.\n");
        return -1;
    }
    pCodec = avcodec_find_decoder(pParameters->codec_id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pParameters);
    if (pCodec == NULL) {
        LOGE("Couldn't find Codec.\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("Couldn't open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer = (unsigned char *) av_malloc(
            av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);


    packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    fp_yuv = fopen(output_str, "wb+");
    if (fp_yuv == NULL) {
        LOGE("Cannot open output file.\n");
        return -1;
    }

    frame_cnt = 0;
    time_start = clock();

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoindex) {
           int ret = avcodec_send_packet(pCodecCtx, packet);
            while (ret == 0 && avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height,
                          pFrameYUV->data, pFrameYUV->linesize);

                y_size = pCodecCtx->width * pCodecCtx->height;
                fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
                fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
                fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
                //Output info
                char pictype_str[10] = {0};
                switch (pFrame->pict_type) {
                    case AV_PICTURE_TYPE_I:
                        LOGE(pictype_str, "I");
                        break;
                    case AV_PICTURE_TYPE_P:
                        LOGE(pictype_str, "P");
                        break;
                    case AV_PICTURE_TYPE_B:
                        LOGE(pictype_str, "B");
                        break;
                    default:
                        LOGE(pictype_str, "Other");
                        break;
                }
                LOGE("Frame Index: %5d. Type:%s", frame_cnt, pictype_str);
                frame_cnt++;
            }
        }
        av_packet_unref(packet);
    }

    time_finish = clock();
    time_duration = (double) (time_finish - time_start);
    LOGE("time duration : %0.2f",time_duration);

    sws_freeContext(img_convert_ctx);

    fclose(fp_yuv);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    env->ReleaseStringUTFChars(input_jstr, input_str);
    env->ReleaseStringUTFChars(output_jstr, output_str);
    return 0;
}
}