#pragma once

#include <stdint.h>
#include <memory>

struct Snes;

class Frontend {
public:
    virtual ~Frontend() = default;

    virtual int init() = 0;
    virtual int run() = 0;

    virtual void setSnes(const std::shared_ptr<Snes>& snes) = 0;
};
