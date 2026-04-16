/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  vibevoice_transcriber.h — VibeVoice-ASR Stage (Who + When + What)        ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 2 (replaces Transcriber + Diarizer combination)
 *
 *   [16kHz WAV] ──► VibeVoiceTranscriber ──► [SegmentList]
 *                                                  │
 *                                          Each segment has:
 *                                            - start_ms / end_ms (timestamps)
 *                                            - original_text     (transcription)
 *                                            - speaker_id        (diarization)
 *                                            - source_language   (auto-detected)
 *
 * WHY THIS IS BETTER THAN WHISPER + PYANNOTE:
 *   - Whisper runs → diarization runs → two outputs must be merged by time overlap
 *     (mismatch frequent, especially at sentence boundaries)
 *   - VibeVoice-ASR does all three in one neural network pass → perfect alignment
 *   - 60-min single-pass (Whisper struggles beyond 30s chunks without VAD)
 *   - 50+ languages natively, no config needed
 *
 * FALLBACK BEHAVIOR:
 *   If the AI Bridge returns 503 (VibeVoice-ASR not installed), the pipeline
 *   automatically falls back to the existing Whisper + Diarizer path.
 *   No user action required.
 *
 * HOTWORDS:
 *   Custom terms (character names, technical words) can be injected via
 *   config "vibevoice_hotwords" list → passed as initial_prompt to the model.
 */

#pragma once

#include "core/segment.h"
#include "network/http_client.h"
#include <string>
#include <vector>
#include <cstdint>

namespace vd {

class VibeVoiceTranscriber {
public:
    /**
     * @param bridge_port  Dynamically assigned AI Bridge port.
     */
    explicit VibeVoiceTranscriber(uint16_t bridge_port);
    ~VibeVoiceTranscriber() = default;

    /**
     * Transcribe audio using VibeVoice-ASR.
     * Returns SegmentList with speaker_id, timestamps, and text all populated.
     *
     * @param wav_path    Path to 16kHz mono WAV audio file
     * @param language    "auto" for detection, or ISO 639-1 code
     * @param hotwords    Custom terms to improve accuracy (e.g. character names)
     * @return            SegmentList (empty if bridge unavailable → use fallback)
     *
     * @throws nothing — errors are logged, empty list triggers Whisper fallback
     */
    SegmentList transcribe(const std::string& wav_path,
                           const std::string& language = "auto",
                           const std::vector<std::string>& hotwords = {});

    /// Returns true if last call succeeded (false → use Whisper fallback).
    bool last_call_succeeded() const noexcept { return last_success_; }

private:
    HttpClient  http_;
    uint16_t    bridge_port_;
    bool        last_success_{false};
};

} // namespace vd
