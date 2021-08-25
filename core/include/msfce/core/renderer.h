#pragma once

#include <stdlib.h>
#include <stdint.h>

namespace msfce::core {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void scanStarted() = 0;
    virtual void drawPixel(const Color& c) = 0;
    virtual void scanEnded() = 0;

    virtual void playAudioSamples(const uint8_t* data, size_t sampleCount) = 0;
};

} // namespace msfce::core
