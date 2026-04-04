/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  audio_syncer.h — Stage 5: Time-Stretch TTS Audio                         ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 5 of 7
 *
 *   [TTS Audio (any length)] ──► AudioSyncer ──► [Audio matching original duration]
 *
 * PROBLEM:
 *   TTS output duration rarely matches the original segment timing.
 *   If original segment = 3.2s but TTS produced 4.1s of audio,
 *   we need to speed up the TTS audio by 4.1/3.2 = 1.28x to fit.
 *
 * ALGORITHM:
 *   1. Get input WAV duration (via FFmpeg)
 *   2. Calculate ratio = input_duration / target_duration
 *   3. Build FFmpeg atempo filter chain:
 *      - atempo supports [0.5, 100.0] per filter instance
 *      - For ratios outside this range, chain multiple filters
 *      - Example: ratio 0.3 → "atempo=0.5,atempo=0.6"
 *   4. Apply filter via ffmpeg CLI subprocess
 *
 * DSA:
 *   - Filter chain construction: string building, O(log ratio) filters
 *   - File I/O: temp WAV files for each segment
 */

#pragma once

#include <string>
#include <cstdint>

namespace vd {

class AudioSyncer {
public:
    /**
     * Time-stretch audio to match a target duration.
     *
     * @param input_wav          Path to TTS-generated WAV
     * @param output_wav         Path to write time-stretched WAV
     * @param target_duration_ms Original segment duration to match
     */
    void sync(const std::string& input_wav,
              const std::string& output_wav,
              int64_t target_duration_ms);

private:
    /**
     * Build an FFmpeg atempo filter string for the given speed ratio.
     * Chains multiple atempo filters if ratio is outside [0.5, 100].
     *
     * @param ratio  Speed multiplier (>1 = faster, <1 = slower)
     * @return       Filter string like "atempo=1.28" or "atempo=0.5,atempo=0.8"
     */
    static std::string build_atempo_filter(double ratio);
};

} // namespace vd
