#pragma once

#include <memory>

#include <msfce/core/renderer.h>
#include <msfce/core/controller.h>

namespace msfce::core {

struct SnesConfig {
    int displayWidth;
    int displayHeight;
    int displayRate;

    int audioChannels;
    int audioSampleSize;
    int audioSampleRate;
};

class Snes {
public:
    static std::shared_ptr<Snes> create();

public:
    virtual ~Snes() = default;

    virtual int addRenderer(const std::shared_ptr<Renderer>& renderer) = 0;
    virtual int removeRenderer(const std::shared_ptr<Renderer>& renderer) = 0;

    virtual int plugCartidge(const char* path) = 0;
    virtual std::string getRomBasename() const = 0;

    virtual int start() = 0;
    virtual int stop() = 0;

    virtual SnesConfig getConfig() = 0;

    virtual int renderSingleFrame(bool renderPpu = true) = 0;

    virtual void setController1(const Controller& controller) = 0;

    virtual void saveState(const std::string& path) = 0;
    virtual void loadState(const std::string& path) = 0;
};

} // namespace msfce::core
