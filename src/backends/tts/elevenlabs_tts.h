// elevenlabs_tts.h
#pragma once
#include "stages/tts_engine.h"
#include "network/http_client.h"

namespace vd {

class ElevenLabsTTS : public ITTSEngine {
public:
    ElevenLabsTTS();

    std::vector<uint8_t> synthesize(const std::string& text,
                                     const std::string& lang,
                                     const std::string& voice_id) override;

    std::string name() const override { return "ElevenLabs"; }

private:
    HttpClient http_;
    std::string api_key_;
};

} // namespace vd
