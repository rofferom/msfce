extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "videorecorder.h"

#define TAG "VideoRecorder"
#include "recorder_utils.h"

namespace msfce::recorder {

constexpr int kOutSampleRate = 48000;

VideoRecorder::VideoRecorder(
    const std::string& basename,
    const msfce::core::SnesConfig& snesConfig)
    : m_SnesConfig(snesConfig), m_Basename(basename)
{
}

int VideoRecorder::initVideo()
{
    AVCodec* codec = nullptr;
    int ret;

    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if (!codec) {
        LOGW(TAG, "Failed to find a valid H264 encoder");
        return -ECANCELED;
    }

    m_VideoCodecCtx = avcodec_alloc_context3(codec);
    if (!m_VideoCodecCtx) {
        LOGW(TAG, "avcodec_alloc_context3() failed");
        return -ECANCELED;
    }

    m_VideoCodecCtx->time_base.num = 1;
    m_VideoCodecCtx->time_base.den = m_SnesConfig.displayRate;
    m_VideoCodecCtx->width = m_SnesConfig.displayWidth;
    m_VideoCodecCtx->height = m_SnesConfig.displayHeight;
    m_VideoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Add and setup video stream
    m_VideoStream = avformat_new_stream(m_ContainerCtx, 0);
    if (!m_VideoStream) {
        LOGW(TAG, "avformat_new_stream() failed");
        goto free_codecctx;
    }

    if (m_ContainerCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_VideoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_VideoCodecCtx, codec, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avcodec_open2", ret);
        goto close_codec;
    }

    ret = avcodec_parameters_from_context(
        m_VideoStream->codecpar, m_VideoCodecCtx);
    if (ret < 0) {
        LOG_AVERROR("avcodec_parameters_from_context", ret);
        goto close_codec;
    }

    // Create RGB => YUV420 converter
    m_VideoSwsCtx = sws_getContext(
        m_SnesConfig.displayWidth,
        m_SnesConfig.displayHeight,
        AV_PIX_FMT_RGB24,
        m_SnesConfig.displayWidth,
        m_SnesConfig.displayHeight,
        AV_PIX_FMT_YUV420P,
        0,
        nullptr,
        nullptr,
        nullptr);
    if (!m_VideoSwsCtx) {
        LOGW(TAG, "sws_getContext() failed");
        goto close_codec;
    }

    return 0;

close_codec:
    avcodec_close(m_VideoCodecCtx);
free_codecctx:
    avcodec_free_context(&m_VideoCodecCtx);
    m_VideoCodecCtx = nullptr;

    return -ECANCELED;
}

int VideoRecorder::clearVideo()
{
    sws_freeContext(m_VideoSwsCtx);

    avcodec_close(m_VideoCodecCtx);
    avcodec_free_context(&m_VideoCodecCtx);

    return 0;
}

int VideoRecorder::initAudio()
{
    AVCodec* codec = nullptr;
    int ret;

    codec = avcodec_find_encoder_by_name("libopus");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    }

    if (!codec) {
        LOGW(TAG, "Failed to find a valid Opus encoder");
        return -ECANCELED;
    }

    m_AudioCodecCtx = avcodec_alloc_context3(codec);
    if (!m_AudioCodecCtx) {
        LOGW(TAG, "avcodec_alloc_context3() failed");
        return -ECANCELED;
    }

    m_AudioCodecCtx->sample_fmt = codec->sample_fmts[0];
    m_AudioCodecCtx->sample_rate = kOutSampleRate;
    m_AudioCodecCtx->channels = m_SnesConfig.audioChannels;
    m_AudioCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    m_AudioCodecCtx->bit_rate = 128000;

    // Add and setup audio stream
    m_AudioStream = avformat_new_stream(m_ContainerCtx, 0);
    if (!m_AudioStream) {
        LOGW(TAG, "avformat_new_stream() failed");
        goto free_codecctx;
    }

    m_AudioStream->time_base = (AVRational) {1, m_AudioCodecCtx->sample_rate};

    if (m_ContainerCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_AudioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_AudioCodecCtx, codec, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avcodec_open2", ret);
        goto close_codec;
    }

    ret = avcodec_parameters_from_context(
        m_AudioStream->codecpar, m_AudioCodecCtx);
    if (ret < 0) {
        LOG_AVERROR("avcodec_parameters_from_context", ret);
        goto close_codec;
    }

    m_AudioSnesFrameSize = av_rescale_rnd(
        m_AudioCodecCtx->frame_size,
        m_SnesConfig.audioSampleRate,
        m_AudioCodecCtx->sample_rate,
        AV_ROUND_UP);

    // Init resample
    m_AudioSwrCtx = swr_alloc();
    if (!m_AudioSwrCtx) {
        LOGW(TAG, "swr_alloc() failed");
        goto close_codec;
    }

