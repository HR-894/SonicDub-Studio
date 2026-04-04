/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  progress_panel.h — Multi-Stage Pipeline Progress Visualization           ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * VISUAL OUTPUT:
 *   1. Extract Audio    [████████████████████] 100%  ✓
 *   2. Transcribe       [████████████████████] 100%  ✓
 *   3. Translate         [█████████░░░░░░░░░░]  50%  ◄ active (animated)
 *   4. Synthesize TTS   [░░░░░░░░░░░░░░░░░░░]   0%
 *   5. Sync Timing      [░░░░░░░░░░░░░░░░░░░]   0%
 *   6. Mux Video        [░░░░░░░░░░░░░░░░░░░]   0%
 *
 * BEHAVIOR:
 *   - Completed stages show solid green (Catppuccin "green" #a6e3a1)
 *   - Active stage shows indeterminate animation (marquee QProgressBar)
 *   - Future stages stay at 0% (default blue)
 *   - On "done": all bars turn green at 100%
 *   - On "failed": active bar turns red (#f38ba8)
 */

#pragma once

#include <QWidget>

class QProgressBar;

namespace vd {

class ProgressPanel : public QWidget {
    Q_OBJECT

public:
    explicit ProgressPanel(QWidget* parent = nullptr);

    /// Reset all progress bars to 0% (for new job).
    void reset();

    /// Update display for current pipeline stage.
    void update_stage(const QString& stage, int progress);

private:
    // ─── Per-Stage Progress Bars ──────────────────────────────────────────
    QProgressBar* extract_bar_{nullptr};       ///< Stage 1: Extraction
    QProgressBar* transcribe_bar_{nullptr};    ///< Stage 2: Transcription
    QProgressBar* translate_bar_{nullptr};     ///< Stage 3: Translation
    QProgressBar* tts_bar_{nullptr};           ///< Stage 4: TTS
    QProgressBar* sync_bar_{nullptr};          ///< Stage 5: Sync + Mix
    QProgressBar* mux_bar_{nullptr};           ///< Stage 6: Mux (+ cleanup)

    /// Map stage name string → corresponding QProgressBar pointer.
    QProgressBar* get_bar_for_stage(const QString& stage);
};

} // namespace vd
