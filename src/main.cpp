#include "frontend_sdl2.h"
#include "log.h"
#include "snes.h"

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
    frontend->init();

    // Create and run SNES
    const char* romPath = argv[1];

    Snes snes(frontend);
    snes.plugCartidge(romPath);
    snes.run();

    frontend.reset();

    return 0;
}
