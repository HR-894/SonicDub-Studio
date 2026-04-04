#include "gui/main_window.h"
#include "gui/drop_zone_widget.h"
#include "gui/language_selector.h"
#include "gui/progress_panel.h"
#include "gui/settings_dialog.h"
#include "gui/log_viewer.h"
#include "core/config_manager.h"
#include "core/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCloseEvent>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QSplitter>
#include <QGroupBox>
#include <QToolBar>
#include <QStatusBar>

namespace vd {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setup_pipeline();
    setup_ui();
    setup_tray();
    apply_dark_theme();
    setAcceptDrops(true);
    VD_LOG_INFO("VideoDubber started");
}

MainWindow::~MainWindow() {
    if (api_server_) api_server_->stop();
}

void MainWindow::setup_pipeline() {
    pipeline_ = std::make_unique<PipelineManager>();
    pipeline_->set_progress_callback([this](const JobStatus& status) {
        // Emit to GUI thread
        QMetaObject::invokeMethod(this, "on_job_progress", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(status.job_id)),
            Q_ARG(QString, QString::fromStdString(status.stage)),
            Q_ARG(int, status.progress),
            Q_ARG(int, status.eta_seconds));
    });

    auto& cfg = ConfigManager::instance();
    api_server_ = std::make_unique<RestApiServer>(*pipeline_, cfg.get_api_server_port());
    api_server_->start();
}

void MainWindow::setup_ui() {
    setWindowTitle("🎬 VideoDubber");
    setMinimumSize(900, 700);
    resize(1100, 800);

    // Central widget
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(12, 12, 12, 12);

    // ── Toolbar ─────────────────────────────────────
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    auto* add_act  = toolbar->addAction("➕ Add File");
    auto* start_act = toolbar->addAction("▶ Start Dub");
    auto* cancel_act = toolbar->addAction("⏹ Cancel");
    toolbar->addSeparator();
    auto* settings_act = toolbar->addAction("⚙ Settings");
    auto* output_act   = toolbar->addAction("📁 Output Folder");

    connect(add_act,      &QAction::triggered, this, &MainWindow::on_add_file_clicked);
    connect(start_act,    &QAction::triggered, this, &MainWindow::on_start_clicked);
    connect(cancel_act,   &QAction::triggered, this, &MainWindow::on_cancel_clicked);
    connect(settings_act, &QAction::triggered, this, &MainWindow::on_settings_clicked);
    connect(output_act,   &QAction::triggered, this, &MainWindow::on_open_output_clicked);

    // ── Drop Zone + Language Selector ────────────────
    auto* top_layout = new QHBoxLayout;
    drop_zone_    = new DropZoneWidget(this);
    lang_selector_ = new LanguageSelector(this);
    top_layout->addWidget(drop_zone_, 3);
    top_layout->addWidget(lang_selector_, 1);
    main_layout->addLayout(top_layout);

    // ── Splitter: Progress + Log ──────────────────────
    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* prog_group = new QGroupBox("Pipeline Progress", this);
    auto* prog_layout = new QVBoxLayout(prog_group);
    progress_panel_ = new ProgressPanel(this);
    prog_layout->addWidget(progress_panel_);
    splitter->addWidget(prog_group);

    auto* log_group = new QGroupBox("Live Log", this);
    auto* log_layout = new QVBoxLayout(log_group);
    log_viewer_ = new LogViewer(this);
    log_layout->addWidget(log_viewer_);
    splitter->addWidget(log_group);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    main_layout->addWidget(splitter);

    // Status bar
    statusBar()->showMessage("Ready — drag & drop a video file to begin");

    // Connect drop zone
    connect(drop_zone_, &DropZoneWidget::file_dropped,
            this, [this](const QString& path) { start_job(path); });

    // Connect logger to GUI
    Logger::instance().set_gui_callback([this](const std::string& msg, auto level) {
        QMetaObject::invokeMethod(log_viewer_, "append_log", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(msg)),
            Q_ARG(int, static_cast<int>(level)));
    });
}

