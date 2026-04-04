/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  video_muxer.h — Stage 7: Merge Dubbed Audio into Video                   ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 7 of 7 (final stage)
 *
 *   [Original Video] + [Dubbed Audio WAV] ──► VideoMuxer ──► [Final Output MP4]
 *
 * MODES:
 *   "replace"  — Original audio is completely removed, dubbed track only
 *   "overlay"  — Original audio lowered (volume × original_volume) + dubbed overlaid
 *                Uses FFmpeg amix filter for smooth blending
 *
 * IMPLEMENTATION:
 *   Uses FFmpeg CLI via std::system() for reliability.
 *   Video stream is copied (no re-encoding) for speed — only audio is processed.
 */

#pragma once

#include <string>

namespace vd {

class VideoMuxer {
public:
    /**
     * Mux dubbed audio back into the original video.
     *
     * @param video_path        Original video file path
     * @param dubbed_audio_path Dubbed audio WAV from AudioMixer
     * @param output_path       Output video file path
     * @param mode              "replace" or "overlay"
     * @param original_volume   Original audio volume (0.0–1.0, used in overlay)
     *
     * @throws FFmpegException  if muxing fails
     */
    void mux(const std::string& video_path,
             const std::string& dubbed_audio_path,
             const std::string& output_path,
             const std::string& mode = "overlay",
             double original_volume = 0.1);
};

} // namespace vd
