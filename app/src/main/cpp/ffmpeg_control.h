#include <stdio.h>
#include <android/log.h>
#include "syn_queue.cpp"
#include <pthread.h>

extern "C"
{
#include "ffmpeg.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

using namespace std;

class FFMpegControl {
private:
    AVFormatContext *pFormatCtx;
    AVStream *video_st;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket pkt;
    AVFrame *pFrame;

    int picture_size;
    int y_size;
    int framecnt = 0;
    char *parsed_key, *value;
    const char *settings_args = "profile=main:vpre=ultrafast";
    int in_w;
    int in_h;

    char *media_base_path;
    char *media_name;
    char *media_full_name;
    char *temp_h264_path;

    SyncQueue<uint8_t *> sync_queue;

    int is_end;

private:
    static void log_callback_null(void *ptr, int level, const char *fmt, va_list vl);

public:
    void encodeFlagEnd();

    void putOneFrame(uint8_t *data);

    int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

    static void *h264ToMp4(void *obj);

    int prepareEncode(char *mediaBasePath_, char *mediaName_, int inWidth,
                      int inHeight, int outWidth, int outHeight,
                      int frameRate, long bitRate);

    int getYUVSize();

    ~FFMpegControl() {
    }

};
