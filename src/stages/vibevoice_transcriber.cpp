/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  vibevoice_transcriber.cpp — VibeVoice-ASR Client Implementation          ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 */

#include "stages/vibevoice_transcriber.h"
#include "core/config_manager.h"
#include "core/logger.h"
#include "core/error_handler.h"

#include <nlohmann/json.hpp>
#include <format>

namespace vd {

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

VibeVoiceTranscriber::VibeVoiceTranscriber(uint16_t bridge_port)
    : bridge_port_(bridge_port)
{
    // VibeVoice-ASR processes up to 60 min of audio in ~2-10 min on GPU.
    // Set generous timeout so it doesn't abort on long videos.
    http_.set_timeout(3600);  // 1 hour max
}

// ─────────────────────────────────────────────────────────────────────────────
// transcribe — POST /transcribe_vibevoice → parse SegmentList
// ─────────────────────────────────────────────────────────────────────────────

SegmentList VibeVoiceTranscriber::transcribe(
    const std::string& wav_path,
    const std::string& language,
    const std::vector<std::string>& hotwords)
{
    last_success_ = false;

    if (bridge_port_ == 0) {
        VD_LOG_WARN("VibeVoiceTranscriber: Bridge not running, using Whisper fallback.");
        return {};
    }

    // Build JSON payload
    nlohmann::json payload;
    payload["audio_path"] = wav_path;
    payload["language"]   = language;
    payload["hotwords"]   = hotwords;
    payload["use_gpu"]    = ConfigManager::instance().get_vibevoice_use_gpu();
    payload["model_path"] = ConfigManager::instance().get_vibevoice_asr_model();

    std::string url = std::format("http://127.0.0.1:{}/transcribe_vibevoice", bridge_port_);

    VD_LOG_INFO("VibeVoiceTranscriber: Sending '{}' to bridge (hotwords={})",
                wav_path, hotwords.size());

    // Make HTTP request
    HttpResponse resp;
    try {
        resp = http_.post(url, payload.dump(),
                          {{"Content-Type", "application/json"}});
    } catch (const std::exception& e) {
        VD_LOG_WARN("VibeVoiceTranscriber: Bridge connection error: {} → Whisper fallback",
                    e.what());
        return {};
    }

    // If bridge returns 503 → VibeVoice not installed → fallback
    if (resp.status_code == 503) {
        VD_LOG_WARN("VibeVoiceTranscriber: VibeVoice-ASR not available on bridge → "
                    "Whisper fallback");
        return {};
    }

    if (resp.status_code != 200) {
        VD_LOG_WARN("VibeVoiceTranscriber: Bridge HTTP {} → Whisper fallback",
                    resp.status_code);
        return {};
    }

    // Parse JSON response
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(resp.body);
    } catch (const nlohmann::json::exception& ex) {
        VD_LOG_ERROR("VibeVoiceTranscriber: Invalid JSON from bridge: {}", ex.what());
        return {};
    }

    auto raw_segs = j.value("segments", nlohmann::json::array());
    if (raw_segs.empty()) {
        VD_LOG_WARN("VibeVoiceTranscriber: Bridge returned 0 segments → Whisper fallback");
        return {};
    }

    // Build SegmentList from VibeVoice-ASR output
    SegmentList result;
    result.reserve(raw_segs.size());
    uint32_t idx = 0;

    for (const auto& s : raw_segs) {
        Segment seg;
        seg.id              = idx++;
        seg.start_ms        = s.value("start_ms",   int64_t{0});
        seg.end_ms          = s.value("end_ms",      int64_t{1000});
        seg.original_text   = s.value("text",        std::string{});
        seg.speaker_id      = s.value("speaker",     std::string{"SPEAKER_00"});
        seg.source_language = s.value("language",    std::string{"en"});
        seg.transcription_confidence = s.value("confidence", 1.0f);
        seg.state           = Segment::State::Transcribed;

        // Trim whitespace
        auto& t = seg.original_text;
        t.erase(0, t.find_first_not_of(" \t\n\r"));
        if (!t.empty()) t.erase(t.find_last_not_of(" \t\n\r") + 1);

        // Filter out ultra-short or empty segments
        if (!t.empty() && seg.duration_ms() >= 100) {
            result.push_back(std::move(seg));
        }
    }

    last_success_ = true;
    std::string detected_lang = j.value("detected_language", "?");
    VD_LOG_INFO("VibeVoiceTranscriber: ✅ {} valid segments, lang={}, model={}",
                result.size(), detected_lang,
                j.value("model_used", "?"));

    return result;
}

} // namespace vd
