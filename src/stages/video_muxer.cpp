/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  video_muxer.cpp — Stage 7 Implementation                                ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * SECURITY:
 *   Uses QProcess::execute() instead of std::system() to prevent command
 *   injection via malicious filenames (e.g. `video" && calc.exe && ".mp4`).
 *   QProcess passes arguments directly as an argv array, bypassing the shell.
 */

#include "stages/video_muxer.h"
#include "core/logger.h"
#include "core/error_handler.h"
#include <filesystem>
#include <QProcess>
#include <QStringList>
#include <QString>

namespace vd {

void VideoMuxer::mux(const std::string& video_path,
                      const std::string& dubbed_audio_path,
                      const std::string& output_path,
                      const std::string& mode,
                      double original_volume) {
    std::filesystem::create_directories(
        std::filesystem::path(output_path).parent_path());

    QStringList args;

    if (mode == "replace") {
        args << "-y"
             << "-i" << QString::fromStdString(video_path)
             << "-i" << QString::fromStdString(dubbed_audio_path)
             << "-map" << "0:v" << "-map" << "1:a"
             << "-c:v" << "copy" << "-c:a" << "aac"
             << "-loglevel" << "warning"
             << QString::fromStdString(output_path);
    } else {
        // overlay mode: mix original (lowered) + dubbed
        QString filter = QString("[0:a]volume=%1[orig];[orig][1:a]amix=inputs=2:duration=first[aout]")
                             .arg(original_volume, 0, 'f', 2);
        args << "-y"
             << "-i" << QString::fromStdString(video_path)
             << "-i" << QString::fromStdString(dubbed_audio_path)
             << "-filter_complex" << filter
             << "-map" << "0:v" << "-map" << "[aout]"
             << "-c:v" << "copy" << "-c:a" << "aac"
             << "-loglevel" << "warning"
             << QString::fromStdString(output_path);
    }

    VD_LOG_INFO("VideoMuxer: ffmpeg {}", args.join(" ").toStdString());
    int rc = QProcess::execute("ffmpeg", args);
    if (rc != 0) throw FFmpegException("VideoMuxer: ffmpeg failed with code " + std::to_string(rc));
    VD_LOG_INFO("VideoMuxer: output -> {}", output_path);
}

} // namespace vd

