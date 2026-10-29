#pragma once

#include "media/MediaTypes.h"

#include <functional>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;
struct SwsContext;

namespace fp {

// Demuxes and decodes one media file with FFmpeg. Not thread-safe: one thread drives it,
// calling step() repeatedly; decoded output is handed to the callbacks as it appears.
class MediaReader {
public:
    MediaReader();
    ~MediaReader();
    MediaReader(const MediaReader &) = delete;
    MediaReader &operator=(const MediaReader &) = delete;

    bool open(const QString &path, QString *error);
    const MediaInfo &info() const { return m_info; }

    // Decoding a stream nobody needs is wasted work (thumbnails skip audio, waveforms skip video).
    void setVideoEnabled(bool enabled) { m_videoEnabled = enabled; }
    void setAudioEnabled(bool enabled) { m_audioEnabled = enabled; }

    std::function<void(VideoFramePtr)> onVideo;
    std::function<void(AudioChunk &&)> onAudio;

    // Reads and decodes one packet. Returns false once the file is finished and the decoders are drained.
    bool step();

    // Jumps so the next output starts at `seconds`; anything decoded before it is discarded.
    void seek(double seconds);

private:
    void close();
    void receiveVideo();
    void receiveAudio();
    VideoFramePtr convertVideo(const AVFrame *frame, double pts);
    double toSeconds(long long timestamp, int streamIndex) const;

    MediaInfo m_info;
    AVFormatContext *m_format = nullptr;
    AVCodecContext *m_video = nullptr;
    AVCodecContext *m_audio = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;
    SwsContext *m_sws = nullptr;
    SwrContext *m_swr = nullptr;
    int m_videoStream = -1;
    int m_audioStream = -1;
    bool m_videoEnabled = true;
    bool m_audioEnabled = true;
    bool m_finished = false;
    double m_startTime = 0.0; // container start, subtracted so every file begins at 0
    double m_discardBefore = 0.0;
    double m_nextVideoPts = 0.0; // used when a frame comes without a timestamp
    double m_nextAudioPts = 0.0;
};

} // namespace fp
