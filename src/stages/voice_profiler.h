#pragma once

#include "core/segment.h"
#include <string>
#include <map>

namespace vd {

class VoiceProfiler {
public:
    VoiceProfiler();
    ~VoiceProfiler() = default;

    /**
     * Iterates through the segment list and groups by speaker_id.
     * Selects the cleanest/longest original sequence for each speaker 
     * and uses FFmpeg to extract it as a 16kHz mono reference WAV file.
     * Returns a map mapping speaker_id to reference_audio path.
     */
    std::map<std::string, std::string> profile_speakers(const std::string& input_video_path,
                                                        const std::string& temp_dir,
                                                        const SegmentList& segments);
};

} // namespace vd
