#pragma once

#include "media/MediaTypes.h"

#include <QColor>
#include <QImage>

namespace fp {

// Scales a decoded frame to fit in maxSize, keeping its display aspect ratio.
QImage frameToImage(const VideoFrame &frame, QSize maxSize);

// Decodes the frame shown a tenth of the way into the clip. Returns a null image on failure.
QImage makeThumbnail(const QString &path, QSize maxSize, QString *error = nullptr);

// Peak level (0..1) of each of `buckets` equal slices of the file's audio, both channels mixed.
std::vector<float> computePeaks(const QString &path, int buckets, QString *error = nullptr);

QImage renderWaveform(const std::vector<float> &peaks, QSize size, QColor color);

// "HH:MM:SS:FF" with frames counted at the nominal rate (29.97 counts 30 frames a second).
QString formatTimecode(double seconds, double fps);

} // namespace fp
