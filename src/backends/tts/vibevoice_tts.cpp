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
#include <filesystem>
#include <QProcess>
#include <QStringList>
#include <QString>

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

VibeVoiceTTS::VoiceConfig VibeVoiceTTS::resolve_voice(const std::string& speaker_id) {
    auto it = speaker_voice_map_.find(speaker_id);
    if (it != speaker_voice_map_.end()) {
        return it->second;
    }
    // Auto-assign: hash speaker_id → deterministic voice name and pitch shift
    // This provides 4 * 5 = 20 mathematically distinct voices from 4 models.
    size_t hash_val = std::hash<std::string>{}(speaker_id);
    size_t name_idx = hash_val % kVoicePoolSize;
    std::string voice_name = kVoicePool[name_idx];

    // Calculate deterministic pitch shift: -0.1, -0.05, 0.0, +0.05, +0.1
    int pitch_tier = (hash_val / kVoicePoolSize) % 5;
    float pitch_shift = 1.0f + (pitch_tier - 2) * 0.05f;

    VoiceConfig config = {voice_name, pitch_shift};
    speaker_voice_map_[speaker_id] = config;
    VD_LOG_INFO("VibeVoiceTTS: Assigned speaker '{}' → voice '{}' (pitch {}x)",
                speaker_id, config.voice_name, config.pitch_shift);
    return config;
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

    // Resolve voice config (speaker-consistent)
    VoiceConfig config = resolve_voice(speaker_id);

    // Build request
    std::string url = std::format("http://127.0.0.1:{}/tts_vibevoice", bridge_port_);

    nlohmann::json payload = {
        {"text",         text},
        {"speaker_name", config.voice_name},
        {"output_path",  out_path},
        {"use_gpu",      use_gpu_},
    };

    VD_LOG_INFO("VibeVoiceTTS: seg={} speaker='{}' voice='{}' pitch={:.2f} len={}chars",
                segment_id_, speaker_id, config.voice_name, config.pitch_shift, text.size());

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

    // ── Apply Math Pitch Filter if necessary ──────────────────────────────
    if (std::abs(config.pitch_shift - 1.0f) > 0.01f) {
        std::string tmp_path = output_dir_ + "/tmp_pitch_" + std::to_string(segment_id_) + ".wav";
        
        // VibeVoice outputs at 22050 Hz.
        // asetrate=22050*P,aresample=22050,atempo=1/P
        // This shifts pitch while precisely preserving original tempo!
        std::string filter = std::format("asetrate=22050*{0:.2f},aresample=22050,atempo=1/{0:.2f}", config.pitch_shift);
        
        QStringList args;
        args << "-y" << "-i" << QString::fromStdString(result_path)
             << "-af" << QString::fromStdString(filter)
             << "-ar" << "22050" << "-ac" << "1" << "-loglevel" << "quiet"
             << QString::fromStdString(tmp_path);
             
        int rc = QProcess::execute("ffmpeg", args);
        if (rc == 0) {
            std::filesystem::rename(tmp_path, result_path);
        } else {
            VD_LOG_WARN("VibeVoiceTTS: FFmpeg pitch shift failed (code {})", rc);
        }
    }

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
