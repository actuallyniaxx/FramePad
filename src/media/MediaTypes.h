#pragma once

#include <QByteArray>
#include <QString>

#include <memory>
#include <vector>

namespace fp {

// Every decoded audio stream is resampled to this format before it reaches the player,
// so the audio output, waveforms and (later) the mixer only ever see one layout.
constexpr int AudioRate = 48000;
constexpr int AudioChannels = 2; // interleaved float, left/right

struct MediaInfo {
    QString path;
    double duration = 0.0; // seconds

    bool hasVideo = false;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    QString videoCodec;

    bool hasAudio = false;
    int sampleRate = 0;
    int channels = 0;
    QString audioCodec;
};

// A decoded picture as planar 8-bit YUV 4:2:0 with tightly packed rows
// (planes[0] is width*height, planes[1] and planes[2] are chromaWidth()*chromaHeight()).
struct VideoFrame {
    double pts = 0.0; // seconds from the start of the file
    int width = 0;
    int height = 0;
    double pixelAspect = 1.0;
    bool bt709 = true;      // colour matrix; false means BT.601
    bool fullRange = false; // 0-255 instead of 16-235
    QByteArray planes[3];

    int chromaWidth() const { return (width + 1) / 2; }
    int chromaHeight() const { return (height + 1) / 2; }
};
using VideoFramePtr = std::shared_ptr<const VideoFrame>;

struct AudioChunk {
    double pts = 0.0;          // seconds of the first sample
    std::vector<float> samples; // interleaved, AudioChannels per frame, at AudioRate

    int frames() const { return int(samples.size() / AudioChannels); }
};

} // namespace fp
