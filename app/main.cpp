#include <msfce/core/log.h>
#include <msfce/core/snes.h>
#include "frontend_sdl2.h"

#define TAG "main"

int main(int argc, char* argv[])
{
    int ret;

    if (argc < 2) {
        LOGE(TAG, "Missing ROM arg");
        return 1;
    }

    //logSetLevel(LOG_DEBUG);

    LOGI(TAG, "Welcome to Monkey Super Famicom Emulator");

    // Create frontend
    auto frontend = std::make_shared<FrontendSdl2>();

    // Create and run SNES
    const char* romPath = argv[1];

    auto snes = msfce::core::Snes::create();
    snes->addRenderer(frontend);
    snes->plugCartidge(romPath);

    snes->start();

    // Run frontend
    frontend->init(snes);
    frontend->run();

    snes->stop();

    snes->removeRenderer(frontend);
    frontend.reset();

    return 0;
}
