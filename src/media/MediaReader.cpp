#include "media/MediaReader.h"

#include "media/FFmpeg.h"

#include <QCoreApplication>

#include <algorithm>

namespace fp {

namespace {

AVCodecContext *openDecoder(const AVStream *stream)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
        return nullptr;
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx)
        return nullptr;
    if (avcodec_parameters_to_context(ctx, stream->codecpar) < 0) {
        avcodec_free_context(&ctx);
        return nullptr;
    }
    ctx->pkt_timebase = stream->time_base;
    ctx->thread_count = 0; // one thread per core
    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        return nullptr;
    }
    return ctx;
}

bool isBt709(const AVFrame *frame)
{
    switch (frame->colorspace) {
    case AVCOL_SPC_BT709:
        return true;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_FCC:
        return false;
    default:
        // Untagged files: HD and up are almost always BT.709, SD is BT.601.
        return frame->height >= 720;
    }
}

} // namespace

MediaReader::MediaReader() = default;

MediaReader::~MediaReader()
{
    close();
}

bool MediaReader::open(const QString &path, QString *error)
{
    close();
    m_info = {};
    m_info.path = path;

    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        close();
        return false;
    };

    int result = avformat_open_input(&m_format, path.toUtf8().constData(), nullptr, nullptr);
    if (result < 0)
        return fail(QCoreApplication::translate("fp::Media", "The file could not be opened: %1")
                        .arg(ffmpegError(result)));
    result = avformat_find_stream_info(m_format, nullptr);
    if (result < 0)
        return fail(QCoreApplication::translate("fp::Media", "The file could not be read: %1")
                        .arg(ffmpegError(result)));

    m_startTime = m_format->start_time != AV_NOPTS_VALUE ? double(m_format->start_time) / AV_TIME_BASE : 0.0;
    if (m_format->duration != AV_NOPTS_VALUE)
        m_info.duration = double(m_format->duration) / AV_TIME_BASE;

    const int video = av_find_best_stream(m_format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    // Cover art in music files shows up as a one-picture "video" stream; it is not a clip.
    if (video >= 0 && !(m_format->streams[video]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
        AVStream *stream = m_format->streams[video];
        m_video = openDecoder(stream);
        if (m_video) {
            m_videoStream = video;
            m_info.hasVideo = true;
            m_info.width = stream->codecpar->width;
            m_info.height = stream->codecpar->height;
            m_info.fps = av_q2d(av_guess_frame_rate(m_format, stream, nullptr));
            m_info.videoCodec = QString::fromLatin1(avcodec_get_name(stream->codecpar->codec_id));
        }
    }

    const int audio = av_find_best_stream(m_format, AVMEDIA_TYPE_AUDIO, -1, m_videoStream, nullptr, 0);
    if (audio >= 0) {
        AVStream *stream = m_format->streams[audio];
        m_audio = openDecoder(stream);
        if (m_audio) {
            m_audioStream = audio;
            m_info.hasAudio = true;
            m_info.sampleRate = stream->codecpar->sample_rate;
            m_info.channels = stream->codecpar->ch_layout.nb_channels;
            m_info.audioCodec = QString::fromLatin1(avcodec_get_name(stream->codecpar->codec_id));
        }
    }

    if (!m_video && !m_audio)
        return fail(QCoreApplication::translate("fp::Media", "The file has no video or audio that FramePad can read."));

    if (m_info.duration <= 0.0) {
        const int index = m_videoStream >= 0 ? m_videoStream : m_audioStream;
        const AVStream *stream = m_format->streams[index];
        if (stream->duration != AV_NOPTS_VALUE)
            m_info.duration = double(stream->duration) * av_q2d(stream->time_base);
    }

    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    return true;
}

void MediaReader::close()
{
    sws_freeContext(m_sws);
    m_sws = nullptr;
    swr_free(&m_swr);
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
    avcodec_free_context(&m_video);
    avcodec_free_context(&m_audio);
    avformat_close_input(&m_format);
    m_videoStream = -1;
    m_audioStream = -1;
    m_finished = false;
    m_discardBefore = 0.0;
    m_nextVideoPts = 0.0;
    m_nextAudioPts = 0.0;
}

bool MediaReader::step()
{
    if (!m_format || m_finished)
        return false;

    const int result = av_read_frame(m_format, m_packet);
    if (result < 0) {
        // End of file (a read error is treated the same): drain what the decoders still hold.
        if (m_video && m_videoEnabled) {
            avcodec_send_packet(m_video, nullptr);
            receiveVideo();
        }
        if (m_audio && m_audioEnabled) {
            avcodec_send_packet(m_audio, nullptr);
            receiveAudio();
        }
        m_finished = true;
        return false;
    }

    if (m_packet->stream_index == m_videoStream && m_videoEnabled) {
        if (avcodec_send_packet(m_video, m_packet) >= 0)
            receiveVideo();
    } else if (m_packet->stream_index == m_audioStream && m_audioEnabled) {
        if (avcodec_send_packet(m_audio, m_packet) >= 0)
            receiveAudio();
    }
    av_packet_unref(m_packet);
    return true;
}

void MediaReader::seek(double seconds)
{
    if (!m_format)
        return;
    // Land on the keyframe before the target; the frames between it and the target are decoded and discarded.
    const auto target = static_cast<int64_t>((seconds + m_startTime) * AV_TIME_BASE);
    av_seek_frame(m_format, -1, target, AVSEEK_FLAG_BACKWARD);
    if (m_video)
        avcodec_flush_buffers(m_video);
    if (m_audio)
        avcodec_flush_buffers(m_audio);
    swr_free(&m_swr);
    m_finished = false;
    m_discardBefore = seconds;
    m_nextVideoPts = seconds;
    m_nextAudioPts = seconds;
}

double MediaReader::toSeconds(long long timestamp, int streamIndex) const
{
    return double(timestamp) * av_q2d(m_format->streams[streamIndex]->time_base) - m_startTime;
}

void MediaReader::receiveVideo()
{
    const double frameDuration = 1.0 / (m_info.fps > 0.0 ? m_info.fps : 30.0);
    while (avcodec_receive_frame(m_video, m_frame) >= 0) {
        const long long ts = m_frame->best_effort_timestamp;
        const double pts = ts == AV_NOPTS_VALUE ? m_nextVideoPts : toSeconds(ts, m_videoStream);
        m_nextVideoPts = pts + frameDuration;
        if (pts >= m_discardBefore - frameDuration / 2 && onVideo) {
            if (VideoFramePtr frame = convertVideo(m_frame, pts))
                onVideo(std::move(frame));
        }
        av_frame_unref(m_frame);
    }
}

VideoFramePtr MediaReader::convertVideo(const AVFrame *frame, double pts)
{
    auto out = std::make_shared<VideoFrame>();
    out->pts = pts;
    out->width = frame->width;
    out->height = frame->height;
    if (frame->sample_aspect_ratio.num > 0 && frame->sample_aspect_ratio.den > 0)
        out->pixelAspect = av_q2d(frame->sample_aspect_ratio);
    out->bt709 = isBt709(frame);
    out->fullRange = frame->color_range == AVCOL_RANGE_JPEG || frame->format == AV_PIX_FMT_YUVJ420P;

    const int widths[3] = {out->width, out->chromaWidth(), out->chromaWidth()};
    const int heights[3] = {out->height, out->chromaHeight(), out->chromaHeight()};
    uint8_t *dst[4] = {};
    int dstStride[4] = {};
    for (int p = 0; p < 3; ++p) {
        out->planes[p].resize(qsizetype(widths[p]) * heights[p]);
        dst[p] = reinterpret_cast<uint8_t *>(out->planes[p].data());
        dstStride[p] = widths[p];
    }

    if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
        // Already the layout the viewer wants: just drop the row padding.
        for (int p = 0; p < 3; ++p) {
            for (int y = 0; y < heights[p]; ++y)
                std::copy_n(frame->data[p] + qsizetype(y) * frame->linesize[p], widths[p], dst[p] + qsizetype(y) * widths[p]);
        }
    } else {
        // 10-bit, 4:2:2, 4:4:4, NV12, RGB...: let swscale bring it to 8-bit 4:2:0.
        m_sws = sws_getCachedContext(m_sws, frame->width, frame->height, AVPixelFormat(frame->format),
                                     frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR,
                                     nullptr, nullptr, nullptr);
        if (!m_sws)
            return nullptr;
        sws_scale(m_sws, frame->data, frame->linesize, 0, frame->height, dst, dstStride);
    }
    return out;
}

