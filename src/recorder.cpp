#include <assert.h>
#include <errno.h>
#include <time.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/pixdesc.h>
    #include <libswscale/swscale.h>
}

#include "log.h"
#include "recorder.h"

#define TAG "Recorder"

#define LOG_AVERROR(func, ret) \
    do { \
        char err[128]; \
        av_strerror(ret, err, sizeof(err)); \
        LOGW(TAG, "%s() failed: %s", func, err); \
    } while(0);

constexpr int kRgbSampleSize = 3;

namespace {

std::string getDate()
{
    time_t t;
    struct tm localTm;

    time(&t);
    localtime_r(&t, &localTm);

    char s[128];
    strftime(s, sizeof(s), "%F %T", &localTm);

    return s;
}

} // anonymous namespace

Recorder::FrameRecorder::FrameRecorder(std::unique_ptr<FrameRecorderBackend> backend)
    : m_Backend(std::move(backend))
{
}

void Recorder::FrameRecorder::pushFrame(const std::shared_ptr<Frame>& frame)
{
    std::unique_lock<std::mutex> lock(m_Mtx);

    m_Queue.push(frame);
    m_Cv.notify_one();
}

std::shared_ptr<Recorder::Frame> Recorder::FrameRecorder::popFrame()
{
    std::unique_lock<std::mutex> lock(m_Mtx);

    m_Cv.wait(lock, [this](){ return !m_Queue.empty(); });

    auto frame = m_Queue.back();
    m_Queue.pop();

    return frame;
}

void Recorder::FrameRecorder::start()
{
    m_State = State::started;
    m_Thread = std::thread(&FrameRecorder::threadEntry, this);
}

void Recorder::FrameRecorder::stop()
{
    if (m_Thread.joinable()) {
        pushFrame(nullptr);
        m_Thread.join();
    }

    m_State = State::stopped;
}

bool Recorder::FrameRecorder::waitForStop() const
{
    return m_State == State::stopPending;
}

void Recorder::FrameRecorder::threadEntry()
{
    m_Backend->start();

    while (auto frame = popFrame()) {
        bool ret = m_Backend->onFrameReceived(frame);
        if (!ret) {
            break;
        }
    }

    m_Backend->stop();
    m_State = State::stopPending;
}


Recorder::ImageRecorder::ImageRecorder(const std::string& basename, int frameWidth, int frameHeight)
    : m_Basename(basename),
      m_FrameWidth(frameWidth),
      m_FrameHeight(frameHeight)
{
}

int Recorder::ImageRecorder::start()
{
    return 0;
}

int Recorder::ImageRecorder::stop()
{
    return 0;
}

bool Recorder::ImageRecorder::onFrameReceived(const std::shared_ptr<Frame>& rgbFrame)
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
    codecCtx->width = m_FrameWidth;
    codecCtx->height = m_FrameHeight;
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

    avFrame->width = m_FrameWidth;
    avFrame->height = m_FrameHeight;
    avFrame->format = AV_PIX_FMT_RGB24;

    ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_AVERROR("av_frame_get_buffer", ret);
        goto free_avframe;
    }

    memcpy(avFrame->data[0], rgbFrame->data(), avFrame->width * avFrame->height * kRgbSampleSize);

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

Recorder::VideoRecorder::VideoRecorder(const std::string& basename, int frameWidth, int frameHeight, int framerate)
    : m_Basename(basename),
    m_FrameWidth(frameWidth),
    m_FrameHeight(frameHeight),
    m_Framerate(framerate)
{
}

int Recorder::VideoRecorder::start()
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

    m_FormatCtx = avformat_alloc_context();
    m_FormatCtx->oformat = fmt;
    m_FormatCtx->url = av_strdup(outname.c_str());

    ret = avio_open(&m_FormatCtx->pb, m_FormatCtx->url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOG_AVERROR("avio_open", ret);
        goto free_format;
    }

    // Create encoder
    codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if (!codec) {
        LOGW(TAG, "Failed to find a valid H264 encoder");
        goto close_avio;
    }

    m_CodecCtx = avcodec_alloc_context3(codec);
    if (!m_CodecCtx) {
        LOGW(TAG, "avcodec_alloc_context3() failed");
        goto close_avio;
    }

    m_CodecCtx->time_base.num = 1;
    m_CodecCtx->time_base.den = m_Framerate;
    m_CodecCtx->width = m_FrameWidth;
    m_CodecCtx->height = m_FrameHeight;
    m_CodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Add and setup video stream
    m_VideoStream = avformat_new_stream(m_FormatCtx, 0);
    if (!m_VideoStream) {
        LOGW(TAG, "avformat_new_stream() failed");
        goto free_codecctx;
    }

    if (m_FormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_CodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_CodecCtx, codec, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avcodec_open2", ret);
        goto close_codec;
    }

    ret = avcodec_parameters_from_context(m_VideoStream->codecpar, m_CodecCtx);
    if (ret < 0) {
        LOG_AVERROR("avcodec_parameters_from_context", ret);
        goto close_codec;
    }

    // Write container header
    ret = avformat_write_header(m_FormatCtx, nullptr);
    if (ret < 0) {
        LOG_AVERROR("avformat_write_header", ret);
        goto close_codec;
    }

    // Create RGB => YUV420 converter
    m_SwsCtx = sws_getContext(
        m_FrameWidth, m_FrameHeight, AV_PIX_FMT_RGB24,
        m_FrameWidth, m_FrameHeight, AV_PIX_FMT_YUV420P,
        0, nullptr, nullptr, nullptr);
    if (!m_SwsCtx) {
        LOGW(TAG, "sws_getContext() failed");
        goto close_codec;
    }

    return 0;

