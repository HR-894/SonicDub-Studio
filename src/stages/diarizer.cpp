#include "stages/diarizer.h"
#include "network/http_client.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>

namespace vd {

Diarizer::Diarizer() {}

void Diarizer::diarize(const std::string& audio_path, SegmentList& segments) {
    if (segments.empty()) return;

    HttpClient http;
    nlohmann::json payload;
    payload["audio_path"] = audio_path;

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"}
    };

    HttpResponse res;
    try {
        VD_LOG_INFO("Diarizer: Calling AI Bridge at http://127.0.0.1:8000/diarize");
        res = http.post("http://127.0.0.1:8000/diarize", payload.dump(), headers);
    } catch (const std::exception& e) {
        VD_LOG_ERROR("Diarizer: Connection to AI Bridge failed: {}", e.what());
        return; // Proceed without diarization if bridge is down
    }

    if (res.status_code != 200) {
        VD_LOG_WARN("Diarizer: AI Bridge returned HTTP {}", res.status_code);
        return;
    }

    try {
        auto response_json = nlohmann::json::parse(res.body);
        auto diarized_segs = response_json["segments"];

        for (auto& seg : segments) {
            int64_t midpoint = seg.start_ms + (seg.duration_ms() / 2);
            seg.speaker_id = "UNKNOWN";

            for (const auto& ds : diarized_segs) {
                int64_t d_start = ds["start_ms"].get<int64_t>();
                int64_t d_end   = ds["end_ms"].get<int64_t>();
                
                if (midpoint >= d_start && midpoint <= d_end) {
                    seg.speaker_id = ds["speaker"].get<std::string>();
                    break;
                }
            }
        }
        VD_LOG_INFO("Diarizer: Successfully tagged {} segments.", segments.size());
    } catch (const auto& e) {
        VD_LOG_ERROR("Diarizer: Failed to parse bridge response: {}", e.what());
    }
}

} // namespace vd
