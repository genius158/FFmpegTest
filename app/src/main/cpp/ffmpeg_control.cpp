#include <jni.h>
#include <string>
#include <stdio.h>
#include <android/log.h>
#include <libavutil/opt.h>
#include "threadsafe_queue.cpp"
#include <pthread.h>

/**
 * 初始化视频编码器
 * @return
 */

using namespace std;

extern "C"
{

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "ffmpeg.h"
//Error:error: assembler command failed with exit code 1 (use -v to see invocation)
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"FFMPEG",FORMAT,##__VA_ARGS__);

int is_end = 0;
int is_release = 0;
threadsafe_queue<uint8_t *> frame_queue;
AVFormatContext *pFormatCtx;
AVOutputFormat *fmt;
AVStream *video_st;
AVCodecContext *pCodecCtx;
AVCodec *pCodec;
AVPacket pkt;
AVFrame *pFrame;
int picture_size;
int out_y_size;
int framecnt = 0;
int frame_count = 0;

const char *media_full_path; //文件储存地址
int in_width; //输出宽度
int in_height; //输入高度
int out_height; //输出高度
int out_width; //输出宽度
int frame_rate; //视频帧率控制
long long video_bit_rate; //视频比特率控制
int audio_bit_rate; //音频比特率控制
int audio_sample_rate; //音频采样率控制（44100）
int v_custom_format; //一些滤镜操作控制
JNIEnv *env; //env全局指针
JavaVM *javaVM; //jvm指针
jclass java_class; //java接口类的calss对象

// -------------------- line -----------------
int initVideoEncoder();
void *startEncode(void *obj);
int encodeEnd();
int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_prepareEncode(JNIEnv *ev, jobject instance,
                                                jstring mediaFullPath_, jint vcFormat, jint inWidth,
                                                jint inHeight, jint outWidth, jint outHeight,
                                                jint frameRate, jlong bitRate) {
    java_class = (jclass) env->NewGlobalRef(instance);
    media_full_path = env->GetStringUTFChars(mediaFullPath_, 0);
    video_bit_rate = bitRate;
    frame_rate = frameRate;
    audio_bit_rate = 40000;
    audio_sample_rate = 44100;
    in_width = inWidth;
    in_height = inHeight;
    out_height = outHeight;
    out_width = outWidth;
    v_custom_format = vcFormat;
    env = ev;
    int v_code = initVideoEncoder();
    env->ReleaseStringUTFChars(mediaFullPath_, media_full_path);
    if (v_code == 0) {
        return 0;
    } else {
        return -1;
    }
}


JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_videoEncode(JNIEnv *env, jobject instance,
                                              jobjectArray commends) {


}
/**
 * 初始化视频编码器
 * @return
 */
int initVideoEncoder() {
    LOGE("视频编码器初始化开始")

    size_t path_length = strlen(media_full_path);
    char *out_file = (char *) malloc(path_length + 1);
    strcpy(out_file, media_full_path);

    av_register_all();
    avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    fmt = pFormatCtx->oformat;

    //Open output URL
    if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
        LOGE("_Failed to open output file! \n");
        return -1;
    }

    video_st = avformat_new_stream(pFormatCtx, 0);

    if (video_st == NULL) {
        LOGE ("_video_st==null");
        return -1;
    }

    //Param that must set
    pCodecCtx = video_st->codec;
    //pCodecCtx->codec_id =AV_CODEC_ID_HEVC;
    pCodecCtx->codec_id = AV_CODEC_ID_H264;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    if (v_custom_format == 0 || v_custom_format == 2) {
        pCodecCtx->width = out_width;
        pCodecCtx->height = out_height;
    } else {
        pCodecCtx->width = out_height;
        pCodecCtx->height = out_width;
    }

    pCodecCtx->bit_rate = video_bit_rate;
    pCodecCtx->gop_size = 50;
    pCodecCtx->thread_count = 12;

    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = frame_rate;
    pCodecCtx->qmin = 10;
    pCodecCtx->qmax = 51;

    //Optional Param
    pCodecCtx->max_b_frames = 3;

    // Set Option
    AVDictionary *param = 0;
    //H.264
    if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "tune", "zerolatency", 0);
        av_opt_set(pCodecCtx->priv_data, "preset", "ultrafast", 0);
        av_dict_set(&param, "profile", "baseline", 0);
    }

    //Show some Information
    av_dump_format(pFormatCtx, 0, out_file, 1);

    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec) {
        LOGE("Can not find encoder! \n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, &param) < 0) {
        LOGE("Failed to open encoder! \n");
        return -1;
    }


    pFrame = av_frame_alloc();
    picture_size = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
    LOGE("   picture_size:%d", picture_size);
    uint8_t *buf = (uint8_t *) av_malloc(picture_size);
    avpicture_fill((AVPicture *) pFrame, buf, pCodecCtx->pix_fmt, pCodecCtx->width,
                   pCodecCtx->height);

    //Write File Header
    avformat_write_header(pFormatCtx, NULL);
    av_new_packet(&pkt, picture_size);
    out_y_size = pCodecCtx->width * pCodecCtx->height;
    pthread_t thread;
    pthread_create(&thread, NULL, startEncode, NULL);

    LOGE("视频编码器初始化完成");

    return 0;
}


