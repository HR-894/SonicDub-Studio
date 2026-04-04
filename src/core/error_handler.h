/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  error_handler.h — Exception Hierarchy & Error Utilities                  ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Defines a type-safe exception hierarchy so callers can catch errors
 *   from specific subsystems (FFmpeg, Whisper, TTS, etc.) independently.
 *
 * EXCEPTION HIERARCHY (Inheritance Tree):
 *
 *   std::runtime_error
 *   └── VDException            ← base for all VideoDubber errors
 *       ├── PipelineException  ← pipeline orchestration errors
 *       ├── FFmpegException    ← libavcodec/avformat/avfilter errors
 *       ├── NetworkException   ← HTTP / WebSocket errors (libcurl, Beast)
 *       ├── WhisperException   ← whisper.cpp model/inference errors
 *       ├── TTSException       ← text-to-speech backend errors
 *       └── ConfigException    ← config file parse / missing key errors
 *
 * USAGE:
 *   try {
 *       transcriber.transcribe(wav_path);
 *   } catch (const vd::WhisperException& e) {
 *       // Handle whisper-specific error
 *   } catch (const vd::VDException& e) {
 *       // Handle any VideoDubber error
 *   }
 *
 * DESIGN PATTERN:
 *   Exception Shielding — each subsystem throws its own type so that
 *   the PipelineManager can log and recover per-stage.
 *
 * C++20 FEATURE:  std::source_location for automatic file/line capture.
 */

#pragma once

#include <stdexcept>
#include <string>
#include <source_location>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// VDException — Base exception for all VideoDubber errors
// ═══════════════════════════════════════════════════════════════════════════════
// Automatically captures file + line via C++20 std::source_location.

struct VDException : std::runtime_error {
    explicit VDException(
        const std::string& msg,
        std::source_location loc = std::source_location::current())
        : std::runtime_error(msg), location(loc) {}

    std::source_location location;   ///< Where the exception was thrown
};

// ═══════════════════════════════════════════════════════════════════════════════
// Specialized Exceptions — One per subsystem
// ═══════════════════════════════════════════════════════════════════════════════

/// Pipeline orchestration errors (stage ordering, job not found, etc.)
struct PipelineException : VDException {
    using VDException::VDException;
    std::string stage_name;          ///< Which pipeline stage failed
};

/// FFmpeg library errors (returned from libavcodec, libavformat, etc.)
struct FFmpegException : VDException {
    using VDException::VDException;
    int error_code{0};               ///< Raw FFmpeg error code (negative int)
};

/// HTTP / REST / WebSocket errors
struct NetworkException : VDException {
    using VDException::VDException;
    int http_status{0};              ///< HTTP status code (e.g. 401, 500)
};

/// whisper.cpp model loading or inference errors
struct WhisperException : VDException { using VDException::VDException; };

/// Text-to-speech backend errors (Google, Gemini, ElevenLabs, Edge)
struct TTSException     : VDException { using VDException::VDException; };

/// Configuration file errors (missing keys, bad JSON, etc.)
struct ConfigException  : VDException { using VDException::VDException; };

// ═══════════════════════════════════════════════════════════════════════════════
// Utility — Convert FFmpeg error code to human-readable string
// ═══════════════════════════════════════════════════════════════════════════════
/// Wraps av_strerror() from libavutil.
std::string ffmpeg_error_string(int errnum);

// ═══════════════════════════════════════════════════════════════════════════════
// Macro — VD_CHECK_FFMPEG(ret, msg)
// ═══════════════════════════════════════════════════════════════════════════════
// Usage:  int ret = avformat_open_input(...);
//         VD_CHECK_FFMPEG(ret, "Cannot open input file");
// If ret < 0, throws FFmpegException with decoded error string.

#define VD_CHECK_FFMPEG(ret, msg) \
    do { \
        if ((ret) < 0) { \
            throw vd::FFmpegException( \
                std::string(msg) + ": " + vd::ffmpeg_error_string(ret)); \
        } \
    } while(0)

} // namespace vd
