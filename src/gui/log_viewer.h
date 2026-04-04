/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  log_viewer.h — Real-Time Color-Coded Log Output Widget                   ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * VISUAL OUTPUT:
 *   ┌──────────────────────────────────────────────────────┐
 *   │ [15:30:00] [info]  Stage 2: Transcription            │  green
 *   │ [15:30:12] [info]  Got 42 segments                   │  green
 *   │ [15:30:13] [warn]  Translation retry 1: timeout      │  yellow
 *   │ [15:30:15] [error] TTS failed for segment 7          │  red
 *   │ [15:30:16] [debug] ffmpeg cmd: ...                   │  gray
 *   └──────────────────────────────────────────────────────┘
 *
 * COLOR CODING (Catppuccin Mocha palette):
 *   trace / debug  → #6c7086 (overlay0, gray)
 *   info           → #a6e3a1 (green)
 *   warn           → #f9e2af (yellow)
 *   error / critical → #f38ba8 (red, bold)
 *
 * THREAD SAFETY:
 *   append_log() is called from Logger's GuiSink via QMetaObject::invokeMethod
 *   with Qt::QueuedConnection, ensuring the slot runs on the GUI thread.
 */

#pragma once

#include <QWidget>

class QPlainTextEdit;

namespace vd {

class LogViewer : public QWidget {
    Q_OBJECT

public:
    explicit LogViewer(QWidget* parent = nullptr);

public Q_SLOTS:
    /// Append a colored log line.  Called from any thread via queued connection.
    void append_log(const QString& msg, int level);

    /// Clear all log output.
    void clear();

private:
    QPlainTextEdit* text_edit_{nullptr};  ///< Read-only, monospace, dark background
};

} // namespace vd
