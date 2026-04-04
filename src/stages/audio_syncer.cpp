/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  audio_syncer.cpp — Stage 5 Implementation                                ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * SECURITY:
 *   Uses QProcess::execute() instead of std::system() to prevent command
 *   injection via crafted filenames.
 */

#include "stages/audio_syncer.h"
#include "core/logger.h"
#include "core/error_handler.h"
#include <cmath>
#include <sstream>
#include <format>
#include <filesystem>

#include <QProcess>
#include <QStringList>
#include <QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
}

namespace vd {

std::string AudioSyncer::build_atempo_filter(double ratio) {
    // atempo supports [0.5, 100.0] per filter
    // Chain multiple for extreme ratios
    std::ostringstream oss;
    bool first = true;

    while (ratio > 100.0) {
        if (!first) oss << ",";
        oss << "atempo=100.0";
        ratio /= 100.0;
        first = false;
    }
    while (ratio < 0.5) {
        if (!first) oss << ",";
        oss << "atempo=0.5";
        ratio /= 0.5;
        first = false;
    }
    if (!first) oss << ",";
    oss << std::format("atempo={:.6f}", ratio);
    return oss.str();
}

void AudioSyncer::sync(const std::string& input_wav,
                        const std::string& output_wav,
                        int64_t target_duration_ms) {
    // Get input duration
    AVFormatContext* in_ctx_raw = nullptr;
    int ret = avformat_open_input(&in_ctx_raw, input_wav.c_str(), nullptr, nullptr);
    VD_CHECK_FFMPEG(ret, "AudioSyncer: open input");

    std::unique_ptr<AVFormatContext, decltype([](AVFormatContext* c){ avformat_close_input(&c); })>
        in_ctx(in_ctx_raw);
    avformat_find_stream_info(in_ctx.get(), nullptr);

    int64_t input_duration_ms = in_ctx->duration / (AV_TIME_BASE / 1000);
    if (input_duration_ms <= 0 || target_duration_ms <= 0) {
        // Just copy if we can't determine durations
        std::filesystem::copy_file(input_wav, output_wav,
            std::filesystem::copy_options::overwrite_existing);
        return;
    }

    double ratio = static_cast<double>(input_duration_ms) / target_duration_ms;
    // Clamp ratio to sane range
    ratio = std::clamp(ratio, 0.1, 20.0);

    std::string filter_str = build_atempo_filter(ratio);
    VD_LOG_DEBUG("AudioSyncer: ratio={:.3f}, filter={}", ratio, filter_str);

    // Use QProcess to avoid shell injection via crafted filenames
    QStringList args;
    args << "-y"
         << "-i" << QString::fromStdString(input_wav)
         << "-af" << QString::fromStdString(filter_str)
         << "-ar" << "16000"
         << "-ac" << "1"
         << "-loglevel" << "quiet"
         << QString::fromStdString(output_wav);

    int rc = QProcess::execute("ffmpeg", args);
    if (rc != 0) {
        VD_LOG_WARN("AudioSyncer: ffmpeg returned {}, copying original", rc);
        std::filesystem::copy_file(input_wav, output_wav,
            std::filesystem::copy_options::overwrite_existing);
    }
}

} // namespace vd

