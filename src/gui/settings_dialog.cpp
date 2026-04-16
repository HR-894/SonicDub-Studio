#include "gui/settings_dialog.h"
#include "core/config_manager.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <nlohmann/json.hpp>

namespace vd {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("⚙ SonicDubStudio Settings");
    setMinimumWidth(520);
    setWindowModality(Qt::ApplicationModal);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(12);

    // ═══════════════════════════════════════
    // GROUP 1: API Keys
    // ═══════════════════════════════════════
    auto* keys_group  = new QGroupBox("🔑 API Keys", this);
    auto* keys_form   = new QFormLayout(keys_group);
    keys_form->setLabelAlignment(Qt::AlignRight);

    gemini_key_ = new QLineEdit;
    google_key_ = new QLineEdit;
    eleven_key_ = new QLineEdit;
    deepl_key_  = new QLineEdit;

    gemini_key_->setPlaceholderText("AIza...");
    google_key_->setPlaceholderText("Google Cloud API Key");
    eleven_key_->setPlaceholderText("ElevenLabs API Key");
    deepl_key_->setPlaceholderText("DeepL API Key");

    for (auto* le : {gemini_key_, google_key_, eleven_key_, deepl_key_}) {
        le->setEchoMode(QLineEdit::PasswordEchoOnEdit);
        le->setMinimumWidth(300);
    }

    keys_form->addRow("Gemini API Key:",  gemini_key_);
    keys_form->addRow("Google Cloud:",    google_key_);
    keys_form->addRow("ElevenLabs:",      eleven_key_);
    keys_form->addRow("DeepL:",           deepl_key_);
    main_layout->addWidget(keys_group);

    // ═══════════════════════════════════════
    // GROUP 2: AI Backends
    // ═══════════════════════════════════════
    auto* backend_group = new QGroupBox("🤖 AI Backends", this);
    auto* backend_form  = new QFormLayout(backend_group);
    backend_form->setLabelAlignment(Qt::AlignRight);

    asr_backend_ = new QComboBox;
    asr_backend_->addItem("VibeVoice-ASR (Recommended — 60min, Who+When+What)", "vibevoice");
    asr_backend_->addItem("Whisper (Local, no internet required)",               "whisper");

    tts_backend_ = new QComboBox;
    tts_backend_->addItem("Gemini TTS (Best quality, API key required)",         "gemini");
    tts_backend_->addItem("VibeVoice-Realtime (Local, offline, speaker-consistent)", "vibevoice_local");
    tts_backend_->addItem("Google Cloud TTS",                                    "google");
    tts_backend_->addItem("ElevenLabs",                                          "elevenlabs");
    tts_backend_->addItem("Edge TTS (Free, no API key)",                         "edge");
    tts_backend_->addItem("XTTS v2 (Local voice cloning)",                       "xttsv2_local");

    trans_backend_ = new QComboBox;
    trans_backend_->addItem("Google Translate",  "google");
    trans_backend_->addItem("DeepL",             "deepl");
    trans_backend_->addItem("LibreTranslate",     "libretranslate");

    mix_mode_ = new QComboBox;
    mix_mode_->addItem("Overlay (keep original audio at 10% volume)", "overlay");
    mix_mode_->addItem("Replace (remove original audio completely)",   "replace");

    backend_form->addRow("ASR Backend:",          asr_backend_);
    backend_form->addRow("TTS Backend:",          tts_backend_);
    backend_form->addRow("Translation Backend:",   trans_backend_);
    backend_form->addRow("Audio Mix Mode:",        mix_mode_);
    main_layout->addWidget(backend_group);

    // ═══════════════════════════════════════
    // GROUP 3: VibeVoice-Specific Settings
    // ═══════════════════════════════════════
    auto* vv_group = new QGroupBox("🎙️ VibeVoice Settings", this);
    auto* vv_form  = new QFormLayout(vv_group);
    vv_form->setLabelAlignment(Qt::AlignRight);

    vibevoice_gpu_ = new QCheckBox("Use GPU (CUDA) — requires NVIDIA GPU");
    vibevoice_gpu_->setChecked(true);

