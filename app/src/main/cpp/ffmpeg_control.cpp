#include "ffmpeg_control.h"

#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"FFMPEG",FORMAT,##__VA_ARGS__);

void FFMpegControl::encodeFlagEnd() {
    is_end = 1;
}

void FFMpegControl::putOneFrame(uint8_t *new_buf) {
    sync_queue.put(new_buf);
}

int FFMpegControl::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index) {
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

void *FFMpegControl::h264ToMp4(void *obj) {
    FFMpegControl *fmc = (FFMpegControl *) obj;
    fmc->y_size = fmc->pCodecCtx->width * fmc->pCodecCtx->height;
    int i = 0;
    while (1) {
        if (fmc->sync_queue.isEmpty()) {
            if (fmc->is_end) {
                LOGE("encode end ");
                break;
            }
            continue;
        }
        LOGE("encode ");
        uint8_t *picture_buf;
        fmc->sync_queue.take(picture_buf);
        LOGE("take %d", picture_buf);
        fmc->pFrame->data[0] = picture_buf;              // Y
        fmc->pFrame->data[1] = picture_buf + fmc->y_size;      // U
        fmc->pFrame->data[2] = picture_buf + fmc->y_size * 5 / 4;  // V
        //PTS
        fmc->pFrame->pts = i++;
        int got_picture = 0;
        //Encode
        int ret = avcodec_encode_video2(fmc->pCodecCtx, &fmc->pkt, fmc->pFrame, &got_picture);
        if (ret < 0) {
            LOGE("Failed to encode! \n");
        }
        LOGE("Succeed step %d", got_picture);
        if (got_picture == 1) {
            LOGE("Succeed to encode frame: %5d\tsize:%5d\n", fmc->framecnt, fmc->pkt.size);
            fmc->framecnt++;
            fmc->pkt.stream_index = fmc->video_st->index;
        }
        av_packet_unref(&fmc->pkt);
        delete (picture_buf);
    }
    LOGE("Succeed flush_encoder");

    //Flush Encoder
    int ret = fmc->flush_encoder(fmc->pFormatCtx, 0);
    if (ret < 0) {
        LOGE("Flushing encoder failed\n");
    }

    //Write file trailer
    av_write_trailer(fmc->pFormatCtx);

    //Clean
    if (fmc->video_st) {
        avcodec_close(fmc->video_st->codec);
        av_free(fmc->pFrame);
    }
    avio_close(fmc->pFormatCtx->pb);
    avformat_free_context(fmc->pFormatCtx);

    remove(fmc->media_full_name);

    char *cmd[4];
    cmd[0] = "ffmpeg";
    cmd[1] = "-i";
    cmd[2] = fmc->temp_h264_path;
    cmd[3] = fmc->media_full_name;

    ffmpeg_run(4, cmd);
    LOGE("mp4 encode finish !!!!!!");

    free(fmc->media_base_path);
    free(fmc->media_name);
    free(fmc->media_full_name);
    free(fmc->temp_h264_path);
    delete (fmc);
    return 0;
}

int FFMpegControl::prepareEncode(char *mediaBasePath, char *mediaName, int inWidth, int inHeight,
                                 int outWidth, int outHeight, int frameRate, long bitRate) {
//    av_log_set_callback(log_callback_null);
    media_base_path = mediaBasePath;
    media_name = mediaName;
    media_full_name = (char *) malloc(strlen(media_base_path) + strlen(media_name) + 1);
    sprintf(media_full_name, "%s%s", media_base_path, media_name);

    char temp_name[] = "temp.h264";
    temp_h264_path = (char *) malloc(strlen(media_base_path) + strlen(temp_name) + 1);
    sprintf(temp_h264_path, "%s%s", media_base_path, temp_name);

    LOGE("h264path : %s, mp4path : %s", temp_h264_path, media_full_name);

    in_w = inWidth;
    in_h = inHeight;

    //const char* out_file = "src01.h264";              //Output Filepath
    //const char* out_file = "src01.ts";
    //const char* out_file = "src01.hevc";
    //const char* out_file = "ds.h264";

    av_register_all();
    //Method1.
//    pFormatCtx = avformat_alloc_context();
    //Guess Format
//    fmt = av_guess_format(NULL, temp_h264_path, NULL);
//    pFormatCtx->oformat = fmt;

    //Method 2.
    avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, temp_h264_path);


    //Open output URL
    if (avio_open(&pFormatCtx->pb, temp_h264_path, AVIO_FLAG_READ_WRITE) < 0) {
        LOGE("Failed to open output file! \n");
        return -1;
    }

    video_st = avformat_new_stream(pFormatCtx, 0);
//    video_st->time_base.num = 1;
//    video_st->time_base.den = frameRate;

    if (video_st == NULL) {
        return -1;
    }
    //Param that must set
    pCodecCtx = video_st->codec;
    //pCodecCtx->codec_id =AV_CODEC_ID_HEVC;
    pCodecCtx->codec_id = AV_CODEC_ID_H264;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx->width = in_w;
    pCodecCtx->height = in_h;
    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = frameRate;
    pCodecCtx->bit_rate = bitRate;
    pCodecCtx->gop_size = 50;
    pCodecCtx->thread_count = 12;
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
        int ret = av_opt_get_key_value(&settings_args, "=", ":", 0, &parsed_key, &value);
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
    //Show some Information
    av_dump_format(pFormatCtx, 0, temp_h264_path, 1);

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
    picture_size = av_image_get_buffer_size(pCodecCtx->pix_fmt, pCodecCtx->width,
                                            pCodecCtx->height, 1);
    uint8_t *buf = (uint8_t *) av_malloc(picture_size);
    av_image_fill_arrays(pFrame->data, pFrame->linesize, buf,
                         pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, 1);

    //Write File Header
    avformat_write_header(pFormatCtx, NULL);

    av_new_packet(&pkt, picture_size);

    pthread_t thread;
    pthread_create(&thread, NULL, h264ToMp4, this);
    return 0;

}

int FFMpegControl::getYUVSize() {
    return in_h * in_w;
}

void FFMpegControl::log_callback_null(void *ptr, int level, const char *fmt, va_list vl) {
    static int print_prefix = 1;
    static int count;
    static char prev[1024];
    char line[1024];
    static int is_atty;

    av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);

    strcpy(prev, line);
    LOGE("ffmpeg-system-log: %s", line);
}
