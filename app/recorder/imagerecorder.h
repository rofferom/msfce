#pragma once

#include <memory>
#include <string>

#include <msfce/core/snes.h>

#include "framerecorder.h"

namespace msfce::recorder {

class ImageRecorder : public FrameRecorderBackend {
public:
    ImageRecorder(
        const std::string& basename,
        const msfce::core::SnesConfig& snesConfig);

    int start() final;
    int stop() final;

    bool onFrameReceived(const std::shared_ptr<Frame>& inputFrame) final;

private:
    std::string m_Basename;
    const msfce::core::SnesConfig m_SnesConfig;
};

} // namespace msfce::recorder
