/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  config_manager.cpp — Thread-Safe Configuration (Implementation)          ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * IMPLEMENTATION NOTES:
 *   - Every getter uses the CFG_GET macro which:
 *     1. Acquires the mutex (lock_guard)
 *     2. Calls json::value(key, default) for safe access with fallback
 *   - Windows API ExpandEnvironmentStringsA() resolves %APPDATA% etc.
 *   - JSON parsing uses "allow comments" mode for user convenience
 */

#include "core/config_manager.h"
#include "core/error_handler.h"

#include <fstream>
#include <sstream>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#include <iomanip>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// Singleton — Meyer's pattern
// ═══════════════════════════════════════════════════════════════════════════════

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cryptography Helpers (DPAPI)
// ═══════════════════════════════════════════════════════════════════════════════

static std::string to_hex(const BYTE* data, DWORD length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (DWORD i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

static std::vector<BYTE> from_hex(const std::string& hex) {
    std::vector<BYTE> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        BYTE byte = static_cast<BYTE>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string ConfigManager::encrypt_string(const std::string& plaintext) const {
    if (plaintext.empty()) return "";
    DATA_BLOB data_in;
    data_in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    data_in.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB data_out;
    if (CryptProtectData(&data_in, L"VideoDubberKey", nullptr, nullptr, nullptr, 0, &data_out)) {
        std::string hex = to_hex(data_out.pbData, data_out.cbData);
        LocalFree(data_out.pbData);
        return hex;
    }
    return ""; // Encryption failed
}

std::string ConfigManager::decrypt_string(const std::string& ciphertext) const {
    if (ciphertext.empty()) return "";
    std::vector<BYTE> raw_bytes = from_hex(ciphertext);
    if (raw_bytes.empty()) return "";

    DATA_BLOB data_in;
    data_in.pbData = raw_bytes.data();
    data_in.cbData = static_cast<DWORD>(raw_bytes.size());

    DATA_BLOB data_out;
    if (CryptUnprotectData(&data_in, nullptr, nullptr, nullptr, nullptr, 0, &data_out)) {
        std::string plaintext(reinterpret_cast<char*>(data_out.pbData), data_out.cbData);
        LocalFree(data_out.pbData);
        return plaintext;
    }
    return ""; // Decryption failed
}

// ═══════════════════════════════════════════════════════════════════════════════
// expand_path — Replace %ENVVAR% tokens with actual values
// ═══════════════════════════════════════════════════════════════════════════════
// Uses Windows ExpandEnvironmentStringsA():
//   "%APPDATA%/VideoDubber"  →  "C:/Users/user/AppData/Roaming/VideoDubber"

std::string ConfigManager::expand_path(const std::string& path) const {
    std::string result = path;
    char expanded[MAX_PATH];
    if (ExpandEnvironmentStringsA(result.c_str(), expanded, MAX_PATH)) {
        result = expanded;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// load — Parse JSON file into memory
// ═══════════════════════════════════════════════════════════════════════════════

void ConfigManager::load(const std::filesystem::path& path) {
    std::lock_guard lock(mutex_);
    config_path_ = path;

    if (!std::filesystem::exists(path)) {
        config_ = nlohmann::json::object();   // Start with empty config
        return;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        throw ConfigException("Cannot open config file: " + path.string());
    }

    try {
        // parse(input, callback, allow_exceptions, ignore_comments)
        config_ = nlohmann::json::parse(f, nullptr, true, true);
    } catch (const nlohmann::json::exception& e) {
        throw ConfigException(std::string("JSON parse error in config: ") + e.what());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// save — Write current config to disk (pretty-printed)
// ═══════════════════════════════════════════════════════════════════════════════

void ConfigManager::save() const {
    std::lock_guard lock(mutex_);
    std::ofstream f(config_path_);
    f << config_.dump(2);   // 2-space indentation
}

// ═══════════════════════════════════════════════════════════════════════════════
// set / get_json — Runtime config modification (from Settings dialog)
// ═══════════════════════════════════════════════════════════════════════════════

void ConfigManager::set(const std::string& key, const nlohmann::json& value) {
    std::lock_guard lock(mutex_);
    config_[key] = value;
}

nlohmann::json ConfigManager::get_json() const {
    std::lock_guard lock(mutex_);
    return config_;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CFG_GET Macro — Safe typed access with default value
// ═══════════════════════════════════════════════════════════════════════════════
// Pattern:  auto val = CFG_GET("key", default_value);
// Effect:   Acquires lock → calls json::value() → returns typed result

#define CFG_GET(key, default_val) \
    ([&]() { \
        std::lock_guard lock(mutex_); \
        return config_.value(key, default_val); \
    }())

// ═══════════════════════════════════════════════════════════════════════════════
// Typed Getters — One-liners using CFG_GET
// ═══════════════════════════════════════════════════════════════════════════════

// -- Backend selection --
std::string ConfigManager::get_translation_backend() const { return CFG_GET("active_translation_backend", std::string("google")); }
std::string ConfigManager::get_tts_backend()         const { return CFG_GET("active_tts_backend",         std::string("gemini")); }

// -- Whisper settings --
std::string ConfigManager::get_whisper_model()       const { return CFG_GET("whisper_model",    std::string("medium")); }
bool        ConfigManager::get_whisper_use_gpu()     const { return CFG_GET("whisper_use_gpu",  true);  }
int         ConfigManager::get_whisper_threads()     const { return CFG_GET("whisper_threads",  4);     }

// -- Language --
std::string ConfigManager::get_target_language()     const { return CFG_GET("default_target_language", std::string("hi"));   }
std::string ConfigManager::get_source_language()     const { return CFG_GET("default_source_language", std::string("auto")); }
std::string ConfigManager::get_tts_voice_id()        const { return CFG_GET("tts_voice_id",            std::string("hi-IN-Standard-A")); }

// -- Audio --
double ConfigManager::get_original_audio_vol()  const { return CFG_GET("original_audio_volume", 0.10); }
double ConfigManager::get_dubbed_audio_vol()    const { return CFG_GET("dubbed_audio_volume",   1.0);  }
std::string ConfigManager::get_audio_mix_mode() const { return CFG_GET("audio_mix_mode", std::string("overlay")); }

// -- Parallelism --
int ConfigManager::get_max_parallel_tts()       const { return CFG_GET("max_parallel_tts_calls",       4); }
int ConfigManager::get_max_parallel_translate() const { return CFG_GET("max_parallel_translate_calls",  8); }

// -- Server --
int ConfigManager::get_api_server_port() const { return CFG_GET("api_server_port", 7070); }
int ConfigManager::get_websocket_port()  const { return CFG_GET("websocket_port",  7071); }

// -- Logging --
std::string ConfigManager::get_log_level()      const { return CFG_GET("log_level",        std::string("info")); }
bool        ConfigManager::get_hardware_decode() const { return CFG_GET("hardware_decode",  true); }
std::string ConfigManager::get_output_format()   const { return CFG_GET("output_format",    std::string("mp4")); }

// -- Retry --
int ConfigManager::get_tts_retry_count()    const { return CFG_GET("tts_retry_count",    3);    }
int ConfigManager::get_tts_retry_delay_ms() const { return CFG_GET("tts_retry_delay_ms", 1000); }

// -- Sub-backends --
std::string ConfigManager::get_libretranslate_url() const { return CFG_GET("libretranslate_url", std::string("http://localhost:5000")); }
std::string ConfigManager::get_edge_tts_voice()     const { return CFG_GET("edge_tts_voice",     std::string("hi-IN-MadhurNeural"));    }

// ═══════════════════════════════════════════════════════════════════════════════
// Path Getters — Expand environment variables before returning
// ═══════════════════════════════════════════════════════════════════════════════

std::string ConfigManager::get_whisper_model_path() const {
    std::lock_guard lock(mutex_);
    auto raw = config_.value("whisper_model_path",
        std::string("%APPDATA%/VideoDubber/models/ggml-medium.bin"));
    return expand_path(raw);
}

std::string ConfigManager::get_temp_dir() const {
    std::lock_guard lock(mutex_);
    auto raw = config_.value("temp_dir", std::string("%APPDATA%/VideoDubber/temp"));
    return expand_path(raw);
}

std::string ConfigManager::get_output_dir() const {
    std::lock_guard lock(mutex_);
    auto raw = config_.value("output_dir", std::string("%USERPROFILE%/Videos/Dubbed"));
    return expand_path(raw);
}

std::string ConfigManager::get_log_file() const {
    std::lock_guard lock(mutex_);
    auto raw = config_.value("log_file", std::string("%APPDATA%/VideoDubber/logs/app.log"));
    return expand_path(raw);
}

// ═══════════════════════════════════════════════════════════════════════════════
// get_api_key — Nested JSON access: config["api_keys"]["service_name"]
// ═══════════════════════════════════════════════════════════════════════════════

std::string ConfigManager::get_api_key(const std::string& service) const {
    std::lock_guard lock(mutex_);
    // Must verify is_object() before calling .contains() — if a user manually
    // edits config.json and sets "api_keys" to a string or number instead of
    // an object, .contains() throws nlohmann::json::type_error, crashing the app.
    if (config_.contains("api_keys") &&
        config_["api_keys"].is_object() &&
        config_["api_keys"].contains(service)) {
        std::string ciphertext = config_["api_keys"][service].get<std::string>();
        return decrypt_string(ciphertext);
    }
    return "";  // Not configured
}

void ConfigManager::set_api_key(const std::string& service, const std::string& key) {
    std::lock_guard lock(mutex_);
    if (!config_.contains("api_keys") || !config_["api_keys"].is_object()) {
        config_["api_keys"] = nlohmann::json::object();
    }
    config_["api_keys"][service] = encrypt_string(key);
}

} // namespace vd
