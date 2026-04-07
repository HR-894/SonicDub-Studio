#include "stages/voice_profiler.h"
#include "core/logger.h"
#include <QProcess>
#include <QStringList>

namespace vd {

VoiceProfiler::VoiceProfiler() {}

std::map<std::string, std::string> VoiceProfiler::profile_speakers(
        const std::string& input_video_path,
        const std::string& temp_dir,
        const SegmentList& segments) {
    
    std::map<std::string, const Segment*> best_segments;
    
    // Find the longest segment for each speaker to use as the voice clone reference
    for (const auto& seg : segments) {
        if (seg.speaker_id.empty() || seg.speaker_id == "UNKNOWN") continue;
        
        auto it = best_segments.find(seg.speaker_id);
        if (it == best_segments.end() || seg.duration_ms() > it->second->duration_ms()) {
            best_segments[seg.speaker_id] = &seg;
        }
    }
    
    std::map<std::string, std::string> speaker_refs;
    for (const auto& [speaker_id, seg] : best_segments) {
        std::string ref_path = temp_dir + "/" + speaker_id + "_ref.wav";
        
        QStringList args;
        args << "-y"
             << "-ss" << QString::number(seg->start_ms / 1000.0, 'f', 3)
             << "-t" << QString::number(seg->duration_ms() / 1000.0, 'f', 3)
             << "-i" << QString::fromStdString(input_video_path)
             << "-ar" << "16000"
             << "-ac" << "1"
             << "-c:a" << "pcm_s16le"
             << "-loglevel" << "quiet"
             << QString::fromStdString(ref_path);

        int rc = QProcess::execute("ffmpeg", args);
        if (rc == 0) {
            speaker_refs[speaker_id] = ref_path;
            VD_LOG_INFO("VoiceProfiler: Created reference audio for {}", speaker_id);
        } else {
            VD_LOG_ERROR("VoiceProfiler: Failed to extract reference for {}", speaker_id);
        }
    }
    
    return speaker_refs;
}

} // namespace vd
