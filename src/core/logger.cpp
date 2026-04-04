/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  logger.cpp — Centralized Logging System (Implementation)                 ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * IMPLEMENTATION NOTES:
 *   - Uses a custom spdlog sink (GuiSink) to forward messages to the Qt GUI
 *   - Console sink provides colored output to stderr
 *   - File sink rotates at 5 MB with 3 backup files
 *   - All sinks share the same logger instance for consistent output
 */

#include "core/logger.h"
#include <spdlog/sinks/base_sink.h>
#include <mutex>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// GuiSink — Custom spdlog sink forwarding to a std::function callback
// ═══════════════════════════════════════════════════════════════════════════════
// This allows the LogViewer widget to receive log messages in real time.
// Template parameter Mutex controls thread safety (std::mutex for production).

template<typename Mutex>
class GuiSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit GuiSink(std::function<void(const std::string&, spdlog::level::level_enum)> cb)
        : callback_(std::move(cb)) {}

protected:
    /// Called by spdlog for each log message.  Formats and forwards to GUI.
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

        if (callback_) {
            callback_(fmt::to_string(formatted), msg.level);
        }
    }

    void flush_() override { /* GUI sink doesn't buffer */ }

private:
    std::function<void(const std::string&, spdlog::level::level_enum)> callback_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Logger::instance — Meyer's Singleton
// ═══════════════════════════════════════════════════════════════════════════════
// Thread-safe guaranteed by C++11 §6.7: "concurrent execution shall wait"

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Logger::init — Create sinks and configure formatting
// ═══════════════════════════════════════════════════════════════════════════════

void Logger::init(const std::string& log_file, spdlog::level::level_enum level) {

    std::vector<spdlog::sink_ptr> sinks;

    // ── Sink 1: Console with ANSI colors ──────────────────────────────────
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(level);
    sinks.push_back(console_sink);

    // ── Sink 2: Rotating file (5 MB × 3 backups) ─────────────────────────
    if (!log_file.empty()) {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file,
                5 * 1024 * 1024,   // 5 MB max size
                3                  // Keep 3 rotated files
            );
            file_sink->set_level(level);
            sinks.push_back(file_sink);
        } catch (const std::exception& e) {
            // Non-fatal: continue logging to console if file fails
        }
    }

    // ── Create multi-sink logger ──────────────────────────────────────────
    logger_ = std::make_shared<spdlog::logger>("vd", sinks.begin(), sinks.end());
    logger_->set_level(level);

    // Format:  [2026-04-04 15:30:00.123] [info] [thread-id] message
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    spdlog::register_logger(logger_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Logger::set_gui_callback — Attach GUI log forwarding
// ═══════════════════════════════════════════════════════════════════════════════

void Logger::set_gui_callback(
    std::function<void(const std::string&, spdlog::level::level_enum)> cb)
{
    gui_callback_ = std::move(cb);

    if (logger_) {
        auto gui_sink = std::make_shared<GuiSink<std::mutex>>(gui_callback_);
        logger_->sinks().push_back(gui_sink);
    }
}

} // namespace vd
