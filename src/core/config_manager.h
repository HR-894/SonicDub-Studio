/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  config_manager.h — Thread-Safe Application Configuration                 ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Manages all application settings via a JSON config file located at
 *   %APPDATA%/VideoDubber/config.json.  Provides typed getters for every
 *   setting and thread-safe access from any pipeline thread.
 *
 * CONFIG FILE STRUCTURE:
 *   {
 *     "active_translation_backend": "google",     // ← get_translation_backend()
 *     "active_tts_backend": "gemini",              // ← get_tts_backend()
 *     "whisper_model_path": "%APPDATA%/.../ggml-medium.bin",
 *     "api_keys": {
 *       "gemini": "AIza...",
 *       "google_translate": "..."
 *     },
 *     ...
 *   }
 *
 * DESIGN PATTERN:
 *   - Singleton (thread-safe Meyer's singleton via instance())
 *   - Observer-ready: set() + save() allow runtime config updates from GUI
 *
 * THREAD SAFETY:
 *   All getters/setters acquire mutex_ before accessing config_ JSON.
 *
 * DSA NOTES:
 *   - Internal store: nlohmann::json (backed by std::map<string, json>)
 *     → O(log n) key lookup, O(1) after iterator obtained
 *   - Environment variable expansion uses Windows ExpandEnvironmentStringsA()
 *
 * DEPENDENCIES:
 *   nlohmann/json, <mutex>, <filesystem>, Windows.h (for path expansion)
 */

#pragma once

#include <string>
#include <mutex>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace vd {

class ConfigManager {
public:
    // ─── Singleton Access ─────────────────────────────────────────────────
    static ConfigManager& instance();

    // ─── File I/O ─────────────────────────────────────────────────────────

    /// Load config from disk.  Creates empty config if file doesn't exist.
    void load(const std::filesystem::path& path);

    /// Persist current config to disk.
    void save() const;

    // ─── Typed Getters (each acquires mutex internally) ───────────────────

    // Backend selection
    std::string get_translation_backend()    const;  ///< e.g. "google", "deepl"
    std::string get_tts_backend()            const;  ///< e.g. "gemini", "edge"

    // Whisper settings
    std::string get_whisper_model_path()     const;  ///< Expanded absolute path
    std::string get_whisper_model()          const;  ///< e.g. "medium", "large-v3"
    bool        get_whisper_use_gpu()        const;  ///< true = CUDA acceleration
    int         get_whisper_threads()        const;  ///< Number of CPU threads

    // Language settings
    std::string get_target_language()        const;  ///< e.g. "hi" (Hindi)
    std::string get_source_language()        const;  ///< "auto" or ISO code

    // TTS settings
    std::string get_tts_voice_id()           const;  ///< Voice identifier

    // Audio mixing
    double      get_original_audio_vol()     const;  ///< 0.0–1.0 (original track volume)
    double      get_dubbed_audio_vol()       const;  ///< 0.0–1.0 (dubbed track volume)
    std::string get_audio_mix_mode()         const;  ///< "overlay" or "replace"

    // Parallelism
    int         get_max_parallel_tts()       const;  ///< Thread pool size for TTS
    int         get_max_parallel_translate()  const;  ///< Thread pool size for translation

    // Paths (all support %APPDATA% expansion)
    std::string get_temp_dir()               const;  ///< Temporary file directory
    std::string get_output_dir()             const;  ///< Final output directory
    std::string get_log_file()               const;  ///< Log file path

    // Server
    int         get_api_server_port()        const;  ///< REST API port (default 7070)
    int         get_websocket_port()         const;  ///< WebSocket port (future use)

    // Logging
    std::string get_log_level()              const;  ///< e.g. "info", "debug"

    // Video
    bool        get_hardware_decode()        const;  ///< Use GPU video decoding
    std::string get_output_format()          const;  ///< e.g. "mp4"

    // Retry
    int         get_tts_retry_count()        const;  ///< Max retries per TTS call
    int         get_tts_retry_delay_ms()     const;  ///< Base delay between retries

    // API Keys
    std::string get_api_key(const std::string& service) const;  ///< e.g. "gemini"

    // Edge TTS
    std::string get_libretranslate_url()     const;  ///< e.g. "http://localhost:5000"
    std::string get_edge_tts_voice()         const;  ///< e.g. "hi-IN-MadhurNeural"

    // ─── Generic Access ───────────────────────────────────────────────────

    /// Expand %APPDATA% etc. in a path string (Windows-specific).
    std::string expand_path(const std::string& path) const;

    /// Set any config key at runtime (for Settings dialog).
    void set(const std::string& key, const nlohmann::json& value);

    /// Get full JSON snapshot (for Settings dialog initialization).
    nlohmann::json get_json() const;

private:
    ConfigManager() = default;           // Singleton — use instance()

    mutable std::mutex     mutex_;       ///< Guards all config_ access
    nlohmann::json         config_;      ///< Parsed JSON config tree
    std::filesystem::path  config_path_; ///< Path to config.json on disk
};

} // namespace vd
