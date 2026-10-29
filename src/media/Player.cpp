#include "media/Player.h"

#include "audio/AudioOutput.h"
#include "media/MediaReader.h"

#include <QDebug>

#include <algorithm>

namespace fp {

namespace {

// How far the decoder runs ahead. 4K frames are 12 MB each, so video is kept short;
// the hard caps only matter for badly interleaved files.
constexpr size_t VideoQueueTarget = 8;
constexpr size_t VideoQueueCap = 40;
constexpr long long AudioQueueTarget = AudioRate / 2;
constexpr long long AudioQueueCap = AudioRate * 4;

} // namespace

Player::Player(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(4);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Player::tick);
}

Player::~Player()
{
    close();
    m_audio.reset(); // its thread calls pullAudio(), so it has to stop before the queues go away
}

bool Player::open(const QString &path, QString *error)
{
    close();
    auto reader = std::make_unique<MediaReader>();
    if (!reader->open(path, error))
        return false;
    m_info = reader->info();

    m_useAudioClock = false;
    if (m_info.hasAudio) {
        if (!m_audio)
            m_audio = std::make_unique<AudioOutput>();
        QString audioError;
        if ((m_audio->isOpen() && !m_audio->failed())
            || m_audio->open([this](float *out, int frames, double *endTime) { return pullAudio(out, frames, endTime); },
                             &audioError)) {
            m_useAudioClock = true;
            m_audio->reset(0.0);
        } else {
            qWarning().noquote() << "FramePad: no audio output:" << audioError;
        }
    }
    // Without a device nobody would drain the audio queue, so do not decode audio at all.
    m_decodeAudio = m_useAudioClock;
    reader->setAudioEnabled(m_decodeAudio);

    reader->onVideo = [this](VideoFramePtr frame) {
        std::lock_guard lock(m_mutex);
        if (m_workerGeneration == m_generation)
            m_videoQueue.push_back(std::move(frame));
    };
    reader->onAudio = [this](AudioChunk &&chunk) {
        std::lock_guard lock(m_mutex);
        if (m_workerGeneration == m_generation) {
            m_audioQueued += chunk.frames();
            m_audioQueue.push_back(std::move(chunk));
        }
    };
    m_reader = std::move(reader);

    m_finished = false;
    m_seekPending = false;
    m_generation = m_workerGeneration = 0;
    m_atEnd = false;
    m_audioDrained = false;
    m_needFrame = true;
    m_position = m_shownPts = m_wallBase = 0.0;
    m_thread = std::thread(&Player::decodeLoop, this);
    m_timer.start();
    emit positionChanged(0.0);
    return true;
}

void Player::close()
{
    if (m_thread.joinable()) {
        {
            std::lock_guard lock(m_mutex);
            m_stop = true;
        }
        m_wake.notify_all();
        m_thread.join();
    }
    m_timer.stop();
    if (m_audio && m_audio->isOpen()) {
        m_audio->stop();
        m_audio->reset(0.0);
    }
    {
        std::lock_guard lock(m_mutex);
        m_videoQueue.clear();
        m_audioQueue.clear();
        m_audioOffset = 0;
        m_audioQueued = 0;
        m_stop = false;
    }
    m_reader.reset();
    m_info = {};
    m_position = 0.0;
    if (m_playing) {
        m_playing = false;
        emit playingChanged(false);
    }
}

void Player::play()
{
    if (!isOpen() || m_playing)
        return;
    if (m_atEnd)
        seek(0.0);
    m_playing = true;
    m_wallBase = m_position;
    m_wallTimer.start();
    m_lastReport.start();
    if (m_useAudioClock)
        m_audio->start();
    emit playingChanged(true);
}

void Player::pause()
{
    if (!m_playing)
        return;
    m_position = clock();
    m_playing = false;
    m_wallBase = m_position;
    if (m_useAudioClock)
        m_audio->stop();
    emit playingChanged(false);
    emit positionChanged(m_position);
}

void Player::togglePlay()
{
    if (m_playing)
        pause();
    else
        play();
}

void Player::seek(double seconds)
{
    if (!isOpen())
        return;
    // The last frame starts one frame before the end; seeking past it would show nothing.
    const double lastFrame = m_info.hasVideo && m_info.fps > 0.0 ? 1.0 / m_info.fps : 0.0;
    const double target = std::clamp(seconds, 0.0, std::max(0.0, m_info.duration - lastFrame));
    {
        std::lock_guard lock(m_mutex);
        m_videoQueue.clear();
        m_audioQueue.clear();
        m_audioOffset = 0;
        m_audioQueued = 0;
        m_seekTarget = target;
        m_seekPending = true;
        ++m_generation;
        m_finished = false;
    }
    m_wake.notify_all();
    if (m_useAudioClock)
        m_audio->reset(target);
    m_atEnd = false;
    m_audioDrained = false;
    m_needFrame = true;
    m_position = m_wallBase = target;
    m_wallTimer.start();
    emit positionChanged(target);
}

