/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  audio_mixer.h — Stage 6: Combine Synced Segments into One Track          ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 6 of 7
 *
 *   [N synced WAV files] ──► AudioMixer ──► [Single dubbed audio WAV]
 *
 * ALGORITHM:
 *   1. Allocate output buffer sized for the entire video duration (silence)
 *   2. For each segment:
 *      a. Load its synced WAV as int16_t PCM data
 *      b. Calculate start sample index from segment.start_ms
 *      c. Additively mix PCM samples into the output buffer
 *      d. Clamp values to INT16 range to prevent clipping
 *   3. Write final mixed buffer as WAV
 *
 * DSA:
 *   - Output buffer:  vector<int16_t> of total video length — O(T) space
 *   - Mixing:         O(N × S) where N = segments, S = avg segment samples
 *   - Clipping:       std::clamp per sample — O(1) per sample
 */

#pragma once

#include "core/segment.h"
#include <string>

namespace vd {

class AudioMixer {
public:
    /**
     * Mix all synced segment audio files into one continuous track.
     * Inserts silence between segments to preserve original timing.
     *
     * @param segments    SegmentList with populated synced_audio_path fields
     * @param output_wav  Path to write the final mixed WAV file
     */
    void mix(const SegmentList& segments, const std::string& output_wav);
};

} // namespace vd
