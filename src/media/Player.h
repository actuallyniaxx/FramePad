#pragma once

#include "media/MediaTypes.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace fp {

class AudioOutput;
class MediaReader;

// Plays one media file. A worker thread decodes ahead into small queues; the audio device pulls
// samples from its own thread; a GUI-thread timer picks the video frame that matches the audio clock
// (or the wall clock for files without sound) and hands it out through frameReady().
class Player : public QObject {
    Q_OBJECT
public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    bool open(const QString &path, QString *error);
    void close();

    bool isOpen() const { return m_reader != nullptr; }
    bool isPlaying() const { return m_playing; }
    const MediaInfo &info() const { return m_info; }
    double position() const { return m_position; }

    void play();
    void pause();
    void togglePlay();
    void seek(double seconds);
    // Moves by whole frames from the frame on screen; pauses first.
    void stepFrames(int count);

signals:
    void frameReady(fp::VideoFramePtr frame);
    void positionChanged(double seconds);
    void playingChanged(bool playing);

private:
    void decodeLoop();
    void tick();
    double clock() const;
    bool queuesWantMoreLocked() const;
    int pullAudio(float *out, int frames, double *endTime);

    std::unique_ptr<MediaReader> m_reader;
    std::unique_ptr<AudioOutput> m_audio;
    MediaInfo m_info;
    QTimer m_timer;

    // Shared with the decode and audio threads, guarded by m_mutex.
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::thread m_thread;
    std::deque<VideoFramePtr> m_videoQueue;
    std::deque<AudioChunk> m_audioQueue;
    int m_audioOffset = 0;      // frames of the front chunk already played
    long long m_audioQueued = 0; // frames in the queue, including the played part of the front chunk
    bool m_stop = false;
    bool m_finished = false; // the decoder reached the end of the file
    bool m_seekPending = false;
    double m_seekTarget = 0.0;
    unsigned m_generation = 0;       // bumped by every seek
    unsigned m_workerGeneration = 0; // the seek the decoder has caught up with

    // Set before the decode thread starts.
    bool m_decodeAudio = false;

    // GUI thread only.
    bool m_playing = false;
    bool m_atEnd = false;
    bool m_useAudioClock = false;
    bool m_audioDrained = false; // no audio left (end of file): the wall clock takes over
    bool m_needFrame = false; // show the next frame even if it is not due (after open or seek)
    double m_position = 0.0;
    double m_shownPts = 0.0;
    double m_wallBase = 0.0;
    QElapsedTimer m_wallTimer;
    QElapsedTimer m_lastReport;
};

} // namespace fp
