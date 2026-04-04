#include "gui/log_viewer.h"
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QScrollBar>

namespace vd {

LogViewer::LogViewer(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    text_edit_ = new QPlainTextEdit(this);
    text_edit_->setReadOnly(true);
    text_edit_->setFont(QFont("Consolas", 9));
    text_edit_->setStyleSheet(R"(
        QPlainTextEdit {
            background-color: #11111b;
            color: #cdd6f4;
            border: 1px solid #313244;
            border-radius: 4px;
        }
    )");

    layout->addWidget(text_edit_);
}

void LogViewer::append_log(const QString& msg, int level) {
    QString html;
    switch (level) {
        case 0: // trace
        case 1: // debug
            html = QString("<span style='color:#6c7086;'>%1</span>").arg(msg.toHtmlEscaped()); break;
        case 2: // info
            html = QString("<span style='color:#a6e3a1;'>%1</span>").arg(msg.toHtmlEscaped()); break;
        case 3: // warn
            html = QString("<span style='color:#f9e2af;'>%1</span>").arg(msg.toHtmlEscaped()); break;
        case 4: // err
        case 5: // critical
            html = QString("<span style='color:#f38ba8;'><b>%1</b></span>").arg(msg.toHtmlEscaped()); break;
        default:
            html = msg.toHtmlEscaped();
    }

    text_edit_->appendHtml(html);
    text_edit_->verticalScrollBar()->setValue(text_edit_->verticalScrollBar()->maximum());
}

void LogViewer::clear() {
    text_edit_->clear();
}

} // namespace vd
