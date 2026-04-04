// elevenlabs_tts.cpp
#include "backends/tts/elevenlabs_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include <nlohmann/json.hpp>
#include <format>

namespace vd {

ElevenLabsTTS::ElevenLabsTTS() {
    api_key_ = ConfigManager::instance().get_api_key("elevenlabs");
}

std::vector<uint8_t> ElevenLabsTTS::synthesize(const std::string& text,
                                               const std::string& lang,
                                               const std::string& voice_id) {
    if (text.empty()) return {};

    // voice_id here should be ElevenLabs voice ID string
    std::string url = std::format(
        "https://api.elevenlabs.io/v1/text-to-speech/{}/stream", voice_id);

    nlohmann::json body = {
        {"text", text},
        {"model_id", "eleven_multilingual_v2"},
        {"voice_settings", {{"stability", 0.5}, {"similarity_boost", 0.75}}}
    };

    auto resp = http_.post(url, body.dump(), {
        {"xi-api-key",    api_key_},
        {"Content-Type",  "application/json"},
        // Request WAV PCM format directly
        {"Accept",        "audio/wav"}
    });

    if (resp.status_code != 200)
        throw TTSException("ElevenLabs failed: " + std::to_string(resp.status_code));

    return std::vector<uint8_t>(resp.body.begin(), resp.body.end());
}

} // namespace vd