close_codec:
    avcodec_close(m_CodecCtx);
free_codecctx:
    avcodec_free_context(&m_CodecCtx);
    m_CodecCtx = nullptr;
close_avio:
    avio_close(m_FormatCtx->pb);
free_format:
    avformat_free_context(m_FormatCtx);
    m_FormatCtx = nullptr;

    return -ECANCELED;
}

int Recorder::VideoRecorder::stop()
{
    int ret;

    ret = av_write_trailer(m_FormatCtx);
    if (ret < 0) {
        LOG_AVERROR("av_write_trailer", ret);
    }

    sws_freeContext(m_SwsCtx);

    avcodec_close(m_CodecCtx);
    avcodec_free_context(&m_CodecCtx);

    avio_close(m_FormatCtx->pb);
    avformat_free_context(m_FormatCtx);

    return 0;
}

bool Recorder::VideoRecorder::onFrameReceived(const std::shared_ptr<Frame>& rgbFrame)
{
    const int rgbStride = m_FrameWidth * kRgbSampleSize;
    const uint8_t* data;
    int ret;

    // Alloc YUV frame
    AVFrame* avFrame;
    avFrame = av_frame_alloc();

    avFrame->width = m_FrameWidth;
    avFrame->height = m_FrameHeight;
    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->pts = m_FrameIdx++;

    ret = av_frame_get_buffer(avFrame, 0);
    if (ret < 0) {
        LOG_AVERROR("av_frame_get_buffer", ret);
        goto free_frame;
    }

    // Do convert
    data = rgbFrame->data();

    ret = sws_scale(
        m_SwsCtx,
        &data, &rgbStride, 0, m_FrameHeight,
        avFrame->data, avFrame->linesize);
    if (ret != m_FrameHeight) {
        LOGW(TAG, "sws_scale() returned an unexpected value");
        goto free_frame;
    }

    // Encode frame
    ret = avcodec_send_frame(m_CodecCtx, avFrame);
    if (ret < 0) {
        LOG_AVERROR("avcodec_send_frame", ret);
        goto free_frame;
    }

    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();

        ret = avcodec_receive_packet(m_CodecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;
        } else if (ret < 0) {
            LOG_AVERROR("avcodec_receive_packet", ret);
            av_packet_free(&pkt);
            goto free_frame;
        }

        av_packet_rescale_ts(pkt, m_CodecCtx->time_base, m_VideoStream->time_base);

        ret = av_interleaved_write_frame(m_FormatCtx, pkt);
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

Recorder::Recorder(int width, int height, int framerate, const std::string& basename)
    : m_FrameWidth(width),
      m_FrameHeight(height),
      m_Framerate(framerate),
      m_Basename(basename),
      m_ImgSize(width * height * kRgbSampleSize)
{
}

Recorder::~Recorder()
{
    if (m_ImageRecorder) {
        m_ImageRecorder->stop();
        m_ImageRecorder.reset();
    }

    if (m_VideoRecorder) {
        m_VideoRecorder->stop();
        m_VideoRecorder.reset();
    }

    LOGD(TAG, "Destroyed");
}

void Recorder::scanStarted()
{
    m_Started = true;

    m_BackBuffer = std::make_shared<Frame>(m_ImgSize);
    m_BackBufferWritter = m_BackBuffer->data();
}

void Recorder::drawPixel(const SnesColor& c)
{
    if (!m_Started) {
        return;
    }

    assert(m_BackBuffer);
    assert(m_BackBufferWritter);

    m_BackBufferWritter[0] = c.r;
    m_BackBufferWritter[1] = c.g;
    m_BackBufferWritter[2] = c.b;

    m_BackBufferWritter += 3;
}

void Recorder::scanEnded()
{
    if (!m_Started) {
        return;
    }

    if (m_VideoRecorder) {
        m_VideoRecorder->pushFrame(m_BackBuffer);
    }

    if (m_ImageRecorder) {
        m_ImageRecorder->pushFrame(m_BackBuffer);
    }

    m_BackBuffer = nullptr;
}

bool Recorder::active()
{
    bool ret = false;

    if (m_ImageRecorder) {
        if (m_ImageRecorder->waitForStop()) {
            m_ImageRecorder->stop();
            m_ImageRecorder.reset();
        } else {
            ret = true;
        }
    }

    if (m_VideoRecorder) {
        if (m_VideoRecorder->waitForStop()) {
            m_VideoRecorder->stop();
            m_VideoRecorder.reset();
        } else {
            ret = true;
        }
    }

    return ret;
}

void Recorder::toggleVideoRecord()
{
    LOGI(TAG, "Toggle video record");

    if (m_VideoRecorder) {
        m_VideoRecorder->stop();
        m_VideoRecorder.reset();
    } else {
        m_VideoRecorder = std::make_unique<FrameRecorder>(
            std::make_unique<VideoRecorder>(getTsBasename(), m_FrameWidth, m_FrameHeight, m_Framerate));
        m_VideoRecorder->start();
    }
}

void Recorder::takeScreenshot()
{
    LOGI(TAG, "Take screenshot");

    if (m_ImageRecorder) {
        if (!m_ImageRecorder->waitForStop()) {
            LOGW(TAG, "ImageRecorder busy");
            return;
        }

        m_ImageRecorder->stop();
        m_ImageRecorder.reset();
    }

    m_ImageRecorder = std::make_unique<FrameRecorder>(
        std::make_unique<ImageRecorder>(getTsBasename(), m_FrameWidth, m_FrameHeight));
    m_ImageRecorder->start();
}

std::string Recorder::getTsBasename() const
{
    return m_Basename + " - " + getDate();
}
