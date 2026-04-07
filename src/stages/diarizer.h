#pragma once

#include "core/segment.h"
#include <string>
#include <cstdint>

namespace vd {

class Diarizer {
public:
    /**
     * @param bridge_port The dynamically assigned AI bridge server port.
     */
    explicit Diarizer(uint16_t bridge_port);
    ~Diarizer() = default;

    /**
     * Sends the extracted audio to the Python AI bridge to identify distinct speakers.
     * Assigns speaker_id ("SPEAKER_00", etc.) to each segment in the SegmentList
     * based on time overlaps.
     */
    void diarize(const std::string& audio_path, SegmentList& segments);

private:
    uint16_t bridge_port_;
};

} // namespace vd
