/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  transcriber.h — Stage 2: Speech-to-Text via whisper.cpp                  ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 2 of 7
 *
 *   [16kHz WAV] ──► Transcriber ──► [SegmentList with timestamped text]
 *                                           │
 *                                           └──► Translator (Stage 3)
 *
 * WHAT IT DOES:
 *   Uses whisper.cpp (OpenAI Whisper model via GGML) to convert audio
 *   into timestamped text segments.  Supports automatic language detection.
 *
 * KEY FEATURES:
 *   - Local inference (no API calls, works offline)
 *   - GPU acceleration via CUDA (when ENABLE_CUDA=ON)
 *   - Auto language detection (sets segment.source_language)
 *   - Filters out segments shorter than 100ms (noise)
 *
 * PIMPL PATTERN:
 *   Uses pimpl (Pointer to Implementation) to hide whisper.h dependency
 *   from all other translation units. Only transcriber.cpp includes whisper.h.
 *
 * DEPENDENCIES:
 *   whisper.cpp (v1.6.0+), segment.h
 */

#pragma once

#include "core/segment.h"
#include <string>
#include <memory>

namespace vd {

class Transcriber {
public:
    Transcriber();
    ~Transcriber();

    // Non-copyable (owns whisper model resources)
    Transcriber(const Transcriber&) = delete;
    Transcriber& operator=(const Transcriber&) = delete;

    /**
     * Transcribe a WAV file into timestamped segments.
     *
     * @param wav_path   Path to 16kHz mono PCM s16le WAV file
     * @param language   ISO 639-1 code or "auto" for detection
     * @return           Vector of segments with timing + original_text
     *
     * @throws WhisperException  if model fails to load or inference errors
     */
    SegmentList transcribe(const std::string& wav_path,
                           const std::string& language = "auto");

private:
    struct Impl;                    ///< Forward-declared implementation
    std::unique_ptr<Impl> impl_;   ///< Pimpl — hides whisper.h
};

} // namespace vd
