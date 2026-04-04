/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  settings_dialog.h — API Key & Backend Configuration Dialog               ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * DIALOG LAYOUT:
 *   ┌─ Settings ──────────────────────────────────┐
 *   │                                              │
 *   │  Gemini API Key:       [••••••••••••••••]    │
 *   │  Google Cloud Key:     [••••••••••••••••]    │
 *   │  ElevenLabs Key:       [••••••••••••••••]    │
 *   │  DeepL Key:            [••••••••••••••••]    │
 *   │                                              │
 *   │  TTS Backend:          [gemini        ▼]     │
 *   │  Translation Backend:  [google        ▼]     │
 *   │  Audio Mix Mode:       [overlay       ▼]     │
 *   │                                              │
 *   │            [ Save ]     [ Cancel ]           │
 *   └──────────────────────────────────────────────┘
 *
 * BEHAVIOR:
 *   - On open: loads current values from ConfigManager
 *   - API keys shown with password-on-edit masking
 *   - On Save: writes to ConfigManager + persists to config.json
 */

#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;

namespace vd {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private Q_SLOTS:
    void save_settings();   ///< Write values to ConfigManager and close

private:
    // ─── API Key Fields ───────────────────────────────────────────────────
    QLineEdit* gemini_key_{nullptr};
    QLineEdit* google_key_{nullptr};
    QLineEdit* eleven_key_{nullptr};
    QLineEdit* deepl_key_{nullptr};

    // ─── Backend Selection ────────────────────────────────────────────────
    QComboBox* tts_backend_{nullptr};
    QComboBox* trans_backend_{nullptr};
    QComboBox* mix_mode_{nullptr};
};

} // namespace vd
