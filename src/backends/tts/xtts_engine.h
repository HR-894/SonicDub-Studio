#pragma once
#include "stages/tts_engine.h"
#include <cstdint>

namespace vd {

class XTTSBackend : public ITTSEngine {
public:
    /**
     * @param bridge_port The dynamically assigned AI bridge server port.
     */
    explicit XTTSBackend(uint16_t bridge_port);
    ~XTTSBackend() override = default;

    std::vector<uint8_t> synthesize(const std::string& text,
                                     const std::string& lang,
                                     const std::string& voice_id) override;

    std::string name() const override { return "xttsv2_local"; }

    /// Set the job temp directory so generated files land inside it for automatic cleanup.
    void set_output_dir(const std::string& dir) { output_dir_ = dir; }

    /// Set the current segment ID for unique filename generation.
    void set_segment_id(uint32_t id) { segment_id_ = id; }

private:
    uint16_t bridge_port_;
    std::string output_dir_;
    uint32_t segment_id_{0};
};

} // namespace vd
