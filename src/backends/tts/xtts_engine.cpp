#include "backends/tts/xtts_engine.h"
#include "network/http_client.h"
#include "core/error_handler.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace vd {

XTTSBackend::XTTSBackend(uint16_t bridge_port) : bridge_port_(bridge_port) {}

std::vector<uint8_t> XTTSBackend::synthesize(const std::string& text,
                                              const std::string& lang,
                                              const std::string& voice_id) { // voice_id is reference_audio_path
    HttpClient http;
    // XTTS synthesis can be slow on CPU — don't let the 30s default kill us
    http.set_timeout(600);  // 10 minutes

    nlohmann::json payload;
    payload["text"] = text;
    payload["language"] = lang;
    payload["reference_audio_path"] = voice_id;
    payload["output_dir"] = output_dir_;  // Tell Python where to save

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"}
    };

    std::string url = "http://127.0.0.1:" + std::to_string(bridge_port_) + "/clone_tts";

    HttpResponse res;
    try {
        VD_LOG_INFO("XTTSBackend: Cloning voice via {}", url);
        res = http.post(url, payload.dump(), headers);
    } catch (const std::exception& e) {
        throw TTSException(std::string("XTTS connection failed: ") + e.what());
    }

    if (res.status_code != 200) {
        throw TTSException("XTTS API returned HTTP " + std::to_string(res.status_code) + ": " + res.body);
    }

    // Wrap JSON parse in try-catch — a malformed body from Python would crash here
    nlohmann::json response_json;
    try {
        response_json = nlohmann::json::parse(res.body);
    } catch (const nlohmann::json::exception& e) {
        throw TTSException(std::string("XTTS: Failed to parse response JSON: ") + e.what());
    }

    std::string output_path = response_json.value("output_path", std::string(""));
    if (output_path.empty()) {
        throw TTSException("XTTS: Response missing 'output_path' field");
    }

    std::ifstream f(output_path, std::ios::binary);
    if (!f.is_open()) {
        throw TTSException("XTTS output file missing: " + output_path);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data;
}

} // namespace vd