    av_opt_set_int(
        m_AudioSwrCtx, "in_channel_count", m_SnesConfig.audioChannels, 0);
    av_opt_set_int(m_AudioSwrCtx, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(
        m_AudioSwrCtx, "in_sample_rate", m_SnesConfig.audioSampleRate, 0);
    av_opt_set_sample_fmt(m_AudioSwrCtx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    av_opt_set_int(
        m_AudioSwrCtx, "out_channel_count", m_SnesConfig.audioChannels, 0);
    av_opt_set_int(m_AudioSwrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(m_AudioSwrCtx, "out_sample_rate", kOutSampleRate, 0);
    av_opt_set_sample_fmt(
        m_AudioSwrCtx, "out_sample_fmt", m_AudioCodecCtx->sample_fmt, 0);

    ret = swr_init(m_AudioSwrCtx);
    if (ret < 0) {
        LOG_AVERROR("swr_init", ret);
        goto close_swr;
    }

    // Init fifo
    m_AudioFifo = av_audio_fifo_alloc(
        AV_SAMPLE_FMT_S16,
        m_SnesConfig.audioChannels,
        m_AudioCodecCtx->frame_size);
    if (!m_AudioFifo) {
        LOG_AVERROR("av_audio_fifo_alloc", ret);
        goto close_swr;
    }

    return 0;

close_swr:
    swr_free(&m_AudioSwrCtx);
close_codec:
    avcodec_close(m_AudioCodecCtx);
free_codecctx:
    avcodec_free_context(&m_AudioCodecCtx);
    m_AudioCodecCtx = nullptr;

    return -ECANCELED;
}

int VideoRecorder::clearAudio()
{
    av_audio_fifo_free(m_AudioFifo);
    m_AudioFifo = nullptr;

    swr_free(&m_AudioSwrCtx);

    avcodec_close(m_AudioCodecCtx);
    avcodec_free_context(&m_AudioCodecCtx);
    m_AudioCodecCtx = nullptr;

    return 0;
}

int VideoRecorder::start()
{
    AVCodec* codec = nullptr;
    int ret;

    std::string outname = m_Basename + ".mp4";
    LOGI(TAG, "Start record in '%s'", outname.c_str());

    // Create container
    auto fmt = av_guess_format("mp4", nullptr, nullptr);
    if (!fmt) {
        LOGW(TAG, "av_guess_format(mp4) failed");
        return -ECANCELED;
    }

    m_ContainerCtx = avformat_alloc_context();
    m_ContainerCtx->oformat = fmt;
    m_ContainerCtx->url = av_strdup(outname.c_str());

    ret = avio_open(&m_ContainerCtx->pb, m_ContainerCtx->url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOG_AVERROR("avio_open", ret);
        goto free_format;
    }

    // Init video
    ret = initVideo();
    if (ret < 0) {
        goto close_avio;
    }

    // Init audio
    ret = initAudio();
    if (ret < 0) {
        goto clear_video;
    }

    // Write container header
    ret = avformat_write_header(m_ContainerCtx, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avformat_write_header", ret);
        goto close_avio;
    }

    return 0;

clear_video:
    clearVideo();
close_avio:
    avio_close(m_ContainerCtx->pb);
free_format:
    avformat_free_context(m_ContainerCtx);
    m_ContainerCtx = nullptr;

    return -ECANCELED;
}

int VideoRecorder::stop()
{
    int ret;

    ret = av_write_trailer(m_ContainerCtx);
    if (ret < 0) {
        LOG_AVERROR("av_write_trailer", ret);
    }

    clearAudio();
    clearVideo();

    avio_close(m_ContainerCtx->pb);
    avformat_free_context(m_ContainerCtx);

    return 0;
}

int VideoRecorder::getAudioFrameSize() const
{
    return m_AudioCodecCtx->frame_size;
}

bool VideoRecorder::onFrameReceived(const std::shared_ptr<Frame>& inputFrame)
{
    switch (inputFrame->type) {
    case FrameType::video:
        return onVideoFrameReceived(inputFrame);

    case FrameType::audio:
        return onAudioFrameReceived(inputFrame);

    default:
        return true;
    }
}

bool VideoRecorder::onVideoFrameReceived(
    const std::shared_ptr<Frame>& inputFrame)
{
    const int rgbStride = m_SnesConfig.displayWidth * kRgbSampleSize;
    const uint8_t* data;
    int ret;

    // Alloc YUV frame
    AVFrame* avFrame;
    avFrame = av_frame_alloc();

    avFrame->width = m_SnesConfig.displayWidth;
    avFrame->height = m_SnesConfig.displayHeight;
    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->pts = m_VideoFrameIdx++;

    ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_AVERROR("av_frame_get_buffer", ret);
        goto free_frame;
    }

    // Do convert
    data = inputFrame->payload.data();

    ret = sws_scale(
        m_VideoSwsCtx,
        &data,
        &rgbStride,
        0,
        m_SnesConfig.displayHeight,
        avFrame->data,
        avFrame->linesize);
    if (ret != m_SnesConfig.displayHeight) {
        LOGW(TAG, "sws_scale() returned an unexpected value");
        goto free_frame;
    }

    // Encode frame
    ret = avcodec_send_frame(m_VideoCodecCtx, avFrame);
    if (ret < 0) {
        LOG_AVERROR("avcodec_send_frame", ret);
        goto free_frame;
    }

    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();

        ret = avcodec_receive_packet(m_VideoCodecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            LOG_AVERROR("avcodec_receive_packet", ret);
            av_packet_free(&pkt);
            goto free_frame;
        }

        pkt->stream_index = m_VideoStream->index;
        av_packet_rescale_ts(
            pkt, m_VideoCodecCtx->time_base, m_VideoStream->time_base);

        ret = av_interleaved_write_frame(m_ContainerCtx, pkt);
        if (ret < 0) {
            LOG_AVERROR("av_interleaved_write_frame", ret);
            av_packet_free(&pkt);
            goto free_frame;
        }

        av_packet_free(&pkt);
    }

    // Clean AVFrame
    av_frame_free(&avFrame);

    return true;

free_frame:
    av_frame_free(&avFrame);

    return false;
}

bool VideoRecorder::onAudioFrameReceived(
    const std::shared_ptr<Frame>& inputFrame)
{
    int ret;

    void* payload = inputFrame->payload.data();

    ret = av_audio_fifo_write(m_AudioFifo, &payload, inputFrame->sampleCount);
    if (ret < 0) {
        LOG_AVERROR("av_audio_fifo_write", ret);
        return false;
    }

    while (av_audio_fifo_size(m_AudioFifo) >= m_AudioCodecCtx->frame_size) {
        AVFrame* avFrame = av_frame_alloc();

        // Prepare raw input sample
        avFrame->format = AV_SAMPLE_FMT_S16;
        avFrame->channels = m_SnesConfig.audioChannels;
        avFrame->channel_layout = AV_CH_LAYOUT_STEREO;
        avFrame->sample_rate = m_SnesConfig.audioSampleRate;
        avFrame->nb_samples = m_AudioSnesFrameSize;

        ret = av_frame_get_buffer(avFrame, 0);
        if (ret < 0) {
            LOG_AVERROR("av_frame_get_buffer", ret);
            av_frame_free(&avFrame);
            return false;
        }

        ret = av_audio_fifo_read(
            m_AudioFifo,
            reinterpret_cast<void**>(avFrame->data),
            m_AudioSnesFrameSize);
        if (ret < 0) {
            LOG_AVERROR("av_audio_fifo_read", ret);
            av_frame_free(&avFrame);
            return false;
        }

        avFrame->pts = m_AudioInSamples;
        m_AudioInSamples += m_AudioSnesFrameSize;

        // Encode
        ret = encodeAudioFrame(avFrame);
        av_frame_free(&avFrame);
        if (ret < 0) {
            return false;
        }
    }

    return true;
}

int VideoRecorder::encodeAudioFrame(AVFrame* avFrameSnes)
{
    AVFrame* avFrameResampled;
    uint8_t** dstData = nullptr;
    int dstLinesize;
    int64_t dstSamples;
    int ret;

    // Resampled frame
    avFrameResampled = av_frame_alloc();

    avFrameResampled->format = m_AudioCodecCtx->sample_fmt;
    avFrameResampled->channel_layout = m_AudioCodecCtx->channel_layout;
    avFrameResampled->sample_rate = m_AudioCodecCtx->sample_rate;
    avFrameResampled->nb_samples = m_AudioCodecCtx->frame_size;

    ret = av_frame_get_buffer(avFrameResampled, 0);
    if (ret < 0) {
        LOG_AVERROR("av_frame_get_buffer", ret);
        goto free_frame;
    }

    av_frame_make_writable(avFrameResampled);

    ret = swr_convert(
        m_AudioSwrCtx,
        avFrameResampled->data,
        avFrameResampled->nb_samples,
        (const uint8_t**)avFrameSnes->data,
        avFrameSnes->nb_samples);
    if (ret < 0) {
        LOG_AVERROR("swr_convert", ret);
        goto free_frame;
    }

    avFrameResampled->pts = m_AudioOutSamples;
    m_AudioOutSamples += ret;

    // Encode frame
    ret = avcodec_send_frame(m_AudioCodecCtx, avFrameResampled);
    if (ret < 0) {
        LOG_AVERROR("avcodec_send_frame", ret);
        goto free_frame;
    }

    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();

        ret = avcodec_receive_packet(m_AudioCodecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            LOG_AVERROR("avcodec_receive_packet", ret);
            av_packet_free(&pkt);
            goto free_frame;
        }

        av_packet_rescale_ts(
            pkt, m_AudioCodecCtx->time_base, m_AudioStream->time_base);
        pkt->stream_index = m_AudioStream->index;

        ret = av_interleaved_write_frame(m_ContainerCtx, pkt);
        if (ret < 0) {
            LOG_AVERROR("av_interleaved_write_frame", ret);
            av_packet_free(&pkt);
            goto free_frame;
        }

        av_packet_free(&pkt);
    }

    av_frame_free(&avFrameResampled);

    return true;

free_frame:
    av_frame_free(&avFrameResampled);

    return ret;
}

} // namespace msfce::recorder
