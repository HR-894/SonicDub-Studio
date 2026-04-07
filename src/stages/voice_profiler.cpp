#include "stages/voice_profiler.h"
#include "core/logger.h"
#include <QProcess>
#include <QStringList>
#include <algorithm>

namespace vd {

VoiceProfiler::VoiceProfiler() {}

std::map<std::string, std::string> VoiceProfiler::profile_speakers(
        const std::string& input_video_path,
        const std::string& temp_dir,
        const SegmentList& segments) {

    // ── Phase 1: Select best reference segment per speaker ──────────────
    // XTTSv2 works best with 3–10 second clean reference clips.
    // Feeding it a 40-second clip wastes VRAM and captures background noise.
    // Strategy: prefer segments in [3s, 10s] with highest transcription_confidence.
    // Fallback:  if no segment is in that range, pick the closest to 6s.

    struct CandidateInfo {
        const Segment* seg = nullptr;
        bool           in_range = false;
        float          confidence = 0.0f;
        int64_t        distance_from_ideal = INT64_MAX; // distance from 6000ms
    };

    std::map<std::string, CandidateInfo> best;

    for (const auto& seg : segments) {
        if (seg.speaker_id.empty() || seg.speaker_id == "UNKNOWN") continue;

        int64_t dur = seg.duration_ms();
        if (dur <= 0) continue;

        bool in_range = (dur >= 3000 && dur <= 10000);
        int64_t dist  = std::abs(dur - 6000);

        auto it = best.find(seg.speaker_id);
        if (it == best.end()) {
            // First candidate for this speaker
            best[seg.speaker_id] = {&seg, in_range, seg.transcription_confidence, dist};
            continue;
        }

        auto& current = it->second;

        // In-range candidates always beat out-of-range candidates
        if (in_range && !current.in_range) {
            current = {&seg, in_range, seg.transcription_confidence, dist};
        } else if (in_range == current.in_range) {
            // Among same category: prefer higher confidence, then closer to ideal
            if (seg.transcription_confidence > current.confidence ||
                (seg.transcription_confidence == current.confidence && dist < current.distance_from_ideal)) {
                current = {&seg, in_range, seg.transcription_confidence, dist};
            }
        }
        // else: current is in_range but candidate is not — skip
    }

    // ── Phase 2: Extract reference WAV clips via FFmpeg ─────────────────
    std::map<std::string, std::string> speaker_refs;
    for (const auto& [speaker_id, info] : best) {
        const Segment* seg = info.seg;

        // Clamp extraction to 10 seconds max even for fallback segments.
        double extract_dur_sec = std::min(seg->duration_ms() / 1000.0, 10.0);

        std::string ref_path = temp_dir + "/" + speaker_id + "_ref.wav";

        QStringList args;
        args << "-y"
             << "-ss" << QString::number(seg->start_ms / 1000.0, 'f', 3)
             << "-t" << QString::number(extract_dur_sec, 'f', 3)
             << "-i" << QString::fromStdString(input_video_path)
             << "-ar" << "16000"
             << "-ac" << "1"
             << "-c:a" << "pcm_s16le"
             << "-loglevel" << "quiet"
             << QString::fromStdString(ref_path);

        int rc = QProcess::execute("ffmpeg", args);
        if (rc == 0) {
            speaker_refs[speaker_id] = ref_path;
            VD_LOG_INFO("VoiceProfiler: Reference for {} — {:.1f}s, conf={:.2f}{}",
                        speaker_id, extract_dur_sec, info.confidence,
                        info.in_range ? "" : " (fallback, out of ideal range)");
        } else {
            VD_LOG_ERROR("VoiceProfiler: Failed to extract reference for {}", speaker_id);
        }
    }

    return speaker_refs;
}

} // namespace vd
