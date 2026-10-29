#pragma once

#include <QString>

#include <functional>
#include <memory>

namespace fp {

// Plays interleaved float stereo at AudioRate on the default Windows output device (WASAPI shared mode)
// and reports which media time is coming out of the speakers, which is the clock the player syncs video to.
// All calls are made from one thread (the GUI); the device itself is driven by an internal thread.
class AudioOutput {
public:
    // Called on the audio thread. Fills up to `frames` frames, returns how many it wrote and sets
    // *endTime to the media time just after the last frame written.
    using Source = std::function<int(float *out, int frames, double *endTime)>;

    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    bool open(Source source, QString *error);
    void close();
    bool isOpen() const;
    // True after the device disappeared (headphones unplugged, driver reset); the clock stops then.
    bool failed() const;

    void start();
    void stop();
    // Drops whatever is buffered and restarts the clock at `position`. Keeps playing if it was.
    void reset(double position);

    // Media time currently audible, in seconds.
    double clock() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace fp
