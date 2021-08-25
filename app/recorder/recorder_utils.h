#pragma once

#include <msfce/core/log.h>

#define LOG_AVERROR(func, ret)                   \
    do {                                         \
        char err[128];                           \
        av_strerror(ret, err, sizeof(err));      \
        LOGW(TAG, "%s() failed: %s", func, err); \
    } while (0);

namespace msfce::recorder {

constexpr int kRgbSampleSize = 3;

} // namespace msfce::recorder
