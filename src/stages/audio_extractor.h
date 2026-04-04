/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  audio_extractor.h — Stage 1: Video → Audio WAV Extraction                ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 1 of 7
 *
 *   [Input Video] ──► AudioExtractor ──► [16kHz Mono WAV]
 *                                              │
 *                                              └──► Transcriber (Stage 2)
 *
 * WHAT IT DOES:
 *   Takes any video file (MP4, MKV, AVI, MOV, WEBM) or URL,
 *   extracts the audio track, resamples it to:
 *     - 16,000 Hz sample rate   (required by whisper.cpp)
 *     - 1 channel (mono)        (required by whisper.cpp)
 *     - PCM s16le (signed 16-bit little-endian)
 *   and writes a standard WAV file.
 *
 * ALGORITHM:
 *   1. Open input via FFmpeg libavformat (handles any container)
 *   2. Find the best audio stream
 *   3. Open audio decoder (e.g. AAC, MP3, Opus → PCM)
 *   4. Create SwrContext to resample to 16kHz mono
 *   5. Decode packets → resample frames → accumulate PCM buffer
 *   6. Write WAV header + PCM data to output file
 *
 * DSA:
 *   - PCM buffer: std::vector<int16_t> — contiguous, O(1) amortized append
 *   - WAV file format: 44-byte RIFF header + raw PCM data
 *
 * DEPENDENCIES:
 *   FFmpeg (libavformat, libavcodec, libswresample, libavutil)
 */

#pragma once

#include <string>

namespace vd {

class AudioExtractor {
public:
    AudioExtractor() = default;

    /**
     * Extract audio from a video file or URL and save as WAV.
     *
     * @param input_path   Local file path (MP4, MKV, ...) or stream URL
     * @param output_wav   Destination path for the 16kHz mono WAV output
     *
     * @throws FFmpegException  if input cannot be opened or decoded
     */
    void extract(const std::string& input_path, const std::string& output_wav);

private:
    /// Internal implementation using FFmpeg C API
    void extract_via_ffmpeg(const std::string& input, const std::string& output);
};

} // namespace vd
