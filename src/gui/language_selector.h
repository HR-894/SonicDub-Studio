/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  language_selector.h — Source/Target Language Picker                       ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * SUPPORTED LANGUAGES:
 *   Auto Detect, English, Hindi, Spanish, French, German,
 *   Japanese, Korean, Chinese, Arabic, Portuguese, Russian,
 *   Italian, Tamil, Telugu, Bengali, Marathi
 *
 * UI LAYOUT:
 *   🌐 Source Language
 *   [✓] Auto Detect
 *   [English ▼]  (disabled when auto-detect is checked)
 *
 *   🎯 Target Language
 *   [Hindi ▼]    (always active)
 */

#pragma once

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>

namespace vd {

class LanguageSelector : public QWidget {
    Q_OBJECT

public:
    explicit LanguageSelector(QWidget* parent = nullptr);

    /// Get selected source language ("auto" if auto-detect is checked).
    QString source_language() const;

    /// Get selected target language ISO code (e.g. "hi").
    QString target_language() const;

private:
    QComboBox* source_cb_{nullptr};
    QComboBox* target_cb_{nullptr};
    QCheckBox* auto_detect_{nullptr};
};

} // namespace vd
