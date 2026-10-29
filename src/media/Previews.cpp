#include "media/Previews.h"

#include "media/FFmpeg.h"
#include "media/MediaReader.h"

#include <QCoreApplication>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace fp {

QImage frameToImage(const VideoFrame &frame, QSize maxSize)
{
    if (frame.width <= 0 || frame.height <= 0)
        return {};
    const QSize display = QSize(qRound(frame.width * frame.pixelAspect), frame.height)
                              .scaled(maxSize, Qt::KeepAspectRatio)
                              .expandedTo(QSize(1, 1));

    SwsContext *sws = sws_getContext(frame.width, frame.height, AV_PIX_FMT_YUV420P, display.width(),
                                     display.height(), AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws)
        return {};
    const int *coefficients = sws_getCoefficients(frame.bt709 ? SWS_CS_ITU709 : SWS_CS_ITU601);
    sws_setColorspaceDetails(sws, coefficients, frame.fullRange ? 1 : 0, sws_getCoefficients(SWS_CS_DEFAULT), 1,
                             0, 1 << 16, 1 << 16);

    // Format_RGB32 is B, G, R, A in memory on little-endian machines, which is what BGRA writes.
    QImage image(display, QImage::Format_RGB32);
    const uint8_t *src[4] = {
        reinterpret_cast<const uint8_t *>(frame.planes[0].constData()),
        reinterpret_cast<const uint8_t *>(frame.planes[1].constData()),
        reinterpret_cast<const uint8_t *>(frame.planes[2].constData()),
        nullptr,
    };
    const int srcStride[4] = {frame.width, frame.chromaWidth(), frame.chromaWidth(), 0};
    uint8_t *dst[4] = {image.bits(), nullptr, nullptr, nullptr};
    const int dstStride[4] = {int(image.bytesPerLine()), 0, 0, 0};
    sws_scale(sws, src, srcStride, 0, frame.height, dst, dstStride);
    sws_freeContext(sws);
    return image;
}

QImage makeThumbnail(const QString &path, QSize maxSize, QString *error)
{
    MediaReader reader;
    if (!reader.open(path, error))
        return {};
    if (!reader.info().hasVideo) {
        if (error)
            *error = QCoreApplication::translate("fp::Media", "The file has no video.");
        return {};
    }
    reader.setAudioEnabled(false);

    VideoFramePtr first;
    reader.onVideo = [&first](VideoFramePtr frame) {
        if (!first)
            first = std::move(frame);
    };
    // A tenth of the way in skips black openings and fades, but never more than a few seconds of decoding.
    const double at = std::min(reader.info().duration * 0.1, 5.0);
    if (at > 0.0)
        reader.seek(at);
    while (!first && reader.step()) {
    }
    if (!first) {
        if (error)
            *error = QCoreApplication::translate("fp::Media", "No picture could be decoded.");
        return {};
    }
    return frameToImage(*first, maxSize);
}

std::vector<float> computePeaks(const QString &path, int buckets, QString *error)
{
    MediaReader reader;
    if (!reader.open(path, error))
        return {};
    const double duration = reader.info().duration;
    if (!reader.info().hasAudio || duration <= 0.0 || buckets <= 0) {
        if (error)
            *error = QCoreApplication::translate("fp::Media", "The file has no audio.");
        return {};
    }
    reader.setVideoEnabled(false);

    std::vector<float> peaks(size_t(buckets), 0.0f);
    reader.onAudio = [&](AudioChunk &&chunk) {
        const int frames = chunk.frames();
        for (int i = 0; i < frames; ++i) {
            const double t = chunk.pts + double(i) / AudioRate;
            const int bucket = std::clamp(int(t / duration * buckets), 0, buckets - 1);
            float peak = 0.0f;
            for (int c = 0; c < AudioChannels; ++c)
                peak = std::max(peak, std::abs(chunk.samples[size_t(i) * AudioChannels + c]));
            peaks[size_t(bucket)] = std::max(peaks[size_t(bucket)], std::min(peak, 1.0f));
        }
    };
    while (reader.step()) {
    }
    return peaks;
}

QImage renderWaveform(const std::vector<float> &peaks, QSize size, QColor color)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (peaks.empty() || size.isEmpty())
        return image;

    QPainter painter(&image);
    painter.setPen(color);
    const int middle = size.height() / 2;
    for (int x = 0; x < size.width(); ++x) {
        const size_t bucket = size_t(x) * peaks.size() / size_t(size.width());
        const int half = std::max(1, int(peaks[bucket] * middle));
        painter.drawLine(x, middle - half, x, middle + half);
    }
    return image;
}

QString formatTimecode(double seconds, double fps)
{
    const int nominal = std::max(1, int(std::lround(fps)));
    const long long total = std::max(0LL, (long long)std::floor(seconds * nominal + 1e-6));
    const long long frames = total % nominal;
    const long long secs = total / nominal;
    return QStringLiteral("%1:%2:%3:%4")
        .arg(secs / 3600, 2, 10, QLatin1Char('0'))
        .arg(secs / 60 % 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0'))
        .arg(frames, 2, 10, QLatin1Char('0'));
}

} // namespace fp