    hotwords_field_ = new QLineEdit;
    hotwords_field_->setPlaceholderText("Raj, Simba, OpenAI, mitochondria ...");
    hotwords_field_->setToolTip(
        "Comma-separated custom terms to improve transcription accuracy.\n"
        "Useful for character names, brand names, technical terms, etc."
    );

    auto* hotwords_label = new QLabel(
        "<small style='color: #9399b2;'>"
        "Comma-separated terms injected into ASR for better accuracy.</small>"
    );
    hotwords_label->setTextFormat(Qt::RichText);

    vv_form->addRow("Use GPU:", vibevoice_gpu_);
    vv_form->addRow("Hotwords:", hotwords_field_);
    vv_form->addRow("", hotwords_label);
    main_layout->addWidget(vv_group);

    // ═══════════════════════════════════════
    // Buttons
    // ═══════════════════════════════════════
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this
    );
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save_settings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main_layout->addWidget(buttons);

    // ═══════════════════════════════════════
    // Load Current Config
    // ═══════════════════════════════════════
    auto& cfg = ConfigManager::instance();

    gemini_key_->setText(QString::fromStdString(cfg.get_api_key("gemini")));
    google_key_->setText(QString::fromStdString(cfg.get_api_key("google_translate")));
    eleven_key_->setText(QString::fromStdString(cfg.get_api_key("elevenlabs")));
    deepl_key_->setText(QString::fromStdString(cfg.get_api_key("deepl")));

    // Set ASR backend combo
    QString cur_asr = QString::fromStdString(cfg.get_asr_backend());
    for (int i = 0; i < asr_backend_->count(); ++i) {
        if (asr_backend_->itemData(i).toString() == cur_asr) {
            asr_backend_->setCurrentIndex(i);
            break;
        }
    }

    // Set TTS backend combo
    QString cur_tts = QString::fromStdString(cfg.get_tts_backend());
    for (int i = 0; i < tts_backend_->count(); ++i) {
        if (tts_backend_->itemData(i).toString() == cur_tts) {
            tts_backend_->setCurrentIndex(i);
            break;
        }
    }

    trans_backend_->setCurrentIndex(
        trans_backend_->findData(QString::fromStdString(cfg.get_translation_backend()))
    );
    mix_mode_->setCurrentIndex(
        mix_mode_->findData(QString::fromStdString(cfg.get_audio_mix_mode()))
    );

    vibevoice_gpu_->setChecked(cfg.get_vibevoice_use_gpu());

    // Load hotwords → join as comma-separated string
    auto hotwords = cfg.get_vibevoice_hotwords();
    QString hw_str;
    for (size_t i = 0; i < hotwords.size(); ++i) {
        if (i > 0) hw_str += ", ";
        hw_str += QString::fromStdString(hotwords[i]);
    }
    hotwords_field_->setText(hw_str);
}

void SettingsDialog::save_settings() {
    auto& cfg = ConfigManager::instance();

    cfg.set_api_key("gemini",           gemini_key_->text().toStdString());
    cfg.set_api_key("google_translate", google_key_->text().toStdString());
    cfg.set_api_key("google_tts",       google_key_->text().toStdString());
    cfg.set_api_key("elevenlabs",       eleven_key_->text().toStdString());
    cfg.set_api_key("deepl",            deepl_key_->text().toStdString());

    cfg.set("active_asr_backend",         asr_backend_->currentData().toString().toStdString());
    cfg.set("active_tts_backend",         tts_backend_->currentData().toString().toStdString());
    cfg.set("active_translation_backend", trans_backend_->currentData().toString().toStdString());
    cfg.set("audio_mix_mode",             mix_mode_->currentData().toString().toStdString());

    cfg.set("vibevoice_use_gpu", vibevoice_gpu_->isChecked());

    // Parse hotwords: split by comma, trim whitespace
    auto hotwords_raw = hotwords_field_->text();
    auto parts        = hotwords_raw.split(",", Qt::SkipEmptyParts);
    nlohmann::json hotwords_json = nlohmann::json::array();
    for (const auto& p : parts) {
        std::string word = p.trimmed().toStdString();
        if (!word.empty()) hotwords_json.push_back(word);
    }
    cfg.set("vibevoice_hotwords", hotwords_json);

    cfg.save();
    accept();
}

} // namespace vd
