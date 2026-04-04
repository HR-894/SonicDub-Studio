/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  segment.h — Central Data Structure for the Dubbing Pipeline              ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Defines the `Segment` struct — the fundamental data unit that flows
 *   through every stage of the dubbing pipeline. Each Segment represents
 *   one spoken utterance (a sentence or phrase) from the source video.
 *
 * DATA FLOW THROUGH PIPELINE:
 *   ┌────────────┐     ┌──────────────┐     ┌────────────┐     ┌─────────┐
 *   │ Extraction │────►│ Transcription│────►│ Translation│────►│   TTS   │
 *   │  (Stage 1) │     │   (Stage 2)  │     │  (Stage 3) │     │(Stage 4)│
 *   └────────────┘     └──────────────┘     └────────────┘     └─────────┘
 *        │                    │                    │                  │
 *        ▼                    ▼                    ▼                  ▼
 *   raw_audio_path      original_text       translated_text    tts_audio_path
 *   start_ms/end_ms     source_language                        tts_audio_data
 *
 *   ┌──────────┐     ┌──────────┐
 *   │   Sync   │────►│   Mix    │
 *   │(Stage 5) │     │(Stage 6) │
 *   └──────────┘     └──────────┘
 *        │                │
 *        ▼                ▼
 *   synced_audio_path   Final dubbed video
 *
 * STATE MACHINE:
 *   Raw → Extracted → Transcribed → Translated → TTSDone → Synced → Done
 *                                                                  │
 *                                                               Failed
 *                                                          (from any state)
 *
 * DSA NOTES:
 *   - SegmentList is a std::vector<Segment> — O(1) random access by ID
 *   - Segments are processed in parallel during Translation & TTS stages
 *   - Each segment is self-contained (carries its own state + data paths)
 *
 * DEPENDENCIES:
 *   <string>, <vector>, <cstdint>, <optional>  (standard library only)
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// Segment — One spoken utterance from the source video
// ═══════════════════════════════════════════════════════════════════════════════

struct Segment {

    // ─── Identity ──────────────────────────────────────────────────────────
    uint32_t id{0};                  ///< Zero-based segment index within job

    // ─── Timing (milliseconds from video start) ───────────────────────────
    int64_t start_ms{0};             ///< When this utterance begins
    int64_t end_ms{0};               ///< When this utterance ends

    /// Calculate duration.  Complexity: O(1)
    [[nodiscard]]
    int64_t duration_ms() const noexcept { return end_ms - start_ms; }

    // ─── Text Content ──────────────────────────────────────────────────────
    std::string original_text;       ///< Transcribed text (from whisper.cpp)
    std::string translated_text;     ///< Translated text  (from translator backend)
    std::string source_language;     ///< ISO 639-1 code detected or provided (e.g. "en")

    // ─── File Paths (temp files generated per-stage) ──────────────────────
    std::string raw_audio_path;      ///< Stage 1 output: extracted audio chunk
    std::string tts_audio_path;      ///< Stage 4 output: synthesized speech
    std::string synced_audio_path;   ///< Stage 5 output: time-stretched to match duration

    // ─── In-Memory Audio Buffer ───────────────────────────────────────────
    /// Some TTS backends return audio as byte array instead of file.
    /// DSA: std::vector<uint8_t> — contiguous memory, O(1) access.
    std::vector<uint8_t> tts_audio_data;

    // ─── Processing State Machine ─────────────────────────────────────────
    /**
     * Tracks which pipeline stage has been completed for this segment.
     * Transitions are strictly forward (except to Failed from any state).
     *
     * State Transition Table:
     *   Current State  │  Event              │  Next State
     *   ───────────────┼─────────────────────┼─────────────
     *   Raw            │  Audio extracted     │  Extracted
     *   Extracted      │  Whisper completed   │  Transcribed
     *   Transcribed    │  Translation done    │  Translated
     *   Translated     │  TTS audio received  │  TTSDone
     *   TTSDone        │  Time-stretch done   │  Synced
     *   Synced         │  Mix complete        │  Done
     *   (any)          │  Error occurred      │  Failed
     */
    enum class State : uint8_t {
        Raw = 0,       ///< Initial state — segment created but unprocessed
        Extracted,     ///< Audio chunk has been extracted from video
        Transcribed,   ///< Speech-to-text complete (original_text populated)
        Translated,    ///< Translation complete (translated_text populated)
        TTSDone,       ///< Text-to-speech synthesis complete
        Synced,        ///< Audio time-stretched to match original duration
        Done,          ///< Fully processed and ready for final mix
        Failed         ///< An error occurred — see error_message
    };

    State       state{State::Raw};   ///< Current processing state
    std::string error_message;       ///< Error description if state == Failed

    // ─── Future Extensions ────────────────────────────────────────────────
    std::optional<std::string> speaker_id;  ///< For future multi-speaker diarization

    // ─── Quality Metrics ──────────────────────────────────────────────────
    float transcription_confidence{1.0f};   ///< 0.0–1.0, from whisper.cpp
    float translation_confidence{1.0f};     ///< 0.0–1.0, from translator backend
};

// ═══════════════════════════════════════════════════════════════════════════════
// Type Alias — Collection of Segments
// ═══════════════════════════════════════════════════════════════════════════════
// DSA: std::vector provides O(1) index access, O(n) iteration.
//      Segments are indexed by their `id` field which matches vector index.
using SegmentList = std::vector<Segment>;

} // namespace vd
