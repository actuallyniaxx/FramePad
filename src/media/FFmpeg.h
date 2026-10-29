#pragma once

// FFmpeg is a C library; include it through here so every file wraps it the same way.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <QString>

namespace fp {

// av_err2str() is a C99 compound literal and does not compile as C++.
inline QString ffmpegError(int code)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buffer, sizeof buffer);
    return QString::fromUtf8(buffer);
}

} // namespace fp
