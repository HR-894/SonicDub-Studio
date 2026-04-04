#include "utils/ffmpeg_wrapper.h"

namespace vd {

AVFormatContextPtr open_input(const std::string& path) {
    AVFormatContext* raw = nullptr;
    int ret = avformat_open_input(&raw, path.c_str(), nullptr, nullptr);
    VD_CHECK_FFMPEG(ret, "Cannot open input: " + path);
    AVFormatContextPtr ctx(raw);
    ret = avformat_find_stream_info(raw, nullptr);
    VD_CHECK_FFMPEG(ret, "Cannot find stream info");
    return ctx;
}

int find_audio_stream(AVFormatContext* fmt_ctx) {
    int idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (idx < 0) throw FFmpegException("No audio stream found");
    return idx;
}

AVCodecContextPtr open_decoder(AVFormatContext* fmt_ctx, int stream_idx) {
    auto* stream = fmt_ctx->streams[stream_idx];
    auto* codec  = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) throw FFmpegException("Decoder not found");

    auto ctx = AVCodecContextPtr(avcodec_alloc_context3(codec));
    if (!ctx) throw FFmpegException("Cannot allocate codec context");

    int ret = avcodec_parameters_to_context(ctx.get(), stream->codecpar);
    VD_CHECK_FFMPEG(ret, "Cannot copy codec params");

    ret = avcodec_open2(ctx.get(), codec, nullptr);
    VD_CHECK_FFMPEG(ret, "Cannot open decoder");
    return ctx;
}

int64_t get_duration_ms(AVFormatContext* fmt_ctx) {
    if (fmt_ctx->duration == AV_NOPTS_VALUE) return 0;
    return fmt_ctx->duration / (AV_TIME_BASE / 1000);
}

} // namespace vd
