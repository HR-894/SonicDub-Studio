/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  vibevoice_tts.h — VibeVoice-Realtime-0.5B TTS Backend                    ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * WHAT IT IS:
 *   Microsoft's open-source VibeVoice-Realtime-0.5B local TTS model.
 *   Runs entirely offline on the local machine (no API key needed).
 *   ~200ms first-audio latency. Speaker-consistent per speaker_id.
 *
 * HOW IT WORKS:
 *   - Calls the AI Bridge's /tts_vibevoice endpoint (Python-side model)
 *   - Bridge lazy-loads the 0.5B model once, reuses for all calls
 *   - Each speaker_id maps to a stable voice (speaker_name)
 *     → guarantees voice consistency across all segments of the same speaker
 *
 * SPEAKER VOICE MAPPING:
 *   speaker_id (e.g. "SPEAKER_00") → VibeVoice speaker name (e.g. "Carter")
 *   Built-in voices: Carter, Alice, Mark, Jenna (EN); and 9 multilingual voices
 *
 * THREAD SAFETY:
 *   synthesize() is stateless on the C++ side (HTTP call).
 *   Python bridge serializes GPU access internally.
 *
 * PREREQUISITES:
 *   AI Bridge must have VibeVoice installed:
 *     cd VibeVoice && pip install -e .[streamingtts]
 *   Model auto-downloads from HuggingFace on first use (~1GB).
 */

#pragma once

#include "stages/tts_engine.h"
#include "network/http_client.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace vd {

class VibeVoiceTTS : public ITTSEngine {
public:
    /**
     * @param bridge_port  Dynamically assigned AI Bridge port.
     * @param use_gpu      If true, Python side will use CUDA if available.
     */
    explicit VibeVoiceTTS(uint16_t bridge_port, bool use_gpu = true);
    ~VibeVoiceTTS() override = default;

    /**
     * Synthesize text using VibeVoice-Realtime-0.5B.
     *
     * @param text      Text to speak (avoid code/formulas — unsupported by model)
     * @param lang      ISO 639-1 code (model is primarily English; multilingual experimental)
     * @param speaker_id  speaker_id from diarization (maps to stable VibeVoice voice name)
     * @return          Raw WAV bytes
     *
     * @throws TTSException if bridge is unreachable or synthesis fails
     */
    std::vector<uint8_t> synthesize(const std::string& text,
                                     const std::string& lang,
                                     const std::string& speaker_id) override;

    std::string name() const override { return "vibevoice_local"; }

    /// Set temp directory for output WAV files.
    void set_output_dir(const std::string& dir) { output_dir_ = dir; }

    /// Set segment ID for unique filename generation.
    void set_segment_id(uint32_t id) { segment_id_ = id; }

    /// Auto-assign a voice to an unknown speaker_id (round-robin, stable per speaker_id).
    /// Call for each unique speaker_id before synthesize() to pre-warm the voice map.
    struct VoiceConfig; // Defined below
    VoiceConfig resolve_voice(const std::string& speaker_id);

    HttpClient   http_;
    uint16_t     bridge_port_;
    bool         use_gpu_;
    std::string  output_dir_;
    uint32_t     segment_id_{0};

    struct VoiceConfig {
        std::string voice_name;
        float pitch_shift;
    };

    /// speaker_id → VibeVoice built-in voice configuration
    std::unordered_map<std::string, VoiceConfig> speaker_voice_map_;

    /// Built-in VibeVoice-Realtime voice pool (EN)
    static constexpr const char* kVoicePool[] = {
        "Carter", "Alice", "Mark", "Jenna"
    };
    static constexpr size_t kVoicePoolSize = 4;

    /// Auto-assign a voice (and deterministic pitch shift) to an unknown speaker_id (round-robin)
    VoiceConfig resolve_voice(const std::string& speaker_id);
};

} // namespace vd
