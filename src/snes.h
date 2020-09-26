#pragma once

#include <memory>
#include <vector>

class Frontend;

class Snes {
public:
    Snes(const std::shared_ptr<Frontend>& frontend);

    int plugCartidge(const char* path);

    void run();

private:
    int loadRom(const char* romPath, std::vector<uint8_t>* outRom);

private:
    std::shared_ptr<Frontend> m_Frontend;

    std::vector<uint8_t> romData;
};
