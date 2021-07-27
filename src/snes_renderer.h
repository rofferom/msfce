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
};
