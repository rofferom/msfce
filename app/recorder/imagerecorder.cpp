extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

#include "imagerecorder.h"

#define TAG "ImageRecorder"
#include "recorder_utils.h"

namespace msfce::recorder {

ImageRecorder::ImageRecorder(const std::string& basename, const msfce::core::SnesConfig& snesConfig)
    : m_Basename(basename),
      m_SnesConfig(snesConfig)
{
}

int ImageRecorder::start()
{
    return 0;
}

int ImageRecorder::stop()
{
    return 0;
}

bool ImageRecorder::onFrameReceived(const std::shared_ptr<Frame>& inputFrame)
{
    AVFrame* avFrame;
    AVPacket* pkt = nullptr;
    std::string outname;
    FILE* f;
    int ret;

    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    codecCtx->time_base.num = 1;
    codecCtx->time_base.den = 1;
    codecCtx->width = m_SnesConfig.displayWidth;
    codecCtx->height = m_SnesConfig.displayHeight;
    codecCtx->pix_fmt = AV_PIX_FMT_RGB24;

    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avcodec_open2", ret);
        return false;
    }

    // Input data
    avFrame = av_frame_alloc();
    if (!avFrame) {
        goto free_codecctx;
    }

    avFrame->width = m_SnesConfig.displayWidth;
    avFrame->height = m_SnesConfig.displayHeight;
    avFrame->format = AV_PIX_FMT_RGB24;

    ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_AVERROR("av_frame_get_buffer", ret);
        goto free_avframe;
    }

    memcpy(avFrame->data[0], inputFrame->payload.data(), avFrame->width * avFrame->height * kRgbSampleSize);

    // Output data
    pkt = av_packet_alloc();
    if (!pkt) {
        goto free_avframe;
    }

    ret = avcodec_send_frame(codecCtx, avFrame);
    if (ret < 0) {
        LOG_AVERROR("avcodec_send_frame", ret);
        goto free_packet;
    }

    outname = m_Basename + ".png";
    LOGI(TAG, "Saving screenshot in '%s'", outname.c_str());

    f = fopen(outname.c_str(), "wb");
    if (!f) {
        LOGW(TAG, "Fail to open '%s': %s", outname.c_str(), strerror(errno));
        goto free_packet;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_AVERROR("avcodec_receive_packet", ret);
            break;
        }

        fwrite(pkt->data, pkt->size, 1, f);
        av_packet_unref(pkt);
    }

    fclose(f);

free_packet:
    av_packet_free(&pkt);
free_avframe:
    av_frame_free(&avFrame);
free_codecctx:
    avcodec_free_context(&codecCtx);

    return false;
}

} // namespace msfce::recorder
