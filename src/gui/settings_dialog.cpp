#include "gui/settings_dialog.h"
#include "core/config_manager.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>

namespace vd {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(450);

    auto* layout = new QVBoxLayout(this);
    auto* form   = new QFormLayout;

    gemini_key_ = new QLineEdit;
    google_key_ = new QLineEdit;
    eleven_key_ = new QLineEdit;
    deepl_key_  = new QLineEdit;

    for (auto* le : {gemini_key_, google_key_, eleven_key_, deepl_key_}) {
        le->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    }

    tts_backend_ = new QComboBox;
    tts_backend_->addItems({"gemini", "google", "elevenlabs", "edge"});

    trans_backend_ = new QComboBox;
    trans_backend_->addItems({"google", "deepl", "libretranslate"});

    mix_mode_ = new QComboBox;
    mix_mode_->addItems({"overlay", "replace"});

    form->addRow("Gemini API Key:", gemini_key_);
    form->addRow("Google Cloud Key:", google_key_);
    form->addRow("ElevenLabs Key:", eleven_key_);
    form->addRow("DeepL Key:", deepl_key_);
    form->addRow("TTS Backend:", tts_backend_);
    form->addRow("Translation Backend:", trans_backend_);
    form->addRow("Audio Mix Mode:", mix_mode_);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save_settings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Load current
    auto& cfg = ConfigManager::instance();
    gemini_key_->setText(QString::fromStdString(cfg.get_api_key("gemini")));
    google_key_->setText(QString::fromStdString(cfg.get_api_key("google_translate"))); // simplification
    eleven_key_->setText(QString::fromStdString(cfg.get_api_key("elevenlabs")));
    deepl_key_->setText(QString::fromStdString(cfg.get_api_key("deepl")));

    tts_backend_->setCurrentText(QString::fromStdString(cfg.get_tts_backend()));
    trans_backend_->setCurrentText(QString::fromStdString(cfg.get_translation_backend()));
    mix_mode_->setCurrentText(QString::fromStdString(cfg.get_audio_mix_mode()));
}

void SettingsDialog::save_settings() {
    auto& cfg = ConfigManager::instance();
    auto j = cfg.get_json();

    j["api_keys"]["gemini"]           = gemini_key_->text().toStdString();
    j["api_keys"]["google_translate"] = google_key_->text().toStdString();
    j["api_keys"]["google_tts"]       = google_key_->text().toStdString();
    j["api_keys"]["elevenlabs"]       = eleven_key_->text().toStdString();
    j["api_keys"]["deepl"]            = deepl_key_->text().toStdString();

    j["active_tts_backend"]         = tts_backend_->currentText().toStdString();
    j["active_translation_backend"] = trans_backend_->currentText().toStdString();
    j["audio_mix_mode"]             = mix_mode_->currentText().toStdString();

    // Use internal set and save logic for actual config manager
    cfg.set("api_keys", j["api_keys"]);
    cfg.set("active_tts_backend", j["active_tts_backend"]);
    cfg.set("active_translation_backend", j["active_translation_backend"]);
    cfg.set("audio_mix_mode", j["audio_mix_mode"]);

    cfg.save();
    accept();
}

} // namespace vd