void MainWindow::setup_tray() {
    tray_icon_ = new QSystemTrayIcon(this);
    // Use a simple icon (in production, load from resources)
    tray_icon_->setToolTip("VideoDubber");
    auto* tray_menu = new QMenu(this);
    tray_menu->addAction("Show", this, &MainWindow::show);
    tray_menu->addAction("Quit", qApp, &QApplication::quit);
    tray_icon_->setContextMenu(tray_menu);
    tray_icon_->show();

    connect(tray_icon_, &QSystemTrayIcon::activated,
            this, &MainWindow::on_tray_icon_activated);
}

void MainWindow::apply_dark_theme() {
    setStyleSheet(R"(
        QMainWindow, QWidget { background-color: #1e1e2e; color: #cdd6f4; }
        QGroupBox { border: 1px solid #313244; border-radius: 6px;
                    margin-top: 8px; font-weight: bold; color: #89b4fa; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { background: #313244; border: 1px solid #45475a;
                      border-radius: 4px; padding: 6px 14px; color: #cdd6f4; }
        QPushButton:hover { background: #45475a; }
        QPushButton:pressed { background: #585b70; }
        QToolBar { background: #181825; border: none; spacing: 4px; }
        QToolBar QToolButton { background: #313244; border-radius: 4px;
                                padding: 4px 10px; color: #cdd6f4; }
        QToolBar QToolButton:hover { background: #45475a; }
        QComboBox { background: #313244; border: 1px solid #45475a;
                    border-radius: 4px; padding: 4px 8px; color: #cdd6f4; }
        QScrollBar:vertical { background: #1e1e2e; width: 8px; }
        QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; }
        QSplitter::handle { background: #313244; }
        QStatusBar { background: #181825; color: #a6adc8; }
    )");
}

void MainWindow::on_add_file_clicked() {
    auto files = QFileDialog::getOpenFileNames(this, "Select Video",
                 QString(), "Video Files (*.mp4 *.mkv *.avi *.mov *.webm);;All Files (*)");
    for (auto& f : files) start_job(f);
}

void MainWindow::start_job(const QString& path) {
    JobConfig cfg;
    cfg.input_path          = path.toStdString();
    cfg.source_language     = lang_selector_->source_language().toStdString();
    cfg.target_language     = lang_selector_->target_language().toStdString();
    cfg.tts_backend         = ConfigManager::instance().get_tts_backend();
    cfg.translation_backend = ConfigManager::instance().get_translation_backend();
    cfg.audio_mix_mode      = ConfigManager::instance().get_audio_mix_mode();

    auto stem = QFileInfo(path).baseName();
    cfg.output_path = QString::fromStdString(ConfigManager::instance().get_output_dir())
        + "/" + stem + "_dubbed.mp4";
    cfg.output_path = cfg.output_path.toStdString();

    current_job_id_ = QString::fromStdString(pipeline_->start_job(cfg));
    job_running_    = true;
    statusBar()->showMessage("Dubbing: " + path);
    progress_panel_->reset();
}

void MainWindow::on_start_clicked() { on_add_file_clicked(); }

void MainWindow::on_cancel_clicked() {
    if (!current_job_id_.isEmpty()) {
        pipeline_->cancel_job(current_job_id_.toStdString());
        statusBar()->showMessage("Cancelled");
        job_running_ = false;
    }
}

void MainWindow::on_settings_clicked() {
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::on_open_output_clicked() {
    auto dir = ConfigManager::instance().get_output_dir();
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(dir)));
}

void MainWindow::on_job_progress(const QString& job_id, const QString& stage,
                                 int progress, int eta) {
    if (job_id != current_job_id_) return;

    progress_panel_->update_stage(stage, progress);
    statusBar()->showMessage(QString("Stage: %1 — %2%").arg(stage).arg(progress));

    if (stage == "done") {
        job_running_ = false;
        statusBar()->showMessage("✅ Done!");
        tray_icon_->showMessage("VideoDubber", "Dubbing complete! 🎉",
                                 QSystemTrayIcon::Information, 4000);
    } else if (stage == "failed") {
        job_running_ = false;
        statusBar()->showMessage("❌ Failed — check logs");
        tray_icon_->showMessage("VideoDubber", "Job failed. Check logs.",
                                 QSystemTrayIcon::Critical, 4000);
    }
}

void MainWindow::on_tray_icon_activated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        show(); raise(); activateWindow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore(); // minimize to tray
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) start_job(url.toLocalFile());
    }
}

} // namespace vd
