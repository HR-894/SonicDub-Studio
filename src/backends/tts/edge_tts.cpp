// edge_tts.cpp
#include "backends/tts/edge_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include "utils/file_utils.h"
#include <fstream>
#include <filesystem>

#include <QProcess>
#include <QStringList>
#include <QString>

namespace vd {

EdgeTTS::EdgeTTS() {
    voice_ = ConfigManager::instance().get_edge_tts_voice();
}

std::vector<uint8_t> EdgeTTS::synthesize(const std::string& text,
                                         const std::string& lang,
                                         const std::string& voice_id) {
    if (text.empty()) return {};

    auto tmp_mp3 = FileUtils::get_temp_path(
        ConfigManager::instance().get_temp_dir(), ".mp3");
    auto tmp_wav = FileUtils::get_temp_path(
        ConfigManager::instance().get_temp_dir(), ".wav");

    // edge-tts is a Python package, invoke via QProcess (NOT std::system)
    // Using QStringList for argument isolation prevents command injection:
    // e.g., text like "Hello && format C:" is passed as a single argument,
    // never interpreted by the shell.
    auto use_voice = voice_id.empty() ? voice_ : voice_id;

    QStringList tts_args;
    tts_args << "--voice" << QString::fromStdString(use_voice)
             << "--text"  << QString::fromStdString(text)
             << "--write-media" << QString::fromStdString(tmp_mp3);

    int rc = QProcess::execute("edge-tts", tts_args);
    if (rc != 0) throw TTSException("edge-tts subprocess failed (exit code: " + std::to_string(rc) + ")");

    // Convert MP3 -> WAV using QProcess (also safe from injection)
    QStringList conv_args;
    conv_args << "-y"
              << "-i" << QString::fromStdString(tmp_mp3)
              << "-ar" << "16000"
              << "-ac" << "1"
              << "-loglevel" << "quiet"
              << QString::fromStdString(tmp_wav);

    int conv_rc = QProcess::execute("ffmpeg", conv_args);
    if (conv_rc != 0) {
        VD_LOG_WARN("EdgeTTS: ffmpeg conversion failed (exit code: {})", conv_rc);
    }

    auto data = FileUtils::read_file(tmp_wav);

    std::filesystem::remove(tmp_mp3);
    std::filesystem::remove(tmp_wav);

    return data;
}

} // namespace vd
