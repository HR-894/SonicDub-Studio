#include "backends/tts/xtts_engine.h"
#include "network/http_client.h"
#include "core/error_handler.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace vd {

std::vector<uint8_t> XTTSBackend::synthesize(const std::string& text,
                                              const std::string& lang,
                                              const std::string& voice_id) { // voice_id is reference_audio_path
    HttpClient http;
    nlohmann::json payload;
    payload["text"] = text;
    payload["language"] = lang;
    payload["reference_audio_path"] = voice_id;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"}
    };

    HttpResponse res;
    try {
        VD_LOG_INFO("XTTSBackend: Cloning voice via http://127.0.0.1:8000/clone_tts");
        res = http.post("http://127.0.0.1:8000/clone_tts", payload.dump(), headers);
    } catch (const std::exception& e) {
        throw TTSException(std::string("XTTS connection failed: ") + e.what());
    }

    if (res.status_code != 200) {
        throw TTSException("XTTS API returned HTTP " + std::to_string(res.status_code) + ": " + res.body);
    }

    auto response_json = nlohmann::json::parse(res.body);
    std::string output_path = response_json["output_path"].get<std::string>();

    std::ifstream f(output_path, std::ios::binary);
    if (!f.is_open()) {
        throw TTSException("XTTS output file missing: " + output_path);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data;
}

} // namespace vd
