#include <stdio.h>
#include <getopt.h>

#include <msfce/core/log.h>
#include <msfce/core/snes.h>
#include "frontend_sdl2/frontend_sdl2.h"

#define TAG "main"

struct Params {
    bool help = false;
    bool verbose = false;
};

int parseArgs(int argc, char* argv[], Params* params)
{
    int optionIndex = 0;
    int value;
    int ret;

    const struct option argsOptions[] = {
        {"help", optional_argument, 0, 'h'},
        {"verbose", optional_argument, 0, 'v'},
        {0, 0, 0, 0}};

    while (true) {
        value = getopt_long(argc, argv, "hv", argsOptions, &optionIndex);
        if (value == -1 || value == '?')
            break;

        switch (value) {
        case 'h':
            params->help = true;
            break;

        case 'v':
            params->verbose = true;
            break;

        default:
            break;
        }
    }

    return 0;
}

void printHelp(int argc, char* argv[])
{
    printf("Usage: %s [-h] [-v] rom\n\n", argv[0]);

    printf("positional arguments:\n");
    printf("  %-20s %s\n", "rom", "Rom to load");
    printf("\n");

    printf("optional arguments:\n");
    printf("  %-20s %s\n", "-h, --help", "show this help message and exit");
    printf("  %-20s %s\n", "-v, --verbose", "add extra logs");
}

int main(int argc, char* argv[])
{
    Params params;
    int ret;

    ret = parseArgs(argc, argv, &params);
    if (ret < 0 || optind == argc) {
        printHelp(argc, argv);
        return 0;
    }

    if (params.verbose) {
        logSetLevel(LOG_DEBUG);
    }

    // Create frontend
    auto frontend = std::make_shared<FrontendSdl2>();

    // Create and run SNES
    const char* romPath = argv[optind];

    auto snes = msfce::core::Snes::create();
    snes->addRenderer(frontend);

    ret = snes->plugCartidge(romPath);
    if (ret < 0) {
        snes->removeRenderer(frontend);
        return 1;
    }

    snes->start();

    // Run frontend
    frontend->init(snes);
    frontend->run();

    snes->stop();

    snes->removeRenderer(frontend);
    frontend.reset();

    return 0;
}
