#include "controller.h"
#include "frontend_dummy.h"

FrontendDummy::FrontendDummy()
    : Frontend()
{
}

int FrontendDummy::init()
{
    return 0;
}

int FrontendDummy::run()
{
    return 0;
}
