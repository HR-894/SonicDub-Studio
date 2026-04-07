/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  main.cpp — Application Entry Point                                       ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * STARTUP SEQUENCE:
 *
 *   1. Initialize QApplication (Qt event loop)
 *   2. Locate config directory: %APPDATA%/VideoDubber/
 *   3. Create required subdirectories (temp, logs, models)
 *   4. Copy default config.json if first run
 *   5. Load configuration via ConfigManager singleton
 *   6. Initialize spdlog logging system
 *   7. Create output directory
 *   8. Launch MainWindow (starts GUI + REST API server)
 *   9. Enter Qt event loop (app.exec())
 *
 * CONFIG FILE LOCATION:
 *   Windows: C:\Users\<user>\AppData\Roaming\VideoDubber\config.json
 *
 * DIRECTORY LAYOUT AFTER FIRST RUN:
 *   %APPDATA%/VideoDubber/
 *   ├── config.json           — User settings & API keys
 *   ├── temp/                 — Temporary pipeline files (auto-cleaned)
 *   ├── logs/                 — Rotating log files
 *   └── models/               — Whisper GGML model files
 *
 *   %USERPROFILE%/Videos/Dubbed/
 *   └── *.mp4                 — Final dubbed video outputs
 */

#include <QApplication>
#include <QDir>
#include <filesystem>
#include <curl/curl.h>

#include "core/config_manager.h"
#include "core/logger.h"
#include "gui/main_window.h"

int main(int argc, char* argv[]) {

    // ── Step 1: Qt Application ───────────────────────────────────────────
    QApplication app(argc, argv);
    curl_global_init(CURL_GLOBAL_ALL);
    app.setApplicationName("VideoDubber");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("VideoDubber");

    // ── Step 2: Config Directory ─────────────────────────────────────────
    const char* appdata_env = std::getenv("APPDATA");
    std::string appdata = appdata_env ? appdata_env : ".";
    std::filesystem::path config_dir  = appdata + "/VideoDubber";
    std::filesystem::path config_path = config_dir / "config.json";

    // ── Step 3: Create Directory Structure ────────────────────────────────
    std::filesystem::create_directories(config_dir);
    std::filesystem::path temp_dir = config_dir / "temp";
    std::filesystem::create_directories(temp_dir);
    std::filesystem::create_directories(config_dir / "logs");
    std::filesystem::create_directories(config_dir / "models");

    // Clear orphaned temp folders left from crashes
    try {
        for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
            std::filesystem::remove_all(entry.path());
        }
    } catch (...) {
        // Ignore permission or locking errors on startup
    }

    // ── Step 4: Copy Default Config (first run only) ─────────────────────
    if (!std::filesystem::exists(config_path)) {
        if (std::filesystem::exists("config.json")) {
            std::filesystem::copy("config.json", config_path);
        }
    }

    // ── Step 5: Load Configuration ───────────────────────────────────────
    auto& cfg = vd::ConfigManager::instance();
    cfg.load(config_path);

    // ── Step 6: Initialize Logging ───────────────────────────────────────
    auto level_str = cfg.get_log_level();
    auto level     = spdlog::level::from_str(level_str);
    vd::Logger::instance().init(cfg.get_log_file(), level);

    // ── Step 7: Ensure Output Directory ──────────────────────────────────
    std::filesystem::create_directories(cfg.get_output_dir());

    // ── Step 8 & 9: Launch GUI & Enter Event Loop ────────────────────────
    vd::MainWindow window;
    window.show();

    int result = app.exec();

    // ── Cleanup ────────────────────────────────────────────────────────
    curl_global_cleanup();

    return result;
}
