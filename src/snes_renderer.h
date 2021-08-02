#pragma once

#include <stdint.h>

struct SnesColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class SnesRenderer {
public:
    virtual ~SnesRenderer() = default;

    virtual void scanStarted() = 0;
    virtual void drawPixel(const SnesColor& c) = 0;
    virtual void scanEnded() = 0;

    virtual void playAudioSamples(const uint8_t* data, size_t sampleCount) = 0;
};