void Player::stepFrames(int count)
{
    if (!isOpen() || !m_info.hasVideo)
        return;
    pause();
    const double fps = m_info.fps > 0.0 ? m_info.fps : 30.0;
    seek(m_shownPts + count / fps);
}

double Player::clock() const
{
    if (!m_playing)
        return m_position;
    if (m_useAudioClock && !m_audioDrained)
        return m_audio->clock();
    return m_wallBase + double(m_wallTimer.nsecsElapsed()) / 1e9;
}

bool Player::queuesWantMoreLocked() const
{
    if (m_videoQueue.size() >= VideoQueueCap || m_audioQueued >= AudioQueueCap)
        return false;
    return (m_info.hasVideo && m_videoQueue.size() < VideoQueueTarget)
        || (m_decodeAudio && m_audioQueued < AudioQueueTarget);
}

void Player::decodeLoop()
{
    std::unique_lock lock(m_mutex);
    while (!m_stop) {
        if (m_seekPending) {
            m_seekPending = false;
            const double target = m_seekTarget;
            const unsigned generation = m_generation;
            lock.unlock();
            m_reader->seek(target);
            lock.lock();
            m_workerGeneration = generation;
            m_finished = false;
            continue;
        }
        if (m_finished || !queuesWantMoreLocked()) {
            m_wake.wait(lock);
            continue;
        }
        lock.unlock();
        const bool more = m_reader->step();
        lock.lock();
        // A seek that arrived meanwhile restarts decoding, so an old end-of-file does not count.
        if (!more && m_workerGeneration == m_generation && !m_seekPending)
            m_finished = true;
    }
}

int Player::pullAudio(float *out, int frames, double *endTime)
{
    std::lock_guard lock(m_mutex);
    int written = 0;
    while (written < frames && !m_audioQueue.empty()) {
        const AudioChunk &chunk = m_audioQueue.front();
        const int count = std::min(frames - written, chunk.frames() - m_audioOffset);
        std::copy_n(chunk.samples.data() + size_t(m_audioOffset) * AudioChannels, size_t(count) * AudioChannels,
                    out + size_t(written) * AudioChannels);
        written += count;
        m_audioOffset += count;
        *endTime = chunk.pts + double(m_audioOffset) / AudioRate;
        if (m_audioOffset >= chunk.frames()) {
            m_audioQueued -= chunk.frames();
            m_audioQueue.pop_front();
            m_audioOffset = 0;
        }
    }
    if (written > 0)
        m_wake.notify_one();
    return written;
}

void Player::tick()
{
    if (!isOpen())
        return;
    if (m_useAudioClock && m_audio->failed()) {
        // The device went away mid-playback: carry on silently on the wall clock.
        m_wallBase = m_playing ? m_audio->clock() : m_position;
        m_wallTimer.start();
        m_useAudioClock = false;
    }

    const double now = clock();
    VideoFramePtr show;
    bool audioDrained = false;
    bool ended = false;
    {
        std::lock_guard lock(m_mutex);
        // Take the newest frame that is due; older due frames are late and get dropped.
        while (!m_videoQueue.empty()) {
            if (!m_needFrame && !(m_playing && m_videoQueue.front()->pts <= now))
                break;
            show = std::move(m_videoQueue.front());
            m_videoQueue.pop_front();
            m_needFrame = false;
            if (!m_playing)
                break;
        }
        if (m_decodeAudio && !m_useAudioClock) {
            while (!m_audioQueue.empty()
                   && m_audioQueue.front().pts + double(m_audioQueue.front().frames()) / AudioRate <= now) {
                m_audioQueued -= m_audioQueue.front().frames();
                m_audioQueue.pop_front();
                m_audioOffset = 0;
            }
        }
        const bool decodedAll = m_finished && m_workerGeneration == m_generation && !m_seekPending;
        audioDrained = decodedAll && m_audioQueue.empty();
        ended = audioDrained && m_videoQueue.empty();
    }

    if (show) {
        m_wake.notify_one();
        m_shownPts = show->pts;
        emit frameReady(show);
    }
    if (!m_playing)
        return;

    if (m_useAudioClock && !m_audioDrained && audioDrained) {
        // A video track that outlasts its audio must not freeze when the audio clock stops.
        m_audioDrained = true;
        m_wallBase = now;
        m_wallTimer.start();
    }
    m_position = m_info.duration > 0.0 ? std::min(now, m_info.duration) : now;
    if (ended) {
        pause();
        m_atEnd = true;
        return;
    }
    if (m_lastReport.elapsed() >= 33) {
        m_lastReport.start();
        emit positionChanged(m_position);
    }
}

} // namespace fp
