// edge_tts.cpp
#include "backends/tts/edge_tts.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include "utils/file_utils.h"
#include <cstdlib>
#include <format>
#include <fstream>
#include <filesystem>

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

    // edge-tts is a Python package, invoke via subprocess
    // Install: pip install edge-tts
    auto use_voice = voice_id.empty() ? voice_ : voice_id;
    std::string cmd = std::format(
        "edge-tts --voice \"{}\" --text \"{}\" --write-media \"{}\" 2>nul",
        use_voice, text, tmp_mp3);

    int rc = std::system(cmd.c_str());
    if (rc != 0) throw TTSException("edge-tts subprocess failed");

    // Convert MP3 -> WAV
    std::string conv = std::format(
        "ffmpeg -y -i \"{}\" -ar 16000 -ac 1 \"{}\" -loglevel quiet",
        tmp_mp3, tmp_wav);
    std::system(conv.c_str());

    auto data = FileUtils::read_file(tmp_wav);

    std::filesystem::remove(tmp_mp3);
    std::filesystem::remove(tmp_wav);

    return data;
}

} // namespace vd
