#pragma once

#include "frontend.h"

class FrontendDummy : public Frontend {
public:
    FrontendDummy();
    ~FrontendDummy() = default;

    int init() override;
    bool runOnce() override;

    std::shared_ptr<SnesController> getController1() override;

    void drawPoint(int x, int y, const Color& c) override;
    void present() override;

private:
    std::shared_ptr<SnesController> m_Controller1;
};
