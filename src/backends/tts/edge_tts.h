// edge_tts.h
#pragma once
#include "stages/tts_engine.h"

namespace vd {

class EdgeTTS : public ITTSEngine {
public:
    EdgeTTS();

    std::vector<uint8_t> synthesize(const std::string& text,
                                     const std::string& lang,
                                     const std::string& voice_id) override;

    std::string name() const override { return "Microsoft Edge TTS"; }

private:
    std::string voice_;
};

} // namespace vd
