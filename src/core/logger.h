/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  logger.h — Centralized Logging System                                    ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Provides a singleton Logger wrapping spdlog. Supports three output sinks
 *   simultaneously (console, rotating file, and GUI widget).
 *
 * ARCHITECTURE:
 *   ┌──────────────┐
 *   │  VD_LOG_*()  │  (convenience macros)
 *   └──────┬───────┘
 *          ▼
 *   ┌──────────────┐     ┌────────────────────┐
 *   │   Logger     │────►│ spdlog::logger      │
 *   │  (singleton) │     │                      │
 *   └──────────────┘     │  Sinks:              │
 *                        │  ├─ Console (color)   │
 *                        │  ├─ File (rotating)   │
 *                        │  └─ GUI (callback)    │
 *                        └────────────────────────┘
 *
 * THREAD SAFETY:
 *   spdlog is inherently thread-safe. All VD_LOG_* macros
 *   may be called from any thread without locking.
 *
 * USAGE:
 *   VD_LOG_INFO("Processing segment {}", seg.id);
 *   VD_LOG_ERROR("FFmpeg error: {}", error_string);
 *
 * DEPENDENCIES:
 *   spdlog (header-only or compiled)
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <string>
#include <memory>
#include <functional>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// Logger — Thread-safe singleton log manager
// ═══════════════════════════════════════════════════════════════════════════════

class Logger {
public:
    /// Access the single Logger instance.  Thread-safe (C++11 magic statics).
    static Logger& instance();

    /**
     * Initialize logging sinks.
     * @param log_file   Path to rotating log file (empty = skip file sink)
     * @param level      Minimum log level (trace/debug/info/warn/error/critical)
     */
    void init(const std::string& log_file, spdlog::level::level_enum level);

    /**
     * Register a callback to forward log messages to the GUI.
     * Called from the GUI thread during MainWindow construction.
     * @param cb  Callback receiving (formatted_message, log_level)
     */
    void set_gui_callback(
        std::function<void(const std::string&, spdlog::level::level_enum)> cb);

    /// Access the underlying spdlog logger.
    std::shared_ptr<spdlog::logger> get() { return logger_; }

private:
    Logger() = default;  // Singleton — use instance()

    std::shared_ptr<spdlog::logger> logger_;
    std::function<void(const std::string&, spdlog::level::level_enum)> gui_callback_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Convenience Macros — VD_LOG_INFO(...), VD_LOG_ERROR(...), etc.
// ═══════════════════════════════════════════════════════════════════════════════
// These wrap spdlog's fmt-based syntax:  VD_LOG_INFO("value={}", x);

#define VD_LOG_TRACE(...)    vd::Logger::instance().get()->trace(__VA_ARGS__)
#define VD_LOG_DEBUG(...)    vd::Logger::instance().get()->debug(__VA_ARGS__)
#define VD_LOG_INFO(...)     vd::Logger::instance().get()->info(__VA_ARGS__)
#define VD_LOG_WARN(...)     vd::Logger::instance().get()->warn(__VA_ARGS__)
#define VD_LOG_ERROR(...)    vd::Logger::instance().get()->error(__VA_ARGS__)
#define VD_LOG_CRITICAL(...) vd::Logger::instance().get()->critical(__VA_ARGS__)

} // namespace vd
