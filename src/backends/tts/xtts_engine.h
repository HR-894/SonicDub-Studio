#pragma once
#include "stages/tts_engine.h"

namespace vd {

class XTTSBackend : public ITTSEngine {
public:
    XTTSBackend() = default;
    ~XTTSBackend() override = default;

    std::vector<uint8_t> synthesize(const std::string& text,
                                     const std::string& lang,
                                     const std::string& voice_id) override;

    std::string name() const override { return "xttsv2_local"; }
};

} // namespace vd
