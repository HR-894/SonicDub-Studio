// audio_mixer.cpp
#include "stages/audio_mixer.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace vd {

static const int SAMPLE_RATE = 16000;
static const int CHANNELS    = 1;

static std::vector<int16_t> load_wav_pcm(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    auto size = f.tellg();
    f.seekg(44); // skip header
    size_t samples = (static_cast<size_t>(size) - 44) / sizeof(int16_t);
    std::vector<int16_t> pcm(samples);
    f.read(reinterpret_cast<char*>(pcm.data()), samples * sizeof(int16_t));
    return pcm;
}

static void write_wav(const std::string& path, const std::vector<int16_t>& pcm) {
    std::ofstream f(path, std::ios::binary);
    uint32_t data_size  = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    uint32_t file_size  = 36 + data_size;
    uint32_t byte_rate  = SAMPLE_RATE * CHANNELS * sizeof(int16_t);
    uint16_t block_align = CHANNELS * sizeof(int16_t);
    uint16_t bits       = 16;
    uint16_t audio_fmt  = 1;

    auto w32 = [&](uint32_t v){ f.write(reinterpret_cast<const char*>(&v), 4); };
    auto w16 = [&](uint16_t v){ f.write(reinterpret_cast<const char*>(&v), 2); };

    f.write("RIFF",4); w32(file_size);
    f.write("WAVE",4); f.write("fmt ",4); w32(16);
    w16(audio_fmt); w16(CHANNELS); w32(SAMPLE_RATE); w32(byte_rate);
    w16(block_align); w16(bits);
    f.write("data",4); w32(data_size);
    f.write(reinterpret_cast<const char*>(pcm.data()), data_size);
}

void AudioMixer::mix(const SegmentList& segments, const std::string& output_wav) {
    if (segments.empty()) {
        VD_LOG_WARN("AudioMixer: no segments to mix");
        return;
    }

    int64_t total_ms = segments.back().end_ms;
    size_t total_samples = static_cast<size_t>(total_ms * SAMPLE_RATE / 1000);
    std::vector<int16_t> output(total_samples, 0); // silence

    for (const auto& seg : segments) {
        if (seg.synced_audio_path.empty()) continue;
        if (seg.state == Segment::State::Failed) continue;

        auto pcm = load_wav_pcm(seg.synced_audio_path);
        if (pcm.empty()) continue;

        size_t start_sample = static_cast<size_t>(seg.start_ms * SAMPLE_RATE / 1000);
        size_t copy_count   = std::min(pcm.size(), total_samples - start_sample);

        for (size_t i = 0; i < copy_count; ++i) {
            // Mix with clipping prevention
            int32_t mixed = static_cast<int32_t>(output[start_sample + i]) + pcm[i];
            output[start_sample + i] = static_cast<int16_t>(
                std::clamp(mixed, static_cast<int32_t>(INT16_MIN),
                           static_cast<int32_t>(INT16_MAX)));
        }
    }

    write_wav(output_wav, output);
    VD_LOG_INFO("AudioMixer: mixed {} samples -> {}", total_samples, output_wav);
}

} // namespace vd
