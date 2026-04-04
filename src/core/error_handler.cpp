/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  error_handler.cpp — FFmpeg Error String Conversion                       ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * Wraps FFmpeg's av_strerror() which converts numeric error codes
 * (like AVERROR(ENOMEM)) into human-readable C strings.
 */

#include "core/error_handler.h"

extern "C" {
#include <libavutil/error.h>
}

namespace vd {

std::string ffmpeg_error_string(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace vd
