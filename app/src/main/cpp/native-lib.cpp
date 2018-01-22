#include <jni.h>
#include <string>
#include <stdio.h>
#include <android/log.h>

using namespace std;

extern "C"
{
#include <pthread.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/parseutils.h"
#include "libavutil/opt.h"
#include "ffmpeg.h"
//Error:error: assembler command failed with exit code 1 (use -v to see invocation)
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"FFMPEG",FORMAT,##__VA_ARGS__);

JNIEXPORT jstring JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_configurationInfo(JNIEnv *env, jobject /* this */) {
    av_register_all();
    string configuration = avcodec_configuration();
    return env->NewStringUTF(configuration.c_str());
}
JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_renderFrame(JNIEnv *env, jobject instance, jstring input_,
                                              jobject surface) {
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_decode(JNIEnv *env, jobject instance, jstring input_jstr,
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
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, input_str, NULL, NULL) != 0) {
        LOGE("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("Couldn't find stream information.\n");
        return -2;
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
        return -3;
    }
    pCodec = avcodec_find_decoder(pParameters->codec_id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pParameters);
    if (pCodec == NULL) {
        LOGE("Couldn't find Codec.\n");
        return -4;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("Couldn't open codec.\n");
        return -5;
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
        return -6;
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
    LOGE("time duration : %0.2f", time_duration);

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

JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_addOverlay(JNIEnv *env, jobject instance, jstring videoPath_,
                                             jstring impPath_, jstring outputPath_) {
    const char *videoPath = env->GetStringUTFChars(videoPath_, 0);
    const char *impPath = env->GetStringUTFChars(impPath_, 0);
    const char *outputPath = env->GetStringUTFChars(outputPath_, 0);

    av_register_all();
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    int ret = avformat_open_input(&pFormatCtx, videoPath, NULL, NULL);
    if (ret != 0) {
        LOGE("Couldn't open input stream.\n");
        return -1;
    }

    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (ret != 0) {
        LOGE("Couldn't find stream info.\n");
        return -2;
    }

    AVCodecParameters *codec_param = NULL;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        AVCodecParameters *c_param = pFormatCtx->streams[i]->codecpar;
        if (c_param->codec_type == AVMEDIA_TYPE_VIDEO) {
            codec_param = c_param;
        }
    }

    if (codec_param == NULL) {
        LOGE("Couldn't find video stream.\n");
        return -3;
    }

    int kbps = (int) (codec_param->bit_rate / 1000);
    LOGE("bit, delay : %d , %d", kbps, codec_param->video_delay);

    int size = 8;

    char **args = (char **) malloc(sizeof(char *) * size);
    for (int i = 0; i < size; ++i) {
        args[i] = (char *) malloc(sizeof(char) * 1024);
    }

    // "ffmpeg -i %s -i %s -filter_complex overlay=150:50 %s"
    sprintf(args[0], "ffmpeg");
    sprintf(args[1], "-i");
    sprintf(args[2], "%s", videoPath);
    sprintf(args[3], "-i");
    sprintf(args[4], "%s", impPath);
    sprintf(args[5], "-filter_complex");
    sprintf(args[6], "overlay=10:10");
    sprintf(args[7], "%s", outputPath);
    for (int i = 0; i < size; i++) {
        LOGE("args ----- %s", args[i]);
    }
    ffmpeg_run(size, args);
    return 0;
}


JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_ffmpegRun(JNIEnv *env, jobject instance, jobjectArray commends) {
    int argc = env->GetArrayLength(commends);
    char *argv[argc];
    int i;
    for (i = 0; i < argc; i++) {
        jstring js = (jstring) env->GetObjectArrayElement(commends, i);
        argv[i] = (char *) env->GetStringUTFChars(js, 0);
    }
    return ffmpeg_run(argc, argv);
}

//--------------------------------------- line ---------------------------------------

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
        LOGE("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
        /* mux encoded frame */
        ret = av_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_yan_ffmpeg_FFMPEGControl_videoEncodeTest(JNIEnv *env, jobject instance, jstring input_,
                                                  jstring output_,
                                                  jstring outputmp4_) {

    const char *inputurl_str = env->GetStringUTFChars(input_, 0);
    const char *outputurl_str = env->GetStringUTFChars(output_, 0);
    const char *outputmp4url_str = env->GetStringUTFChars(outputmp4_, 0);

    AVFormatContext *pFormatCtx;
    AVOutputFormat *fmt;
    AVStream *video_st;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket pkt;
    uint8_t *picture_buf;
    AVFrame *pFrame;
    int picture_size;
    int y_size;
    int framecnt = 0;
    int i;
    int ret = 0;
    char *parsed_key, *value;
    const char *settings_args = "profile=main:vpre=ultrafast";
    //FILE *in_file = fopen("src01_480x272.yuv", "rb");	//Input raw YUV data
    FILE *in_file = fopen(inputurl_str, "rb");   //Input raw YUV data

    int in_w = 1080, in_h = 1920;   //Input data's width and height

    int frame_num = 73;                                   //Frames to encode
    //const char* out_file = "src01.h264";              //Output Filepath
    //const char* out_file = "src01.ts";
    //const char* out_file = "src01.hevc";
    //const char* out_file = "ds.h264";

    av_register_all();
    //Method1.
    pFormatCtx = avformat_alloc_context();
    //Guess Format
    fmt = av_guess_format(NULL, outputurl_str, NULL);
    pFormatCtx->oformat = fmt;

    //Method 2.
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    //fmt = pFormatCtx->oformat;


    //Open output URL
    if (avio_open(&pFormatCtx->pb, outputurl_str, AVIO_FLAG_READ_WRITE) < 0) {
        LOGE("Failed to open output file! \n");
        return -1;
    }

    video_st = avformat_new_stream(pFormatCtx, 0);
    video_st->time_base.num = 1;
    video_st->time_base.den = 25;

    if (video_st == NULL) {
        return -1;
    }
    //Param that must set
    pCodecCtx = video_st->codec;
    //pCodecCtx->codec_id =AV_CODEC_ID_HEVC;
    pCodecCtx->codec_id = fmt->video_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx->width = in_w;
    pCodecCtx->height = in_h;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->gop_size = 250;
    //H264
    //pCodecCtx->me_range = 16;
    //pCodecCtx->max_qdiff = 4;
    //pCodecCtx->qcompress = 0.6;
    pCodecCtx->qmin = 10;
    pCodecCtx->qmax = 51;

    //Optional Param
    pCodecCtx->max_b_frames = 3;

    // Set Option
    AVDictionary *param = 0;
    //Set Options
    while (*settings_args) {
        ret = av_opt_get_key_value(&settings_args, "=", ":", 0, &parsed_key, &value);
        if (ret < 0)
            break;
        av_dict_set(&param, parsed_key, value, 0);
        LOGE("key:%s, value:%s\n", parsed_key, value);
        if (*settings_args)
            settings_args++;
        av_free(parsed_key);
        av_free(value);
    }

    //av_dict_set(&param, "preset", "ultrafast", 0);
    //av_dict_set(&param, "tune", "zerolatency", 0);
    //av_dict_set(&param, "profile", "main", 0);

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
    picture_size = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                            1);
    picture_buf = (uint8_t *) av_malloc(picture_size);
    av_image_fill_arrays(pFrame->data, pFrame->linesize, picture_buf,
                         pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);

    //Write File Header
    avformat_write_header(pFormatCtx, NULL);

    av_new_packet(&pkt, picture_size);

    y_size = pCodecCtx->width * pCodecCtx->height;

    for (i = 0; i < frame_num; i++) {
        //Read raw YUV data
        if (fread(picture_buf, 1, y_size * 3 / 2, in_file) <= 0) {
            LOGE("Failed to read raw data! \n");
            break;
        } else if (feof(in_file)) {
            break;
        }
        pFrame->data[0] = picture_buf;              // Y
        pFrame->data[1] = picture_buf + y_size;      // U
        pFrame->data[2] = picture_buf + y_size * 5 / 4;  // V
        //PTS
        pFrame->pts = i;
        int got_picture = 0;
        //Encode
        int ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_picture);
        if (ret < 0) {
            LOGE("Failed to encode! \n");
            return -1;
        }
        if (got_picture == 1) {
            LOGE("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, pkt.size);
            framecnt++;
            pkt.stream_index = video_st->index;
            av_packet_unref(&pkt);
        }
    }
    //Flush Encoder
    ret = flush_encoder(pFormatCtx, 0);
    if (ret < 0) {
        LOGE("Flushing encoder failed\n");
        return -1;
    }

    //Write file trailer
    av_write_trailer(pFormatCtx);

    //Clean
    if (video_st) {
        avcodec_close(video_st->codec);
        av_free(pFrame);
        av_free(picture_buf);
    }
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);

    fclose(in_file);

    char *  output_url = (char *) malloc(strlen(outputurl_str) + 1);
    strcpy(output_url, outputurl_str);
    char *   outputmp4_url = (char *) malloc(strlen(outputmp4url_str) + 1);
    strcpy(outputmp4_url, outputmp4url_str);
    char *cmd[4];
    cmd[0] = "ffmpeg";
    cmd[1] = "-i";
    cmd[2] = output_url;
    cmd[3] = outputmp4_url;

    for (int i = 0; i < 4; ++i) {
        LOGE("%s", cmd[i]);
    }

    ffmpeg_run(4, cmd);
    LOGE("ffmpegmain end");
    env->ReleaseStringUTFChars(input_, inputurl_str);
    env->ReleaseStringUTFChars(output_, outputurl_str);
    env->ReleaseStringUTFChars(outputmp4_, outputmp4url_str);
    return 0;

}


}
