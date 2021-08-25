#pragma once

#include <stdint.h>
#include <memory>

#include <msfce/core/snes.h>

class Frontend {
public:
    virtual ~Frontend() = default;

    virtual int init(const std::shared_ptr<msfce::core::Snes>& snes) = 0;
    virtual int run() = 0;
};
