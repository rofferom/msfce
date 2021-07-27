#pragma once

#include "frontend.h"

class FrontendDummy : public Frontend {
public:
    FrontendDummy();
    ~FrontendDummy() = default;

    int init() override;
    int run() override;
};
