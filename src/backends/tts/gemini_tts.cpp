// gemini_tts.cpp
#include "backends/tts/gemini_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <format>
#include <vector>
#include <string>

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

GeminiTTS::GeminiTTS() {
    api_key_ = ConfigManager::instance().get_api_key("gemini");
}

std::vector<uint8_t> GeminiTTS::synthesize(const std::string& text,
                                           const std::string& lang,
                                           const std::string& voice_id) {
    if (text.empty()) return {};
    if (api_key_.empty()) throw TTSException("Gemini API key not configured");

    // Use Gemini TTS via AI Studio API
    std::string url = std::format(
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-2.5-flash-preview-tts:generateContent?key={}", api_key_);

    nlohmann::json body = {
        {"contents", nlohmann::json::array({
            {{"parts", nlohmann::json::array({
                {{"text", text}}
            })}}
        })},
        {"generationConfig", {
            {"responseModalities", nlohmann::json::array({"AUDIO"})},
            {"speechConfig", {
                {"voiceConfig", {
                    {"prebuiltVoiceConfig", {
                        {"voiceName", voice_id}
                    }}
                }}
            }}
        }}
    };

    auto resp = http_.post(url, body.dump(),
                           {{"Content-Type", "application/json"}});

    if (resp.status_code != 200)
        throw TTSException("Gemini TTS failed: " + resp.body);

    auto j = nlohmann::json::parse(resp.body);
    std::string audio_b64 = j["candidates"][0]["content"]["parts"][0]["inlineData"]["data"];
    return base64_decode(audio_b64);
}

} // namespace vd
