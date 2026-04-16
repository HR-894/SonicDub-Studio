/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  vibevoice_tts.cpp — VibeVoice-Realtime-0.5B TTS Backend Implementation   ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 */

#include "backends/tts/vibevoice_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include "core/logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <format>

namespace vd {

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

VibeVoiceTTS::VibeVoiceTTS(uint16_t bridge_port, bool use_gpu)
    : bridge_port_(bridge_port)
    , use_gpu_(use_gpu)
{
    // Pre-warm: XTTS timeout is 600s; VibeVoice-Realtime is much faster
    http_.set_timeout(120);  // 2 minutes — model load on first call
}

// ─────────────────────────────────────────────────────────────────────────────
// resolve_voice — Map speaker_id to a stable VibeVoice voice name
// ─────────────────────────────────────────────────────────────────────────────

std::string VibeVoiceTTS::resolve_voice(const std::string& speaker_id) {
    auto it = speaker_voice_map_.find(speaker_id);
    if (it != speaker_voice_map_.end()) {
        return it->second;
    }
    // Auto-assign: hash speaker_id → round-robin index into voice pool
    // This is stable: same speaker_id always gets same voice even across calls.
    size_t idx = std::hash<std::string>{}(speaker_id) % kVoicePoolSize;
    std::string voice = kVoicePool[idx];
    speaker_voice_map_[speaker_id] = voice;
    VD_LOG_INFO("VibeVoiceTTS: Auto-assigned speaker '{}' → voice '{}'",
                speaker_id, voice);
    return voice;
}

// ─────────────────────────────────────────────────────────────────────────────
// synthesize — POST /tts_vibevoice → read output WAV
// ─────────────────────────────────────────────────────────────────────────────

std::vector<uint8_t> VibeVoiceTTS::synthesize(const std::string& text,
                                               const std::string& lang,
                                               const std::string& speaker_id) {
    if (text.empty()) return {};

    // Determine output path
    std::string out_path = output_dir_
        + "/vibevoice_" + std::to_string(segment_id_) + ".wav";

    // Resolve voice name (speaker-consistent)
    std::string voice = resolve_voice(speaker_id);

    // Build request
    std::string url = std::format("http://127.0.0.1:{}/tts_vibevoice", bridge_port_);

    nlohmann::json payload = {
        {"text",         text},
        {"speaker_name", voice},
        {"output_path",  out_path},
        {"use_gpu",      use_gpu_},
    };

    VD_LOG_INFO("VibeVoiceTTS: seg={} speaker='{}' voice='{}' len={}chars",
                segment_id_, speaker_id, voice, text.size());

    // HTTP POST
    HttpResponse resp;
    try {
        resp = http_.post(url, payload.dump(), {{"Content-Type", "application/json"}});
    } catch (const std::exception& e) {
        throw TTSException(std::string("VibeVoice bridge connection failed: ") + e.what());
    }

    // Handle bridge errors
    if (resp.status_code == 503) {
        // VibeVoice-Realtime not installed → TTSException triggers retry
        // with fallback backend in pipeline
        throw TTSException("VibeVoice-Realtime not available on bridge: " + resp.body);
    }
    if (resp.status_code != 200) {
        throw TTSException("VibeVoice bridge error HTTP "
                          + std::to_string(resp.status_code) + ": " + resp.body);
    }

    // Parse response
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(resp.body);
    } catch (const nlohmann::json::exception& e) {
        throw TTSException(std::string("VibeVoice: bad response JSON: ") + e.what());
    }

    std::string result_path = j.value("output_path", out_path);

    // Read WAV file
    std::ifstream f(result_path, std::ios::binary);
    if (!f.is_open()) {
        throw TTSException("VibeVoice output file missing: " + result_path);
    }

    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    VD_LOG_INFO("VibeVoiceTTS: seg={} → {} bytes", segment_id_, data.size());
    return data;
}

} // namespace vd
