/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  ffmpeg_wrapper.h — RAII Wrappers for FFmpeg C Types                      ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   FFmpeg is a C library with manual memory management. This header provides
 *   C++ RAII wrappers using std::unique_ptr with custom deleters, ensuring
 *   resources are always freed even if exceptions occur.
 *
 * WRAPPER TABLE:
 *
 *   C Type            │  RAII Wrapper          │  Deleter Action
 *   ──────────────────┼───────────────────────┼─────────────────────────
 *   AVFormatContext   │  AVFormatContextPtr    │  avformat_close_input()
 *   AVCodecContext    │  AVCodecContextPtr     │  avcodec_free_context()
 *   AVPacket          │  AVPacketPtr           │  av_packet_free()
 *   AVFrame           │  AVFramePtr            │  av_frame_free()
 *   SwrContext        │  SwrContextPtr         │  swr_free()
 *   AVFilterGraph     │  AVFilterGraphPtr      │  avfilter_graph_free()
 *
 * DESIGN PATTERN:  RAII (Resource Acquisition Is Initialization)
 *   - Acquire in constructor / factory function
 *   - Release automatically in destructor
 *   - Exception-safe: no leaks even on early returns or throws
 *
 * UTILITY FUNCTIONS:
 *   open_input()        — Open any media file/URL
 *   find_audio_stream() — Find best audio stream index
 *   open_decoder()      — Create and configure decoder
 *   get_duration_ms()   — Get media duration in milliseconds
 *
 * DEPENDENCIES:
 *   FFmpeg (libavformat, libavcodec, libavutil, libavfilter, libswresample)
 */

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <string>
#include "core/error_handler.h"

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// RAII Deleters — Custom delete functors for std::unique_ptr
// ═══════════════════════════════════════════════════════════════════════════════

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) avformat_close_input(&ctx);
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) avcodec_free_context(&ctx);
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) av_packet_free(&pkt);
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* f) const {
        if (f) av_frame_free(&f);
    }
};

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const {
        if (ctx) swr_free(&ctx);
    }
};

struct AVFilterGraphDeleter {
    void operator()(AVFilterGraph* g) const {
        if (g) avfilter_graph_free(&g);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Smart Pointer Type Aliases
// ═══════════════════════════════════════════════════════════════════════════════

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr  = std::unique_ptr<AVCodecContext,  AVCodecContextDeleter>;
using AVPacketPtr        = std::unique_ptr<AVPacket,        AVPacketDeleter>;
using AVFramePtr         = std::unique_ptr<AVFrame,         AVFrameDeleter>;
using SwrContextPtr      = std::unique_ptr<SwrContext,      SwrContextDeleter>;
using AVFilterGraphPtr   = std::unique_ptr<AVFilterGraph,   AVFilterGraphDeleter>;

// ═══════════════════════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════════════════════

/// Open an input media file/URL and probe stream information.
/// @throws FFmpegException if file cannot be opened or stream info not found
AVFormatContextPtr open_input(const std::string& path);

/// Find the index of the best audio stream.  Returns stream index.
/// @throws FFmpegException if no audio stream exists
int find_audio_stream(AVFormatContext* fmt_ctx);

/// Allocate decoder context for the given stream and open the codec.
/// @throws FFmpegException if codec not found or cannot be opened
AVCodecContextPtr open_decoder(AVFormatContext* fmt_ctx, int stream_idx);

/// Get total media duration in milliseconds.
/// Returns 0 if duration is unknown.
int64_t get_duration_ms(AVFormatContext* fmt_ctx);

} // namespace vd
