#include "gui/language_selector.h"
#include <QVBoxLayout>
#include <QLabel>

namespace vd {

static const QStringList LANGUAGES = {
    "auto|Auto Detect", "en|English", "hi|Hindi", "es|Spanish",
    "fr|French", "de|German", "ja|Japanese", "ko|Korean",
    "zh|Chinese", "ar|Arabic", "pt|Portuguese", "ru|Russian",
    "it|Italian", "ta|Tamil", "te|Telugu", "bn|Bengali", "mr|Marathi"
};

LanguageSelector::LanguageSelector(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(new QLabel("🌐 Source Language"));
    source_cb_ = new QComboBox(this);
    auto_detect_ = new QCheckBox("Auto Detect", this);
    auto_detect_->setChecked(true);

    for (auto& lang : LANGUAGES) {
        auto parts = lang.split('|');
        source_cb_->addItem(parts[1], parts[0]);
    }
    source_cb_->setEnabled(false);

    connect(auto_detect_, &QCheckBox::toggled,
            source_cb_, [this](bool checked){ source_cb_->setEnabled(!checked); });

    layout->addWidget(auto_detect_);
    layout->addWidget(source_cb_);

    layout->addWidget(new QLabel("🎯 Target Language"));
    target_cb_ = new QComboBox(this);
    for (auto& lang : LANGUAGES) {
        if (lang.startsWith("auto")) continue;
        auto parts = lang.split('|');
        target_cb_->addItem(parts[1], parts[0]);
    }
    target_cb_->setCurrentText("Hindi");

    layout->addWidget(target_cb_);
    layout->addStretch();
}

QString LanguageSelector::source_language() const {
    return auto_detect_->isChecked() ? "auto" : source_cb_->currentData().toString();
}

QString LanguageSelector::target_language() const {
    return target_cb_->currentData().toString();
}

} // namespace vd
