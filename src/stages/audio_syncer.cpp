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

    QStringList args;
    args << "-y" << "-i" << QString::fromStdString(input_wav);

    if (input_duration_ms <= target_duration_ms) {
        // If TTS is shorter or equal, just pad with silence
        args << "-af" << "apad"
             << "-t" << QString::number(target_duration_ms / 1000.0, 'f', 3);
        VD_LOG_DEBUG("AudioSyncer: input_dur={} ms, target_dur={} ms, padding with silence",
                     input_duration_ms, target_duration_ms);
    } else {
        // If TTS is longer, use high-quality rubberband filtering to speed it up
        double ratio = static_cast<double>(input_duration_ms) / target_duration_ms;
        // Clamp ratio to sane range
        ratio = std::clamp(ratio, 1.001, 20.0);
        std::string filter_str = std::format("rubberband=tempo={:.6f}", ratio);
        args << "-af" << QString::fromStdString(filter_str);
        
        VD_LOG_DEBUG("AudioSyncer: ratio={:.3f}, filter={}", ratio, filter_str);
    }

    args << "-ar" << "16000"
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

