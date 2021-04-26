#pragma once

#include <memory>
#include <vector>

class Frontend;
class Scheduler;

class Snes {
public:
    Snes(const std::shared_ptr<Frontend>& frontend);

    int plugCartidge(const char* path);

    void run();

    void toggleRunning();

    void speedUp();
    void speedNormal();

private:
    int loadRom(const char* romPath, std::vector<uint8_t>* outRom);

private:
    std::shared_ptr<Frontend> m_Frontend;
    std::shared_ptr<Scheduler> m_Scheduler;

    std::vector<uint8_t> romData;
};
