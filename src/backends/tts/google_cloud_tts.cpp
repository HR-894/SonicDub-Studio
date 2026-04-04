// google_cloud_tts.cpp
#include "backends/tts/google_cloud_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include <nlohmann/json.hpp>
#include <format>

// Base64 decode helper
static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '=') break;
        auto pos = chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

namespace vd {

GoogleCloudTTS::GoogleCloudTTS() {
    api_key_ = ConfigManager::instance().get_api_key("google_tts");
}

std::vector<uint8_t> GoogleCloudTTS::synthesize(const std::string& text,
                                                const std::string& lang,
                                                const std::string& voice_id) {
    if (text.empty()) return {};

    std::string url = std::format(
        "https://texttospeech.googleapis.com/v1/text:synthesize?key={}", api_key_);

    nlohmann::json body = {
        {"input", {{"text", text}}},
        {"voice", {{"languageCode", lang}, {"name", voice_id}}},
        // Request 16kHz linear PCM to match pipeline defaults
        {"audioConfig", {{"audioEncoding", "LINEAR16"}, {"sampleRateHertz", 16000}}}
    };

    auto resp = http_.post(url, body.dump(), {{"Content-Type","application/json"}});

    if (resp.status_code != 200)
        throw TTSException("Google Cloud TTS failed: " + resp.body);

    auto j = nlohmann::json::parse(resp.body);
    return base64_decode(j["audioContent"].get<std::string>());
}

} // namespace vd