/**
 * 启动编码线程
 * @param obj
 * @return
 */
void *startEncode(void *obj) {
    while (!frame_queue.empty()) {
        if (is_release) {
            //Write file trailer
            av_write_trailer(pFormatCtx);

            //Clean
            if (video_st) {
                avcodec_close(video_st->codec);
                av_free(pFrame);
            }
            avio_close(pFormatCtx->pb);
            avformat_free_context(pFormatCtx);
            return 0;
        }
        if (frame_queue.empty()) {
            continue;
        }
        uint8_t *picture_buf = (uint8_t *) frame_queue.wait_and_pop().get();
        LOGE("send_videoframe_count:%d", frame_count);
        int in_y_size = in_width * in_height;

        pFrame->pts = frame_count;
        frame_count++;
        int got_picture = 0;
        //Encode
        int ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_picture);
        if (ret < 0) {
            LOGE("Failed to encode! \n");
        }
        if (got_picture == 1) {
            LOGE("Succeed to encode frame: %5d\tsize:%5d\n", framecnt,
                 pkt.size);
            framecnt++;
            pkt.stream_index = video_st->index;
            ret = av_write_frame(pFormatCtx, &pkt);
            av_free_packet(&pkt);
        }
        delete (picture_buf);
    }
    if (is_end) {
       encodeEnd();
    }
    return 0;
}

/**
 * 视频编码结束
 * @return
 */
int encodeEnd() {
    //Flush Encoder
    int ret_1 = flush_encoder(pFormatCtx, 0);
    if (ret_1 < 0) {
        LOGE( "Flushing encoder failed\n");
        return -1;
    }

    //Write file trailer
    av_write_trailer(pFormatCtx);

    //Clean
    if (video_st) {
        avcodec_close(video_st->codec);
        av_free(pFrame);
//        av_free(picture_buf);
    }
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    LOGE( "视频编码结束");
    return 1;
}

/**
 * 结束编码时刷出还在编码器里面的帧
 * @param fmt_ctx
 * @param stream_index
 * @return
 */
int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index) {
    int ret;
    int got_frame;
    AVPacket enc_pkt;
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
          CODEC_CAP_DELAY))
        return 0;
    while (1) {
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
                                    NULL, &got_frame);
        av_frame_free(NULL);
        if (ret < 0)
            break;
        if (!got_frame) {
            ret = 0;
            break;
        }
        LOGE( "_Flush Encoder: Succeed to encode 1 frame video!\tsize:%5d\n",
             enc_pkt.size);
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }

    return ret;
}

}