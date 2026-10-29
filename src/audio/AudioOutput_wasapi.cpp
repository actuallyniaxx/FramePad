#include "audio/AudioOutput.h"

#include "media/MediaTypes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

#include <QCoreApplication>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>

namespace fp {

namespace {

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

// Enough to ride out a busy GUI thread; the clock accounts for it, so it only costs responsiveness.
constexpr REFERENCE_TIME BufferDuration = 80 * 10000; // 80 ms in 100 ns units

} // namespace

struct AudioOutput::Impl {
    Source source;
    std::thread thread;
    HANDLE wake = nullptr;
    std::atomic<bool> quit{false};
    std::atomic<bool> wantRunning{false};
    std::atomic<bool> failed{false};
    bool open = false;

    // The clock: basePos was audible at baseTime. While running it moves with real time,
    // but never further than maxAdvance, which is how much audio is actually queued.
    mutable std::mutex mutex;
    bool resetPending = false;
    double basePos = 0.0;
    Clock::time_point baseTime = Clock::now();
    double maxAdvance = 0.0;
    bool clockRunning = false;

    double positionLocked(Clock::time_point now) const
    {
        if (!clockRunning)
            return basePos;
        const double elapsed = std::chrono::duration<double>(now - baseTime).count();
        return basePos + std::clamp(elapsed, 0.0, maxAdvance);
    }

    void freezeLocked()
    {
        const double position = positionLocked(Clock::now());
        maxAdvance -= position - basePos;
        basePos = position;
        clockRunning = false;
    }

    void run(std::promise<QString> ready);
};

void AudioOutput::Impl::run(std::promise<QString> ready)
{
    const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HANDLE bufferEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioRenderClient> render;
    UINT32 bufferFrames = 0;

    const char *stage = "CoCreateInstance";
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) {
        stage = "GetDefaultAudioEndpoint";
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr)) {
        stage = "Activate";
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void **>(client.GetAddressOf()));
    }
    if (SUCCEEDED(hr)) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = AudioChannels;
        format.nSamplesPerSec = AudioRate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = WORD(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        // AUTOCONVERTPCM lets Windows resample to whatever the device mixes at.
        stage = "Initialize";
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                                    | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                BufferDuration, 0, &format, nullptr);
    }
    if (SUCCEEDED(hr)) {
        stage = "SetEventHandle";
        hr = client->SetEventHandle(bufferEvent);
    }
    if (SUCCEEDED(hr)) {
        stage = "GetBufferSize";
        hr = client->GetBufferSize(&bufferFrames);
    }
    if (SUCCEEDED(hr)) {
        stage = "GetService";
        hr = client->GetService(IID_PPV_ARGS(&render));
    }

    auto cleanup = [&] {
        render.Reset();
        client.Reset();
        device.Reset();
        enumerator.Reset();
        CloseHandle(bufferEvent);
        if (SUCCEEDED(coInit))
            CoUninitialize();
    };

    if (FAILED(hr)) {
        ready.set_value(QCoreApplication::translate("fp::Audio", "The audio device could not be opened (%1 failed with error 0x%2).")
                            .arg(QLatin1String(stage))
                            .arg(quint32(hr), 8, 16, QLatin1Char('0')));
        cleanup();
        return;
    }
    ready.set_value(QString());

    auto fill = [&]() -> bool {
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding)))
            return false;
        const UINT32 space = bufferFrames - padding;
        if (space == 0)
            return true;
        BYTE *data = nullptr;
        if (FAILED(render->GetBuffer(space, &data)))
            return false;
        auto *out = reinterpret_cast<float *>(data);
        double endTime = 0.0;
        const int got = source ? std::clamp(source(out, int(space), &endTime), 0, int(space)) : 0;
        std::fill(out + size_t(got) * AudioChannels, out + size_t(space) * AudioChannels, 0.0f);
        if (FAILED(render->ReleaseBuffer(space, 0)))
            return false;

        if (got > 0) {
            std::lock_guard lock(mutex);
            // Audio written just before a reset is about to be thrown away; it must not move the clock.
            if (!resetPending) {
                const double queued = double(padding + UINT32(got)) / AudioRate;
                basePos = endTime - queued;
                maxAdvance = queued;
                baseTime = Clock::now();
            }
        }
        return true;
    };

    const HANDLE events[2] = {wake, bufferEvent};
    bool running = false;
    bool ok = true;
    while (ok && !quit) {
        WaitForMultipleObjects(2, events, FALSE, 100);
        if (quit)
            break;

        bool doReset = false;
        {
            std::lock_guard lock(mutex);
            std::swap(doReset, resetPending);
        }
        if (doReset) {
            if (running)
                client->Stop();
            running = false;
            client->Reset();
        }

        const bool want = wantRunning;
        if (want && !running) {
            // Prefill before starting so playback does not open with a glitch.
            ok = fill() && SUCCEEDED(client->Start());
            running = ok;
            std::lock_guard lock(mutex);
            clockRunning = running;
            baseTime = Clock::now();
        } else if (!want && running) {
            client->Stop();
            running = false;
            std::lock_guard lock(mutex);
            freezeLocked();
        } else if (running) {
            ok = fill();
        }
    }

    if (!ok) {
        failed = true;
        std::lock_guard lock(mutex);
        freezeLocked();
    }
    if (running)
        client->Stop();
    cleanup();
}

AudioOutput::AudioOutput()
    : d(std::make_unique<Impl>())
{
}

AudioOutput::~AudioOutput()
{
    close();
}

bool AudioOutput::open(Source source, QString *error)
{
    close();
    d->source = std::move(source);
    d->quit = false;
    d->failed = false;
    d->wantRunning = false;
    d->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    std::promise<QString> ready;
    std::future<QString> result = ready.get_future();
    d->thread = std::thread(&Impl::run, d.get(), std::move(ready));
    const QString failure = result.get();
    if (!failure.isEmpty()) {
        d->thread.join();
        CloseHandle(d->wake);
        d->wake = nullptr;
        if (error)
            *error = failure;
        return false;
    }
    d->open = true;
    return true;
}

void AudioOutput::close()
{
    if (d->thread.joinable()) {
        d->quit = true;
        SetEvent(d->wake);
        d->thread.join();
    }
    if (d->wake) {
        CloseHandle(d->wake);
        d->wake = nullptr;
    }
    d->open = false;
    std::lock_guard lock(d->mutex);
    d->resetPending = false;
    d->basePos = 0.0;
    d->maxAdvance = 0.0;
    d->clockRunning = false;
}

bool AudioOutput::isOpen() const
{
    return d->open;
}

bool AudioOutput::failed() const
{
    return d->failed;
}

void AudioOutput::start()
{
    if (!d->open)
        return;
    d->wantRunning = true;
    SetEvent(d->wake);
}

void AudioOutput::stop()
{
    if (!d->open)
        return;
    d->wantRunning = false;
    SetEvent(d->wake);
}

void AudioOutput::reset(double position)
{
    if (!d->open)
        return;
    {
        // Set the clock here rather than on the audio thread, so clock() is right immediately.
        std::lock_guard lock(d->mutex);
        d->resetPending = true;
        d->basePos = position;
        d->maxAdvance = 0.0;
        d->clockRunning = false;
    }
    SetEvent(d->wake);
}

double AudioOutput::clock() const
{
    std::lock_guard lock(d->mutex);
    return d->positionLocked(Clock::now());
}

} // namespace fp