void MediaReader::receiveAudio()
{
    while (avcodec_receive_frame(m_audio, m_frame) >= 0) {
        const long long ts = m_frame->best_effort_timestamp;
        const double pts = ts == AV_NOPTS_VALUE ? m_nextAudioPts : toSeconds(ts, m_audioStream);

        if (!m_swr) {
            AVChannelLayout outLayout{};
            av_channel_layout_default(&outLayout, AudioChannels);
            AVChannelLayout inLayout{};
            if (m_frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
                av_channel_layout_default(&inLayout, m_frame->ch_layout.nb_channels);
            else
                av_channel_layout_copy(&inLayout, &m_frame->ch_layout);
            const int result = swr_alloc_set_opts2(&m_swr, &outLayout, AV_SAMPLE_FMT_FLT, AudioRate, &inLayout,
                                                   AVSampleFormat(m_frame->format), m_frame->sample_rate, 0, nullptr);
            av_channel_layout_uninit(&inLayout);
            av_channel_layout_uninit(&outLayout);
            if (result < 0 || swr_init(m_swr) < 0) {
                swr_free(&m_swr);
                av_frame_unref(m_frame);
                continue;
            }
        }

        AudioChunk chunk;
        chunk.pts = pts;
        const int capacity = swr_get_out_samples(m_swr, m_frame->nb_samples);
        chunk.samples.resize(size_t(std::max(capacity, 0)) * AudioChannels);
        uint8_t *out = reinterpret_cast<uint8_t *>(chunk.samples.data());
        const int got = swr_convert(m_swr, &out, capacity, m_frame->extended_data, m_frame->nb_samples);
        av_frame_unref(m_frame);
        if (got <= 0)
            continue;
        chunk.samples.resize(size_t(got) * AudioChannels);
        m_nextAudioPts = pts + double(got) / AudioRate;

        if (pts < m_discardBefore) {
            const int drop = int((m_discardBefore - pts) * AudioRate + 0.5);
            if (drop >= got)
                continue;
            chunk.samples.erase(chunk.samples.begin(), chunk.samples.begin() + qsizetype(drop) * AudioChannels);
            chunk.pts = pts + double(drop) / AudioRate;
        }
        if (onAudio)
            onAudio(std::move(chunk));
    }
}

} // namespace fp
