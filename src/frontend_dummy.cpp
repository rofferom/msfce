#include "controller.h"
#include "frontend_dummy.h"

FrontendDummy::FrontendDummy()
    : Frontend(),
      m_Controller1(std::make_shared<SnesController>())
{
}

int FrontendDummy::init()
{
    return 0;
}

bool FrontendDummy::runOnce()
{
    return true;
}

std::shared_ptr<SnesController> FrontendDummy::getController1()
{
    return m_Controller1;
}

void FrontendDummy::drawPoint(int x, int y, const Color& c)
{
}

void FrontendDummy::present()
{
}
