#include "utils/file_utils.h"
#include <fstream>
#include <random>
#include <chrono>
#include <sstream>

namespace vd::FileUtils {

std::string get_temp_path(const std::string& base_dir, const std::string& suffix) {
    static std::mt19937 rng(
        static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist;
    std::ostringstream oss;
    oss << base_dir << "/" << std::hex << dist(rng) << suffix;
    return oss.str();
}

void ensure_dir(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
}

bool is_url(const std::string& path) {
    return path.starts_with("http://") || path.starts_with("https://")
        || path.starts_with("rtmp://");
}

std::string get_extension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

std::string replace_extension(const std::string& path, const std::string& ext) {
    return std::filesystem::path(path).replace_extension(ext).string();
}

std::string get_filename_stem(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

void cleanup_dir(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) throw std::runtime_error("Cannot open file: " + path);
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("Cannot write file: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace vd::FileUtils
