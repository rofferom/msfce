#pragma once

#include <stdint.h>
#include <memory>

struct Snes;
struct SnesController;

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Frontend {
public:
    virtual ~Frontend() = default;

    virtual int init() = 0;
    virtual bool runOnce() = 0;

    virtual std::shared_ptr<SnesController> getController1() = 0;

    virtual void drawPoint(int x, int y, const Color& c) = 0;
    virtual void present() = 0;

    virtual void setSnes(const std::shared_ptr<Snes>& snes) = 0;
};
