#include "gui/progress_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

namespace vd {

ProgressPanel::ProgressPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto add_row = [&](const QString& label, QProgressBar*& bar) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label);
        lbl->setMinimumWidth(120);
        bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(false);
        bar->setStyleSheet(R"(
            QProgressBar { border: 1px solid #45475a; border-radius: 4px; background: #1e1e2e; }
            QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }
        )");
        row->addWidget(lbl);
        row->addWidget(bar);
        layout->addLayout(row);
    };

    add_row("1. Extract Audio",   extract_bar_);
    add_row("2. Transcribe",      transcribe_bar_);
    add_row("3. Translate",       translate_bar_);
    add_row("4. Synthesize TTS",  tts_bar_);
    add_row("5. Sync Timing",     sync_bar_);
    add_row("6. Mux Video",       mux_bar_);

    layout->addStretch();
}

void ProgressPanel::reset() {
    for (auto* bar : {extract_bar_, transcribe_bar_, translate_bar_,
                      tts_bar_, sync_bar_, mux_bar_}) {
        bar->setValue(0);
        bar->setStyleSheet(R"(
            QProgressBar { border: 1px solid #45475a; border-radius: 4px; background: #1e1e2e; }
            QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }
        )");
    }
}

QProgressBar* ProgressPanel::get_bar_for_stage(const QString& stage) {
    if (stage == "extraction") return extract_bar_;
    if (stage == "transcription") return transcribe_bar_;
    if (stage == "translation") return translate_bar_;
    if (stage == "tts") return tts_bar_;
    if (stage == "sync" || stage == "mixing") return sync_bar_;
    if (stage == "muxing") return mux_bar_;
    return nullptr;
}

void ProgressPanel::update_stage(const QString& stage, int /*progress_overall*/) {
    if (stage == "done") {
        for (auto* bar : {extract_bar_, transcribe_bar_, translate_bar_,
                          tts_bar_, sync_bar_, mux_bar_}) {
            bar->setValue(100);
            bar->setStyleSheet(R"(
                QProgressBar { border: 1px solid #45475a; border-radius: 4px; background: #1e1e2e; }
                QProgressBar::chunk { background-color: #a6e3a1; border-radius: 3px; }
            )");
        }
        return;
    }

    if (stage == "failed") {
        auto* bar = get_bar_for_stage(stage);
        if (bar) {
            bar->setStyleSheet(R"(
                QProgressBar { border: 1px solid #45475a; border-radius: 4px; background: #1e1e2e; }
                QProgressBar::chunk { background-color: #f38ba8; border-radius: 3px; }
            )");
        }
        return;
    }

    auto* current_bar = get_bar_for_stage(stage);
    if (!current_bar) return;

    // Fast-forward previous stages to 100%
    bool past = true;
    for (auto* bar : {extract_bar_, transcribe_bar_, translate_bar_,
                      tts_bar_, sync_bar_, mux_bar_}) {
        if (bar == current_bar) {
            past = false;
            bar->setMaximum(0); // Indeterminate animated state
        } else if (past) {
            bar->setMaximum(100);
            bar->setValue(100);
        } else {
            bar->setMaximum(100);
            bar->setValue(0);
        }
    }
}

} // namespace vd
