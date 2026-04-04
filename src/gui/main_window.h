/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  main_window.h — Primary Application Window                               ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * GUI LAYOUT:
 *   ┌────────────────────────────────────────────────────────────────┐
 *   │  ToolBar: [➕ Add] [▶ Start] [⏹ Cancel] [⚙ Settings] [📁]  │
 *   ├────────────────────────────────────────────────────────────────┤
 *   │ ┌──────────────────────────────┐ ┌────────────────────────┐   │
 *   │ │         Drop Zone            │ │   Language Selector    │   │
 *   │ │   🎬 Drop video here         │ │   Source: [Auto ▼]     │   │
 *   │ │   or click to browse         │ │   Target: [Hindi ▼]    │   │
 *   │ └──────────────────────────────┘ └────────────────────────┘   │
 *   ├────────────────────────────────────────────────────────────────┤
 *   │  Pipeline Progress                                            │
 *   │  1. Extract Audio    [████████████████████] 100%              │
 *   │  2. Transcribe       [████████████░░░░░░░░]  60%              │
 *   │  3. Translate         [░░░░░░░░░░░░░░░░░░░]   0%              │
 *   │  ...                                                          │
 *   ├────────────────────────────────────────────────────────────────┤
 *   │  Live Log                                                     │
 *   │  [2026-04-04 15:30:00] [info] Stage 2: Transcription          │
 *   │  [2026-04-04 15:30:12] [info] Got 42 segments                 │
 *   └────────────────────────────────────────────────────────────────┘
 *   │  Status Bar: Ready — drag & drop a video to begin             │
 *   └────────────────────────────────────────────────────────────────┘
 *
 * RESPONSIBILITIES:
 *   - Owns PipelineManager (business logic) and RestApiServer (browser API)
 *   - Connects GUI events to pipeline actions
 *   - Forwards pipeline progress to ProgressPanel via Qt signals
 *   - Supports system tray (minimize to tray on close)
 *   - Accepts drag-and-drop of video files
 *
 * DESIGN PATTERN:
 *   MVC variant: MainWindow acts as Controller between GUI (View)
 *   and PipelineManager (Model).  Progress updates flow via callbacks
 *   bridged through QMetaObject::invokeMethod for thread safety.
 */

#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <memory>
#include "core/pipeline_manager.h"
#include "network/rest_api_server.h"

namespace vd {

class DropZoneWidget;
class LanguageSelector;
class ProgressPanel;
class LogViewer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event)    override;  ///< Minimize to tray
    void dragEnterEvent(QDragEnterEvent*)   override;  ///< Accept video files
    void dropEvent(QDropEvent*)             override;  ///< Start dubbing job

private Q_SLOTS:
    // ─── Toolbar Actions ──────────────────────────────────────────────────
    void on_add_file_clicked();
    void on_start_clicked();
    void on_cancel_clicked();
    void on_settings_clicked();
    void on_open_output_clicked();

    // ─── System Tray ──────────────────────────────────────────────────────
    void on_tray_icon_activated(QSystemTrayIcon::ActivationReason reason);

    // ─── Pipeline Progress (called from background threads via Qt signal) ─
    void on_job_progress(const QString& job_id, const QString& stage,
                         int progress, int eta);

private:
    // ─── Setup Methods ────────────────────────────────────────────────────
    void setup_ui();
    void setup_tray();
    void setup_pipeline();
    void apply_dark_theme();         ///< Catppuccin Mocha color scheme

    // ─── Business Logic ───────────────────────────────────────────────────
    void start_job(const QString& path);
    void update_ui_state();

    // ─── Core Objects (owned) ─────────────────────────────────────────────
    std::unique_ptr<PipelineManager>  pipeline_;      ///< Pipeline orchestrator
    std::unique_ptr<RestApiServer>    api_server_;    ///< REST API on port 7070

    // ─── Child Widgets (Qt parent takes ownership) ────────────────────────
    DropZoneWidget*   drop_zone_{nullptr};
    LanguageSelector* lang_selector_{nullptr};
    ProgressPanel*    progress_panel_{nullptr};
    LogViewer*        log_viewer_{nullptr};
    QSystemTrayIcon*  tray_icon_{nullptr};

    // ─── State ────────────────────────────────────────────────────────────
    QStringList queued_files_;                         ///< Pending file queue
    QString     current_job_id_;                       ///< Active job ID
    bool        job_running_{false};                   ///< Is pipeline active?
};

} // namespace vd
